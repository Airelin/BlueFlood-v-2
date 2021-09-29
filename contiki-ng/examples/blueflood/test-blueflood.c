/*------------------------ Includes ---------------------------------*/

#include <inttypes.h>
#include <string.h>
#include <random.h>
#include "contiki.h"
#include "dev/leds.h"
#include "nrf-radio-driver.h"
#include "watchdog.h"
#include "uuids.h"
#include "ble-beacon-header.h"
#include "testbed.h"
#include "blueflood.h"
/*-------------------------------------------------------------------*/

/*
 * NTX is should be defined in the project-conf.h.
 */
#ifndef NTX
#error "Define the number of transmissions!"
#else
#define ROUND_LEN ((NTX))
#define ROUND_RX_LEN (2*TESTBED_DIAMETER+NTX)
#endif /* NTX */

/*
* Round definitions
*/
#ifndef ROUND_PERIOD_CONF_US
#error "Define round period!"
#endif
#define ROUND_LEN_MAX (ROUND_RX_LEN+ROUND_LEN)
#define ROUND_PERIOD_CONF_RTICKS US_TO_RTIMERTICKS(ROUND_PERIOD_CONF_US)
#define ROUND_PERIOD (ROUND_PERIOD_CONF_RTICKS - ROUND_LEN_MAX * SLOT_LEN)
/*-------------------------------------------------------------------*/
#define LOG_STATE_SIZE (ROUND_LEN_MAX)
/*-------------------------------------------------------------------*/
#define IBEACON_SIZE  (sizeof(ble_beacon_t))
#define BLUETOOTH_BEACON_PDU(S) (8+(S))
#define PACKET_AIR_TIME_MIN (PACKET_AIR_TIME(BLUETOOTH_BEACON_PDU(IBEACON_SIZE),RADIO_MODE_CONF))
#define PAYLOAD_AIR_TIME_MIN (PACKET_AIR_TIME(IBEACON_SIZE,RADIO_MODE_CONF))
#define SLOT_LEN (RX_SLOT_LEN+2*GUARD_TIME_SHORT)
#define SLOT_LEN_NOTSYNCED (RX_SLOT_LEN+GUARD_TIME)
#define FIRST_SLOT_OFFSET (SLOT_PROCESSING_TIME + GUARD_TIME + ADDRESS_EVENT_T_TX_OFFSET)
/*-------------------------------------------------------------------*/
const uint8_t uuids_array[UUID_LIST_LENGTH][16] = UUID_ARRAY;
const uint32_t testbed_ids[] = TESTBED_IDS;
enum {MSG_TURN_BROADCAST=0xff, MSG_TURN_NONE=0xfe};
/*-------------------------------------------------------------------*/
#define tx_node_id        (TESTBED_IDS[INITATOR_NODE_INDEX])
#define IS_INITIATOR()    (my_id == tx_node_id)
/*-------------------------------------------------------------------*/
#if PRINT_CUSTOM_DEBUG_MSG
static char dbgmsg[256]="", dbgmsg2[256]="";
#endif
static uint8_t my_tx_buffer[255] = {0};
static uint8_t my_rx_buffer[255] = {0};

static ble_beacon_t msg;
static uint64_t corrupt_msg_index = 0;
static uint32_t my_id = 0;
/*-------------------------------------------------------------------*/
PROCESS(tx_process, "TX process");
AUTOSTART_PROCESSES(&tx_process);
/*-------------------------------------------------------------------*/
/*
* Returns the index the device has in the testbed.h
*/
static int get_testbed_index(uint32_t my_id, const uint32_t *testbed_ids, uint8_t testbed_size){
  int i;
  for( i=0; i<testbed_size; i++ ){
    if( my_id == testbed_ids[i] ){
      return i;
    }
  }
  return -1;
}
/*-------------------------------------------------------------------*/
/*
* This function is used to build an ibeacon.
* The resulting ble_beacon can be found on the adress passed in the first argument.
*/
static void init_ibeacon_packet(ble_beacon_t *pkt, const uint8_t* uuid, uint16_t round, uint16_t slot){
#if (RADIO_MODE_CONF == RADIO_MODE_MODE_Ieee802154_250Kbit)
    pkt->radio_len = sizeof(ble_beacon_t) - 1; /* execlude len field */
#else
  pkt->radio_len = sizeof(ble_beacon_t) - 2; /* len + pdu_header */ //length of the rest of the packet

#endif
  pkt->pdu_header = 0x42;     //pdu type: 0x02 ADV_NONCONN_IND, rfu 0, rx 0, tx 1 //2;
  pkt->adv_address_low = MY_ADV_ADDRESS_LOW;
  pkt->adv_address_hi = MY_ADV_ADDRESS_HI;
  memcpy(pkt->uuid, uuid, sizeof(pkt->uuid));
  pkt->round = round;
  pkt->slot = slot;
  pkt->turn = MSG_TURN_NONE;

  #if (PACKET_IBEACON_FORMAT)
  pkt->ad_flags_length = 2;   //2bytes flags
  pkt->ad_flags_type = 1;     //1=flags
  pkt->ad_flags_data = 6;     //(non-connectable, undirected advertising, single-mode device)
  pkt->ad_length = 0x1a;      //26 bytes, the remainder of the packet
  pkt->ad_type = 0xff;        //manufacturer specific
  pkt->company_id = 0x004cU;  //Apple ID
  pkt->beacon_type = 0x1502;  //0x0215U; //proximity ibeacon
  pkt->power = 0;             //RSSI = -60 dBm; Measured Power = 256 – 60 = 196 = 0xC4
  #endif
}
/*-------------------------------------------------------------------*/
/* 
 * Checks if the current time has passed a ref time + offset. Assumes
 * a single overflow and ref time prior to now. 
 */
static uint8_t
check_timer_miss(rtimer_clock_t ref_time, rtimer_clock_t offset, rtimer_clock_t now)
{
  rtimer_clock_t target = ref_time + offset;
  uint8_t now_has_overflowed = now < ref_time;
  uint8_t target_has_overflowed = target < ref_time;

  if(now_has_overflowed == target_has_overflowed) {
    /* Both or none have overflowed, just compare now to the target */
    return target <= now;
  } else {
    /* Either now or target of overflowed.
     * If it is now, then it has passed the target.
     * If it is target, then we haven't reached it yet.
     *  */
    return now_has_overflowed;
  }
}
/*-------------------------------------------------------------------*/
#if !BLUEFLOOD_BUSYWAIT
void rtc_schedule(uint32_t ticks);
#endif
/*-------------------------------------------------------------------*/
/*
*This is the starting point in the main function.
*/
PROCESS_THREAD(tx_process, ev, data)
{
  //counts numbers of not okay received packets (rx_ok == 0)
  static uint8_t failed_rounds = 0;
  static int8_t my_index = -1;
  //Some paket ingredience
  static uint16_t round = 0, pktslot = 0;
  //CRC = Cyclic Redundancy Check (Checksum things)
  static uint16_t rx_ok = 0, rx_crc_failed = 0, rx_none = 0, tx_done=0, 
  berr = 0 /* bit errors per round */, 
  berr_per_pkt_max = 0, berr_per_byte_max = 0;

  // Variables that don't belong to blueflood.c, so they stay here
  static uint32_t rx_ok_total = 0, rx_failed_total = 0, berr_total = 0;

  // RSSI: Received Signal Strength Index
  #if PRINT_RSSI
  static int8_t rx_rssi[LOG_STATE_SIZE]={0};
  #endif
  #if PRINT_TS_DELTA
  static int32_t rx_ts_delta[LOG_STATE_SIZE]={0UL};
  #endif //PRINT_TS_DELTA
  #if PRINT_TX_STATUS
  static char tx_status[LOG_STATE_SIZE+1]={0};
  #endif /* PRINT_TX_STATUS */
  #if PRINT_RADIO
  static rtimer_clock_t ts_radio[LOG_STATE_SIZE]={0};
  #endif //PRINT_RADIO

  //Locale clock
  volatile static rtimer_clock_t tt = 0, t_start_round = 0;
  static uint8_t joined = 0;

  //index for for-loops in print-logging
  int i;
  
  PROCESS_BEGIN();

  // to get the ids needed for the testbed.h
  #if TESTBED==HALLOWORLD_TESTBED
    my_radio_init(&my_id, my_tx_buffer);
    my_index = get_testbed_index(my_id, testbed_ids, TESTBED_SIZE);
    init_ibeacon_packet(&msg, &uuids_array[0][0], round, pktslot);
    //put radio in tx idle mode to send continuous carrier
    #if RADIO_TEST_TX_CARRIER
    my_radio_send(my_tx_buffer, BLE_DEFAULT_CHANNEL);
    #endif
    while(1){
      PRINTF("#@ %s, ID: 0x%lx, master: 0x%lx, tx power: %d dBm, channel %u = %u MHz (%s), idx %d\n", 
      FIRMWARE_TIMESTAMP_STR, my_id, tx_node_id, (int8_t)BLE_DEFAULT_RF_POWER, BLE_DEFAULT_CHANNEL, 2400u+ble_hw_frequency_channels[BLE_DEFAULT_CHANNEL], OVERRIDE_BLE_CHANNEL_37 ? "not std" : "std", my_index);
      watchdog_periodic();
    }
  #endif

  testbed_cofigure_pins();
  testbed_clear_debug_pins();
  my_radio_init(&my_id, my_tx_buffer);
  // Get my position in the testbed.h
  my_index = get_testbed_index(my_id, testbed_ids, TESTBED_SIZE);
  init_ibeacon_packet(&msg, &uuids_array[0][0], round, pktslot);
  watchdog_periodic();

// This is basically also guardtime
  if(IS_INITIATOR()){
    nrf_delay_ms(2000);
  } else{
    nrf_delay_ms(1000);
  }

  t_start_round = RTIMER_NOW() + FIRST_SLOT_OFFSET;
  joined = 0;

  while(1)
  {
    // reset booleans
    rx_ok = 0, rx_crc_failed = 0, rx_none = 0; tx_done=0; berr = 0; 
    // reset a lot of variables for the new round
    berr_per_pkt_max = 0, berr_per_byte_max = 0; corrupt_msg_index = 0;

    #if PRINT_TX_STATUS
    tx_status[0] = ':';
    #endif /* PRINT_TX_STATUS */
    
    nrf_gpio_pin_toggle(ROUND_INDICATOR_PIN);

    /* Pass the arrays for logging purposes */
    blueflood_set_tx_status(tx_status);
    blueflood_set_rx_ts_delta_status(rx_ts_delta);
    blueflood_set_rx_rssi(rx_rssi);
    blueflood_set_ts_radio(ts_radio);

    /* Start the next flood */
    blueflood_set_t_start_round(t_start_round);
    blueflood_start(IS_INITIATOR(),&msg,msg.radio_len,4U,joined,round);

    /* Get informations from the synchronization component */
    joined = blueflood_get_joined();
    t_start_round = blueflood_get_t_start_round();

    /* Get some extra logging data */
    round = blueflood_get_round();
    rx_ok = blueflood_get_rx_ok();
    rx_crc_failed = blueflood_get_crc_failed();
    rx_none = blueflood_get_rx_none();

    /* Turn off the radio as the round is done */
    blueflood_stop();

    /* Show that the round is done by toggeling the LED */
    nrf_gpio_pin_toggle(ROUND_INDICATOR_PIN);
    rx_ok_total += rx_ok;
    rx_failed_total += rx_crc_failed + rx_none;

/*
* Print everything we want to be printed in this section.
* This is the main thing this application provides.
* The round logic moves on in line 345
*/
#if ENABLE_BLUEFLOOD_LOGS
  #if TESTBED_LOG_STYLE
    #if PRINT_RADIO
    PRINTF("{ts-radio-%d} ", round);
    for(i=0; i<sizeof(ts_radio)/sizeof(ts_radio[0]); i++){
      PRINTF("%d, ", ts_radio[i]);
    }
    PRINTF("\n");
    PRINTF("{ts-radio-off} %d", blueflood_get_ts_radio_off());
    PRINTF("\n");
    memset(ts_radio,0,sizeof(ts_radio));
    #endif /* PRINT_RADIO */

    #if PRINT_RX_STATS
    PRINTF("{rx-%d} %u, %u, %u, %u, %lu, %lu, %u, %u, %u, %lu\n", round, rx_ok, rx_crc_failed, rx_none, tx_done, rx_ok_total, rx_ok_total+rx_failed_total, berr_per_byte_max, berr_per_pkt_max, berr /* bit errors per round */, berr_total);
    #endif
    #if PRINT_RSSI
    PRINTF("{rssi-%d} ", round);
    for(i=0; i<sizeof(rx_rssi)/sizeof(rx_rssi[0]); i++){
      PRINTF("%d, ", rx_rssi[i]);
    }
    PRINTF("\n");
    memset(rx_rssi, 111, sizeof(rx_rssi));
    #endif /* PRINT_RSSI */

    #if PRINT_TS_DELTA
    PRINTF("{td-%d} ", round);
    for(i=0; i<sizeof(rx_ts_delta)/sizeof(rx_ts_delta[0]); i++){
      PRINTF("%" PRId32 ", ", rx_ts_delta[i]);
    }
    PRINTF("\n");
    memset(rx_ts_delta, 0, sizeof(rx_ts_delta));
    #endif /* PRINT_TS_DELTA */

    #if PRINT_TX_STATUS
    PRINTF("{tx-%d} %s\n", round, tx_status);
    #endif /* PRINT_TX_STATUS */
    #if PRINT_CUSTOM_DEBUG_MSG
    if(dbgmsg[0]!=0){
      PRINTF("{dg-%d} %s\n", round, dbgmsg);
      dbgmsg[0]=0;
    }
    if(dbgmsg2[0]!=0){
    PRINTF("{dg2-%d} %s\n", round, dbgmsg2);
    dbgmsg2[0]=0;
    }
    #endif /* PRINT_DEBUG_MSG */

  #else /* TESTBED_LOG_STYLE */
    PRINTF("rx_ok %u, crc %u, none %u, tx %u: OK %lu of %lu, berr b%u p%u r%u %lu\n", rx_ok, rx_crc_failed, rx_none, tx_done, rx_ok_total, rx_ok_total+rx_failed_total, berr_per_byte_max, berr_per_pkt_max, berr /* bit errors per round */, berr_total);

    #if PRINT_RSSI
    PRINTF("Rssi: ");
    for(i=0; i<sizeof(rx_rssi)/sizeof(rx_rssi[0]); i++){
      PRINTF("%d, ", rx_rssi[i]);
    }
    PRINTF("dB\n");
    memset(rx_rssi, 111, sizeof(rx_rssi));
    #endif /* PRINT_RSSI */

    #if PRINT_TS_DELTA
    PRINTF("Ts delta: ");
    for(i=0; i<sizeof(rx_ts_delta)/sizeof(rx_ts_delta[0]); i++){
      PRINTF("%" PRId32 ", ", rx_ts_delta[i]);
    }
    PRINTF("ticks\n");
    memset(rx_ts_delta, 0, sizeof(rx_ts_delta));
    #endif /* PRINT_TS_DELTA */
    #if PRINT_TX_STATUS
    PRINTF("Tx status: %s\n", tx_status);
    #endif /* PRINT_TX_STATUS */
  #endif /* TESTBED_LOG_STYLE */

  #if PRINT_TX_STATUS
    memset(tx_status, '.', sizeof(tx_status));
    tx_status[sizeof(tx_status)-1] = '\0';
  #endif /* PRINT_TX_STATUS */

  #if PRINT_NODE_CONFIG
    if(round % 1024 == 0){
      PRINTF("#R %u, ID: 0x%lx %d, master: 0x%lx, tx power: %d dBm, channel %u = %u MHz (%s), msg: %d bytes, mode: %s, @ %s\n", 
              round, my_id, my_index, tx_node_id, (int8_t)BLE_DEFAULT_RF_POWER, BLE_DEFAULT_CHANNEL, 
              2400u+ble_hw_frequency_channels[BLE_DEFAULT_CHANNEL], OVERRIDE_BLE_CHANNEL_37 ? "not std" : "std", sizeof(ble_beacon_t)
              , RADIO_MODE_TO_STR(RADIO_MODE_CONF), FIRMWARE_TIMESTAMP_STR);
    }
  #endif /* PRINT_NODE_CONFIG */
#endif /* ENABLE_BLUEFLOOD_LOGS */

    /* 
    * If the received packet was not okay and we already failed to often,
    * Force resynchronization
    */
    if(rx_ok == 0){
      if(failed_rounds > 10){
        joined = 0;
        #if PRINT_NODE_REJOIN_WARNING
        PRINTF("{fr-%d} Rejoining: failed rounds %d, joined %d\n", (int)round, (int)failed_rounds, (int)joined);
        #endif
      }
      failed_rounds++;
    } else {
      failed_rounds = 0;
      rx_ok = 0;
    }

    /* Prepare round number, packet and buffer for next round */
    round++;
    init_ibeacon_packet(&msg, &uuids_array[0][0], round, pktslot);
    memset(my_rx_buffer, 0, msg.radio_len);

    /* Check if we will be in time */
    rtimer_clock_t now;
    now = RTIMER_NOW();
    #define TIMER_GUARD 16
    uint8_t round_is_late = check_timer_miss(t_start_round, ROUND_PERIOD-TIMER_GUARD, now);
    t_start_round += ROUND_PERIOD;

    /* If our calculations took too long, we skip the next round */
    if(round_is_late){
      #if PRINT_GO_LATE_WARNING
      PRINTF("#!{%d}PRE GO late: %ld\n", round, now - t_start_round);
      #endif /* PRINT_GO_LATE_WARNING */
      round_is_late = check_timer_miss(t_start_round, ROUND_PERIOD-TIMER_GUARD, now);
      t_start_round += ROUND_PERIOD;
    }
    
  /* By default busywait os turned off */
  #if BLUEFLOOD_BUSYWAIT
    /* wait at the end of the round */
    NRF_TIMER0->CC[0] = t_start_round - FIRST_SLOT_OFFSET;;
    while(!NRF_TIMER0->EVENTS_COMPARE[0]){
      watchdog_periodic();
    }
  #else
    rtimer_clock_t tnow = RTIMER_NOW();
    uint32_t rtc_ticks = RTIMER_TO_RTC((t_start_round - tnow))-RTC_GUARD; //save one RTC tick for preprocessing!
    rtc_schedule(rtc_ticks);
    /* Go to sleep sequence: 
     * SEV Set event and WFE wait for event first to consume any previously set event if any, then wait for event to sleep the CPU until an event happens. 
    */
    __SEV();
    __WFE();
    __WFE();
    tt = t_start_round - FIRST_SLOT_OFFSET;
    NRF_TIMER0->EVENTS_COMPARE[0]=0;
    NRF_TIMER0->CC[0] = tt;
    BUSYWAIT_UNTIL_ABS(NRF_TIMER0->EVENTS_COMPARE[0] != 0UL, tt);
#endif
  } /* End of big while(1) */
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
#if !BLUEFLOOD_BUSYWAIT

void rtc_schedule(uint32_t ticks)
{     
  //trick: RTC schedule function will trigger overflow event to mirror that on the GPIO for debugging purposes
  #ifdef RTC_SCHEDULE_PIN
  nrf_gpio_pin_toggle(RTC_SCHEDULE_PIN);
  #endif
  /* Set prescaler so that TICK freq is CLOCK_SECOND */
  NRF_RTC2->PRESCALER = RTC_PRESCALER-1; //fRTC [kHz] = 32.768 / (PRESCALER + 1 )
  NRF_RTC2->TASKS_CLEAR=1;
  NRF_RTC2->CC[1]=ticks;
  /* Enable compare event and compare interrupt */
  NRF_RTC2->EVTENSET      = RTC_EVTENSET_COMPARE1_Msk;;
  NRF_RTC2->INTENSET      = RTC_INTENSET_COMPARE1_Msk;
  /* Enable Interrupt for RTC2 in the core */
  NVIC_SetPriority(RTC2_IRQn, 3);
  NVIC_EnableIRQ(RTC2_IRQn);
  NRF_RTC2->TASKS_START = 1;
  //poll the WDT so it does not fire early. we keep it running though so it wakes us up if something wrong happened...
  watchdog_periodic();
}

/** \brief Function for handling the RTC2 interrupts.
 */
void RTC2_IRQHandler()
{
  if(NRF_RTC2->EVENTS_COMPARE[1] == 1){
    #ifdef RTC_SCHEDULE_PIN
    nrf_gpio_cfg_output(RTC_SCHEDULE_PIN);
    nrf_gpio_pin_toggle(RTC_SCHEDULE_PIN);
    #endif
    NRF_RTC2->EVENTS_COMPARE[1] = 0;
    // Disable COMPARE1 event and COMPARE1 interrupt:
    NRF_RTC2->EVTENCLR      = RTC_EVTENSET_COMPARE1_Msk;
    NRF_RTC2->INTENCLR      = RTC_INTENSET_COMPARE1_Msk;
    //PRINTF("poll\n");
    NRF_RTC2->TASKS_STOP = 1;
    NVIC_DisableIRQ(RTC2_IRQn);
  }
}
#endif