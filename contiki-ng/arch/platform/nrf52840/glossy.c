#include "glossy.h"
#include "node-id.h"
#include "nrf_radio.h"
#include "packetbuf.h"

#include "leds.h"

#include "log.h"
#define LOG_MODULE "nrf-glossy"
#define LOG_LEVEL LOG_LEVEL_DBG

#if LOG_LEVEL >= LOG_LELVEL_DBG
#define PRINTBUFFER() glossy_debug_print_buffer()
#else
#define PRINTBUFFER()
#endif

// --------------------------------------------------------
// Defines
// --------------------------------------------------------

#define GLOSSY_MAX_PACKET_LEN         (GLOSSY_CONF_PAYLOAD_LEN + GLOSSY_MAX_HEADER_LEN)
// 1 Byte
#define GLOSSY_LEN_FIELD              glossy_buffer[0]
// 1 Byte
#define GLOSSY_HEADER_FIELD           glossy_buffer[1]
// Dynamic
#define GLOSSY_DATA_FIELD             &glossy_buffer[2]
// 0 or 1 Byte
#define GLOSSY_RELAY_CNT_FIELD        glossy_buffer[packet_len - 2] 
// 1 Byte
#define GLOSSY_RSSI_FIELD             glossy_buffer[packet_len - 1] 

#define GLOSSY_HEADER_BYTE_MASK       0x0f    /* 4 bits */
#define GLOSSY_HEADER_BYTE_LEN        1
#define GLOSSY_RELAY_CNT_LEN          1
#define FOOTER_LEN                    1

#define TIME_BEFORE_ADDRESS           160     /* microseconds */
#define TIME_BEFORE_LENGTH            192     /* microseconds */

#define LF_TICKS_PER_HF_TICK          (RTIMER_EXT_CONF_HF_CLKSPEED / RTIMER_EXT_CONF_LF_CLKSPEED)


// --------------------------------------------------------
// State
// --------------------------------------------------------

static uint8_t glossy_buffer[GLOSSY_MAX_PACKET_LEN + 1];
static uint8_t *glossy_payload;
static uint8_t initiator,
               sync,
               glossy_payload_len,
               packet_len,
               rx_cnt,
               rx_try_cnt,
               tx_cnt,
               tx_max,
               relay_cnt,
               tx_relay_cnt_last __attribute__((unused)),
               bytes_read __attribute__((unused)),
               n_timeouts __attribute__((unused)),
               t_ref_l_updated,
               header_byte;

static uint16_t flood_cnt,
                flood_cnt_success;

static uint32_t total_rx_success_cnt,
                total_rx_try_cnt,
                t_ref_l;

/* List of possible Glossy states. */
enum glossy_state {
  GLOSSY_STATE_OFF,          /**< Glossy is not executing */
  GLOSSY_STATE_WAITING,      /**< Glossy is waiting for a packet being flooded */
  GLOSSY_STATE_RECEIVING,    /**< Glossy is receiving a packet */
  GLOSSY_STATE_RECEIVED,     /**< Glossy has just finished receiving a packet */
  GLOSSY_STATE_TRANSMITTING, /**< Glossy is transmitting a packet */
  GLOSSY_STATE_TRANSMITTED,  /**< Glossy has just finished transmitting a packet */
  GLOSSY_STATE_ABORTED       /**< Glossy has just aborted a packet reception */
};

static volatile uint8_t state = GLOSSY_STATE_OFF;


/* Time */
static rtimer_clock_t T_slot_h;

static rtimer_ext_clock_t t_ref_lf_ext;
                      

// --------------------------------------------------------
// Debug Functions
// --------------------------------------------------------


void
glossy_debug_print_buffer()
{
  LOG_DBG("glossy_buffer: ");
  for (size_t i = 0; i < sizeof(glossy_buffer); i++)
  {
    LOG_DBG_("%02x", glossy_buffer[i]);
  }
  LOG_DBG_("\n");
}

// --------------------------------------------------------
// Helper Functions
// --------------------------------------------------------

void
disable_other_interrupts()
{
  // TODO: do
}

void
enable_other_interrupts()
{
  // TODO: do
}

void 
generic_config_radio()
{
  nrf_radio_task_trigger(NRF_RADIO_TASK_DISABLE);
  static nrf_radio_packet_conf_t conf = {
    .lflen = 8,
    .s0len = 0,
    .s1len = 0,
    .s1incl = false,
    .cilen = 0,
    .plen = NRF_RADIO_PREAMBLE_LENGTH_32BIT_ZERO,
    .crcinc = false,
    .termlen = 0,
    .maxlen = GLOSSY_MAX_PACKET_LEN,
    .statlen = 0,
    .balen = 0,
    .big_endian = false,
    .whiteen = false
  };
  nrf_radio_packetptr_set(glossy_buffer);
  nrf_radio_frequency_set(5);
  nrf_radio_mode_set(NRF_RADIO_MODE_IEEE802154_250KBIT);
  nrf_radio_shorts_set(
    RADIO_SHORTS_READY_START_Msk |  // auotmatically start tx/rx once ramp up done
    RADIO_SHORTS_END_DISABLE_Msk    // automaticcaly turn radio off once rx/tx done
    );
  nrf_radio_packet_configure(&conf);
  nrf_radio_sfd_set(0x06);          // custom SFD, chosen randomly

  nrf_radio_int_mask_t ints = 
    NRF_RADIO_INT_DISABLED_MASK   |   //radio received, ready to turn around
    NRF_RADIO_INT_FRAMESTART_MASK ;   //radio sent / received preamble field.
  nrf_radio_int_enable(ints);
  nrf_radio_int_disable(~ints);
}

// TX

void
tx_config_radio()
{
  // nothing for now
}

/*
 * Assembles packet for initial transmit.
 * Only call when initiator == true
 */
void
tx_assemble_packet()
{
  packet_len = ((sync) ? GLOSSY_RELAY_CNT_LEN : 0) +
                    glossy_payload_len + FOOTER_LEN +  GLOSSY_HEADER_BYTE_LEN
                    + 1;  // LEN fields itself


  GLOSSY_LEN_FIELD = packet_len - 1;  // exclude len field 
  GLOSSY_HEADER_FIELD = header_byte;
  memcpy(GLOSSY_DATA_FIELD, glossy_payload, glossy_payload_len);
  if (sync) {
    GLOSSY_RELAY_CNT_FIELD = 0;
  }
  GLOSSY_RSSI_FIELD = 0x42;  // TODO: actual value here
}

void
tx_start_radio()
{
  state = GLOSSY_STATE_TRANSMITTING;
  nrf_radio_task_trigger(NRF_RADIO_TASK_TXEN);
  leds_single_on(LEDS_LED1);
  leds_single_off(LEDS_LED2);
}

// RX

void 
rx_config_radio()
{

}

void
rx_start_radio()
{
  state = GLOSSY_STATE_WAITING;
  nrf_radio_task_trigger(NRF_RADIO_TASK_RXEN);
  leds_single_on(LEDS_LED2);
  leds_single_off(LEDS_LED1);
}

void glossy_init()
{
  static bool already_init = false;
  if (!already_init)
  {
    generic_config_radio();

    NVIC_ClearPendingIRQ(RADIO_IRQn);
    nrf_radio_event_clear(NRF_RADIO_EVENT_DISABLED);
    nrf_radio_event_clear(NRF_RADIO_EVENT_FRAMESTART);
    NVIC_EnableIRQ(RADIO_IRQn);
    already_init = true;
  }
}

// --------------------------------------------------------
// Main Functions
// --------------------------------------------------------

void
glossy_start(uint16_t initiator_id,
             uint8_t *payload,
             uint8_t payload_len,
             uint8_t n_tx_max,
             uint8_t with_sync,
             uint8_t dco_cal)
{

  LOG_DBG("glossy start\n");
  leds_single_on(LEDS_LED3);


#if GLOSSY_CONF_SETUPTIME_WITH_SYNC
  uint16_t setup_time_start = rtimer_ext_now_lf_hw();
#endif /* GLOSSY_CONF_SETUPTIME_WITH_SYNC */

  // copy function arguments
  glossy_payload = payload;
  glossy_payload_len = payload_len;
  initiator = (initiator_id == node_id);
  sync = with_sync;
  tx_max = n_tx_max;

  t_ref_l_updated = false;
  relay_cnt = 0;

  memset(glossy_buffer, initiator-1, sizeof(glossy_buffer));
  
  // disable all interrupts that may interfere with Glossy
  disable_other_interrupts();
  glossy_init();
  
  // to stop watchdog stopping program
  // TODO: implement. Actually, I think I can just tell the watchdog no.
#if GLOSSY_USE_BUSY_PROCESS
  // make sure the glossy process is running
  if(!glossy_process_started) {
    process_start(&glossy_process, NULL);
    glossy_process_started = 1;
  }
  process_poll(&glossy_process);
#endif /* GLOSSY_USE_BUSY_PROCESS */
  
  // initialize Glossy variables
  tx_cnt = 0;
  rx_cnt = 0;
  rx_try_cnt = 0;
  header_byte = (((sync << 7) & ~GLOSSY_HEADER_BYTE_MASK) | (GLOSSY_CONF_HEADER_BYTE & GLOSSY_HEADER_BYTE_MASK));

  if (initiator)
  {
    
    tx_assemble_packet();

    // LOG_DBG("Initial, packet_len_tmp: %d:\n", packet_len_tmp);
    // PRINTBUFFER();

    state = GLOSSY_STATE_RECEIVED;
  } else {
    // receiver: set Glossy state
    state = GLOSSY_STATE_WAITING;
  }

  // the reference time has not been updated yet
  t_ref_l_updated = 0;

  if(initiator) {
    tx_config_radio();

#if GLOSSY_CONF_SETUPTIME_WITH_SYNC
    // busy wait for the setup time to pass
    if(sync) {
      //GLOSSY_STOPPED;
      while((uint16_t)(rtimer_ext_now_lf_hw() - setup_time_start) < GLOSSY_SYNC_SETUP_TICKS);
      //GLOSSY_STARTED;
    }
#endif /* GLOSSY_CONF_SETUPTIME_WITH_SYNC */

    // start the first transmission
    tx_start_radio();
    // schedule the initiator timeout
    // TODO
    // if((!sync) || T_slot_h) {
    //   n_timeouts = 0;
    //   schedule_initiator_timeout();
    // }
  } else {
    rx_config_radio();
    // turn on the radio (RX mode)
    rx_start_radio();
  }
}
/*---------------------------------------------------------------------------*/
uint8_t
glossy_stop(void)
{
  LOG_DBG("glossy stopped\n");

  // stop the initiator timeout, in case it is still active
  // TODO
  // stop_initiator_timeout();
  // turn off the radio
  state = GLOSSY_STATE_OFF;

  nrf_radio_int_disable(NRF_RADIO_INT_DISABLED_MASK);
  nrf_radio_task_trigger(NRF_RADIO_TASK_DISABLE);
  leds_single_off(LEDS_LED1);
  leds_single_off(LEDS_LED2);
  leds_single_off(LEDS_LED3);
  // NVIC_DisableIRQ(RADIO_IRQn);
  // NVIC_ClearPendingIRQ(RADIO_IRQn);

  // flush radio buffers
  // not needed, i think
  // radio_flush_rx();
  // radio_flush_tx();

  // re-enable non Glossy-related interrupts
  enable_other_interrupts();

  // stats
  if(!initiator) {
    if(rx_try_cnt) {
      flood_cnt++;
    }
    if(rx_cnt) {
      flood_cnt_success++;
    }
  }
  total_rx_success_cnt += rx_cnt;
  total_rx_try_cnt += rx_try_cnt;
  

  // return the number of times the packet has been received
  return rx_cnt;
}


void
glossy_radio_disabled()
{
  if (state == GLOSSY_STATE_WAITING) {
    // packet received
    if (nrf_radio_crc_status_check()) {
      // and it is valid
      packet_len = GLOSSY_LEN_FIELD + 1;  //+1 for len field
      LOG_DBG("received stuff %d :\n", GLOSSY_RELAY_CNT_FIELD);
      // PRINTBUFFER();
      if (sync) {
        relay_cnt = GLOSSY_RELAY_CNT_FIELD + 1;
        GLOSSY_RELAY_CNT_FIELD = relay_cnt;
      }
      tx_start_radio();
    } else {
      leds_single_toggle(LEDS_LED4);
    }
  } else if (state == GLOSSY_STATE_TRANSMITTING) {
    // packet received
    // we just send a packet
    LOG_DBG("sent stuff\n");
    rx_start_radio();
  } else {
    LOG_WARN("Unvorseen state %d\n", state);
  }
}

void
update_t_ref()
{
  rtimer_ext_clock_t now_ext = rtimer_ext_now_lf();
  uint32_t now = now_ext;

  uint64_t time_passed = relay_cnt * T_slot_h / LF_TICKS_PER_HF_TICK;

  t_ref_l = now - time_passed;
  t_ref_lf_ext = now_ext - time_passed;

  t_ref_l_updated = true;
}

void
glossy_radio_address()
{
  static rtimer_clock_t rx_time, tx_time;

  if (state == GLOSSY_STATE_WAITING) {
    rx_time = RTIMER_NOW();
    // T_slot_h = rx_time - tx_time;
  } else if (state == GLOSSY_STATE_TRANSMITTING) {
    if (relay_cnt >= 1) {
      tx_time = RTIMER_NOW();
      T_slot_h = tx_time - rx_time;
      update_t_ref();
    }
  } else {
    LOG_WARN("Unvorseen state %d\n", state);
  }
  LOG_DBG("Slot time: %ld\n", T_slot_h);
}

void
RADIO_IRQHandler()
{
  // NVIC_ClearPendingIRQ(RADIO_IRQn);
  //LOG_DBG("state: %d\n", state);
  if (nrf_radio_event_check(NRF_RADIO_EVENT_FRAMESTART)) {
    nrf_radio_event_clear(NRF_RADIO_EVENT_FRAMESTART);
    glossy_radio_address();
  }
  if (nrf_radio_event_check(NRF_RADIO_EVENT_DISABLED)) {
    nrf_radio_event_clear(NRF_RADIO_EVENT_DISABLED);
    glossy_radio_disabled();
    leds_single_off(LEDS_LED1);
    leds_single_off(LEDS_LED2);
  }
}

// TODO: Timeout

// Getters for various stuff

/*---------------------------------------------------------------------------*/
uint8_t
glossy_is_active(void)
{
  return true;
}
/*---------------------------------------------------------------------------*/
uint8_t
glossy_get_rx_cnt(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
uint8_t
glossy_get_n_tx(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
uint8_t
glossy_get_payload_len(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
uint8_t
glossy_is_t_ref_updated(void)
{
  return t_ref_l_updated;
}
/*---------------------------------------------------------------------------*/
rtimer_ext_clock_t
glossy_get_t_ref(void)
{
  return t_ref_lf_ext;
}
/*---------------------------------------------------------------------------*/
rtimer_ext_clock_t
glossy_get_t_ref_lf(void)
{
  // /* sync HF and LF clocks */
  // rtimer_ext_clock_t hf_now, lf_now;
  // rtimer_ext_now(&hf_now, &lf_now);
  // lf_now = lf_now - 
  //          (uint32_t)(hf_now - g.t_ref) / (uint32_t)RTIMER_EXT_HF_LF_RATIO;
  // return lf_now;
  return 0;
}
/*---------------------------------------------------------------------------*/
#if GLOSSY_CONF_COLLECT_STATS
uint8_t
glossy_get_rx_try_cnt(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
uint8_t
glossy_get_n_crc_ok(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
uint8_t
glossy_get_last_flood_n_rx_fail(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
int8_t
glossy_get_snr(void)
{
  return 0;
  // /* RSSI values are only valid if at least one packet was received */
  // if(g.n_rx == 0 || g.stats.last_flood_rssi_sum == 0 ||
  //    g.stats.last_flood_rssi_noise == 0) {
  //   return 0;
  // }
  // return (int8_t)((g.stats.last_flood_rssi_sum / (int16_t)g.n_rx) -
  //                 g.stats.last_flood_rssi_noise);
}
/*---------------------------------------------------------------------------*/
int8_t
glossy_get_rssi(void)
{
  return 0;
  // /* RSSI values are only valid if at least one packet was received */
  // if(g.n_rx == 0 || g.stats.last_flood_rssi_sum == 0) {
  //   return 0;
  // }
  // return (int8_t)(g.stats.last_flood_rssi_sum / (int16_t)g.n_rx);
}
/*---------------------------------------------------------------------------*/
uint8_t
glossy_get_relay_cnt(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
uint16_t
glossy_get_per(void)
{
  // if(g.stats.pkt_cnt) {
  //   return 10000 -
  //          (uint16_t)((uint64_t)g.stats.pkt_cnt_crcok * 10000 /
  //          (uint64_t)g.stats.pkt_cnt);
  // }
  return 0;
}
/*---------------------------------------------------------------------------*/
uint16_t
glossy_get_fsr(void)
{
  // if(g.stats.flood_cnt) {
  //   return (uint16_t)((uint64_t)g.stats.flood_cnt_success * 10000 /
  //                     (uint64_t)g.stats.flood_cnt);
  // }
  return 10000;
}
/*---------------------------------------------------------------------------*/
uint32_t
glossy_get_n_pkts(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
uint32_t
glossy_get_n_pkts_crcok(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
uint16_t
glossy_get_n_errors(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
uint32_t
glossy_get_flood_duration(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
uint32_t
glossy_get_t_to_first_rx(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
int8_t
glossy_get_noise_floor(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
uint16_t glossy_get_header(void)
{
  return 0; //( (g.header.pkt_type << 8) | (g.header.relay_cnt) );
}
/*---------------------------------------------------------------------------*/
int8_t glossy_get_sync_mode(void)
{
  return 0; // WITH_SYNC();
}
/*---------------------------------------------------------------------------*/
void
glossy_reset_stats(void)
{
  // g.stats.pkt_cnt = 0;
  // g.stats.pkt_cnt_crcok = 0;
  // g.stats.flood_cnt = 0;
  // g.stats.flood_cnt_success = 0;
  // g.stats.error_cnt = 0;
}
#endif /* GLOSSY_CONF_COLLECT_STATS */