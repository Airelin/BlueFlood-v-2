/*------------------------------------------------------------------*
*                                                                   *
* This is the flooding mechanism of BlueFlood v.2                   *
*                                                                   *
*-------------------------------------------------------------------*/

#include "blueflood.h"
#include "ble-beacon-header.h"
#include "nrf-radio-driver.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include <string.h>

/*-------------------------------------------------------------------*/
/*------------------------ Makros and defines -----------------------*/
/*-------------------------------------------------------------------*/

#define ROUND_LEN (tx_max)
#define ROUND_RX_LEN (2*TESTBED_DIAMETER+ROUND_LEN)

/*
* It's all about the packets
*/
#define IBEACON_SIZE  (sizeof(ble_beacon_t))
#define BLUETOOTH_BEACON_PDU(S) (8+(S))
#define PACKET_AIR_TIME_MIN (PACKET_AIR_TIME(BLUETOOTH_BEACON_PDU(IBEACON_SIZE),RADIO_MODE_CONF))
#define PAYLOAD_AIR_TIME_MIN (PACKET_AIR_TIME(IBEACON_SIZE,RADIO_MODE_CONF))

/*
* Definitions regarding the rounds
*/
#ifndef ROUND_PERIOD_CONF_US
#error "Define round period!"
#endif
#define ROUND_LEN_MAX (ROUND_RX_LEN+ROUND_LEN)
#define ROUND_PERIOD_CONF_RTICKS US_TO_RTIMERTICKS(ROUND_PERIOD_CONF_US)
#define ROUND_PERIOD (ROUND_PERIOD_CONF_RTICKS - ROUND_LEN_MAX * SLOT_LEN)

/*
* Makros for slot-stuff
*/
#define RX_SLOT_LEN (SLOT_PROCESSING_TIME+TX_CHAIN_DELAY+US_TO_RTIMERTICKS(MY_RADIO_RAMPUP_TIME_US) + PACKET_AIR_TIME_MIN)
#define SLOT_LEN (RX_SLOT_LEN+2*GUARD_TIME_SHORT)

#define ROUND_LEN_RULE (((!initiator) && synced && (slot < sync_slot + ROUND_LEN)) || ((initiator || !synced) && (slot < ROUND_RX_LEN)) )

#define FIRST_SLOT_OFFSET (SLOT_PROCESSING_TIME + GUARD_TIME + ADDRESS_EVENT_T_TX_OFFSET)
#define TX_TIME_ABS (tt - ADDRESS_EVENT_T_TX_OFFSET + ARTIFICIAL_TX_OFFSET)

/*
* This is made for logging
*/
#define LOG_STATE_SIZE (ROUND_LEN_MAX)

/*-------------------------------------------------------------------*/
/*------------------------ Enums and typedefs -----------------------*/
/*-------------------------------------------------------------------*/

enum {MSG_TURN_BROADCAST=0xff, MSG_TURN_NONE=0xfe};

/*-------------------------------------------------------------------*/
/*------------------------ Variables --------------------------------*/
/*-------------------------------------------------------------------*/

volatile static rtimer_clock_t tt = 0;
static rtimer_clock_t t_start_round = 0;

static ble_beacon_t msg;

static uint8_t initiator,
               tx_max,
               msg_len,
               rx_latest_ok,
               rx_latest_crc_ok,
               my_turn,
               synced,
               joined,
               got_payload_event, 
               got_address_event, 
               got_end_event, 
               slot_started;

static int join_trial;

static uint16_t round,
                slot,
                sync_slot,
                rx_ok,
                logslot,
                rx_crc_failed, 
                rx_none;

static uint64_t corrupt_msg_index = 0;


static uint32_t guard_time,
                slot_len;


static uint8_t my_rx_buffer[255] = {0};

/*----------------------- Logging -----------------------------------*/

#if PRINT_TX_STATUS
static char *tx_status;
#endif 
#if PRINT_TS_DELTA
static int32_t *rx_ts_delta;
#endif 
#if PRINT_RSSI
static int8_t *rx_rssi;
#endif 
#if PRINT_RADIO
static rtimer_clock_t ts_off;
static rtimer_clock_t *ts_radio;
#endif 

/* This makro is only needed when evaluating the fixed round version*/
#if PRINT_FIXED_ROUND
static uint16_t round_fix = 1;
#endif
/*-------------------------------------------------------------------*/
/*----------------------- Helper functions --------------------------*/
/*-------------------------------------------------------------------*/

/* Checks if the current time has passed a ref time + offset. Assumes
 * a single overflow and ref time prior to now. */
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

/*
* This functions does the transmit part
*/
void
transmit(){
  msg.slot = slot;
  msg.round = round;
  joined = 1;
  guard_time = GUARD_TIME_SHORT;
  msg.turn = MSG_TURN_BROADCAST; //all nodes are allowed to send

  uint8_t *tx_buf = (uint8_t *)&msg;

  // log radio-on timestamps
  #if PRINT_RADIO
  ts_radio[logslot] = RTIMER_NOW();
  #endif

  //schedule transmission for absolute time
  #if PRINT_FIXED_ROUND
  schedule_tx_abs(tx_buf, GET_CHANNEL(round_fix,slot), tt - ADDRESS_EVENT_T_TX_OFFSET + ARTIFICIAL_TX_OFFSET);
  #else 
  schedule_tx_abs(tx_buf, GET_CHANNEL(round,slot), tt - ADDRESS_EVENT_T_TX_OFFSET + ARTIFICIAL_TX_OFFSET);
  #endif

  BUSYWAIT_UNTIL_ABS(NRF_TIMER0->EVENTS_COMPARE[0] != 0U, tt - ADDRESS_EVENT_T_TX_OFFSET + ARTIFICIAL_TX_OFFSET);
  if(!NRF_TIMER0->EVENTS_COMPARE[0]){
    #if PRINT_TX_STATUS
    tx_status[logslot] = 'T';
    #endif /* PRINT_TX_STATUS */
  } else {
    if(slot == 0){
      nrf_gpio_pin_toggle(SLOT1_INDICATOR_PIN);
    }
    BUSYWAIT_UNTIL_ABS(NRF_RADIO->EVENTS_END != 0U, tt + ARTIFICIAL_TX_OFFSET + PACKET_AIR_TIME_MIN);
    if(!NRF_RADIO->EVENTS_END){
    #if PRINT_TX_STATUS
      tx_status[logslot] = 'R';
    #endif /* PRINT_TX_STATUS */
    } else {
      #if PRINT_TX_STATUS
      tx_status[logslot] =  ( msg.turn == MSG_TURN_NONE ) ? 'X' : ( ( msg.turn == MSG_TURN_BROADCAST ) ? 'B' : ( ( msg.turn <= TESTBED_SIZE ) ? HEXC(msg.turn) : 'U' ) );
      #endif /* PRINT_TX_STATUS */
    }
  }
}

/*-------------------------------------------------------------------*/

/*
* This function does the bootstrapping/joining part
*/
void
join(){
  uint8_t channel = 0;
  #if PRINT_FIXED_ROUND
  channel = GET_CHANNEL(round_fix,join_trial%ROUND_RX_LEN);
  #else
  channel = GET_CHANNEL(round,join_trial%ROUND_RX_LEN);
  #endif
  join_trial++;
  // log radio-on timestamps
  #if PRINT_RADIO
  ts_radio[logslot] = RTIMER_NOW();
  #endif
  my_radio_rx(my_rx_buffer, channel);
  rtimer_clock_t to = 2UL*ROUND_PERIOD+random_rand()%ROUND_PERIOD;
  #if (RADIO_MODE_CONF == RADIO_MODE_MODE_Ieee802154_250Kbit)
  BUSYWAIT_UNTIL(NRF_RADIO->EVENTS_FRAMESTART != 0UL, to);
  got_address_event = NRF_RADIO->EVENTS_FRAMESTART;
  #else
  BUSYWAIT_UNTIL(NRF_RADIO->EVENTS_ADDRESS != 0UL, to);
  got_address_event = NRF_RADIO->EVENTS_ADDRESS;
  #endif
  watchdog_periodic();
  slot_started = 1;
}

/*-------------------------------------------------------------------*/

/*
* This is the function where we do rx
*/
void
receive(){
  rtimer_clock_t rx_target_time, rx_tn, rx_tref, rx_toffset, t_proc;
  uint8_t rx_missed_slot = 0;
  /* 
  * Note that: tt = t_start_round + slot * SLOT_LEN
  * Round logic started at rx_tref = t_start_round - FIRST_SLOT_OFFSET
  * We want to start rx at rx_target_time
  * rx_toffset = t_start_round + slot * SLOT_LEN - ADDRESS_EVENT_T_TX_OFFSET - guard_time - (t_start_round - FIRST_SLOT_OFFSET);
  */
  join_trial = 0;
  rx_target_time = tt - ADDRESS_EVENT_T_TX_OFFSET - guard_time;
  rx_tn = RTIMER_NOW();
  rx_tref = t_start_round - FIRST_SLOT_OFFSET;
  rx_toffset = slot * SLOT_LEN + FIRST_SLOT_OFFSET - ADDRESS_EVENT_T_TX_OFFSET - guard_time;
  rx_missed_slot = check_timer_miss(rx_tref, rx_toffset, rx_tn);
  if(!rx_missed_slot){
    // log radio-on timestamps
    #if PRINT_RADIO
    ts_radio[logslot] = RTIMER_NOW();
    #endif
    #if PRINT_FIXED_ROUND
    schedule_rx_abs(my_rx_buffer, GET_CHANNEL(round_fix, slot), rx_target_time);
    #else
    schedule_rx_abs(my_rx_buffer, GET_CHANNEL(round, slot), rx_target_time);
    #endif
    t_proc = RTIMER_NOW() - rx_tn;
    BUSYWAIT_UNTIL_ABS(NRF_TIMER0->EVENTS_COMPARE[0] != 0U, rx_target_time + 2*guard_time + SLOT_PROCESSING_TIME_PKT_START );
    slot_started = NRF_TIMER0->EVENTS_COMPARE[0];
    if(slot_started){
      // use the preprocessor if, to avoid having both variants on the chip
      #if (RADIO_MODE_CONF == RADIO_MODE_MODE_Ieee802154_250Kbit)
      BUSYWAIT_UNTIL_ABS(NRF_RADIO->EVENTS_FRAMESTART != 0U, rx_target_time + 2*guard_time + SLOT_PROCESSING_TIME_PKT_START + ADDRESS_EVENT_T_TX_OFFSET );
      got_address_event = NRF_RADIO->EVENTS_FRAMESTART;
      #else
      BUSYWAIT_UNTIL_ABS(NRF_RADIO->EVENTS_ADDRESS != 0U, rx_target_time + 2*guard_time + SLOT_PROCESSING_TIME_PKT_START + ADDRESS_EVENT_T_TX_OFFSET );
      got_address_event = NRF_RADIO->EVENTS_ADDRESS;
      #endif
    }
  }  
  if(rx_missed_slot || !slot_started) {
    // Things did not go like we wanted
  }
}

/*-------------------------------------------------------------------*/

/*
* In this function we evaluate the reception
*/
void
rx_evaluate(){
  if(got_address_event) {
    #if (RADIO_MODE_CONF == RADIO_MODE_MODE_Ieee802154_250Kbit)
    // no EVENTS_PAYLOAD is emitted
    // PAYLOAD_AIR_TIME_MIN includes CRC
    BUSYWAIT_UNTIL_ABS(NRF_RADIO->EVENTS_END != 0U, get_rx_ts() + PAYLOAD_AIR_TIME_MIN + SLOT_PROCESSING_TIME_PKT_END);

    got_end_event = NRF_RADIO->EVENTS_END;
    rx_latest_ok = got_payload_event = got_end_event;
    rx_latest_crc_ok = ((got_end_event != 0U) && (NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_CRCOk));
    #else
    BUSYWAIT_UNTIL_ABS(NRF_RADIO->EVENTS_PAYLOAD != 0U, get_rx_ts() + PAYLOAD_AIR_TIME_MIN);
    got_payload_event = NRF_RADIO->EVENTS_PAYLOAD;
    rx_latest_ok = got_payload_event;
    if(got_payload_event){
      BUSYWAIT_UNTIL_ABS(NRF_RADIO->EVENTS_END != 0U, get_rx_ts() + PAYLOAD_AIR_TIME_MIN + CRC_AIR_T + SLOT_PROCESSING_TIME_PKT_END);
      got_end_event = NRF_RADIO->EVENTS_END;
      rx_latest_crc_ok = ((got_end_event != 0U) && (NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_CRCOk));
    }
    #endif /* (RADIO_MODE_CONF == RADIO_MODE_MODE_Ieee802154_250Kbit) */
  }
}

/*-------------------------------------------------------------------*/

/*
* This function validates that the packet is useful
*/
void
rx_validate(){
  ble_beacon_t *rx_pkt = (ble_beacon_t *) my_rx_buffer;

  /* check if it is our beacon packet */
  rx_latest_ok = rx_latest_crc_ok ? (( rx_pkt->adv_address_low == MY_ADV_ADDRESS_LOW ) && ( rx_pkt->adv_address_hi == MY_ADV_ADDRESS_HI )) : 0;
  // rx_latest_ok = rx_latest_crc_ok; //XXX!

  if(rx_latest_ok){
    memcpy(&msg, &my_rx_buffer, rx_pkt->radio_len + 1);
    if(!synced){
      guard_time = GUARD_TIME_SHORT;
      synced = 1;
      if(!initiator){
        nrf_gpio_pin_toggle(SLOT1_INDICATOR_PIN);
        slot = rx_pkt->slot;
        logslot = slot + 1;
        round = rx_pkt->round;
        sync_slot = slot;
        t_start_round = get_rx_ts() - TX_CHAIN_DELAY - slot * SLOT_LEN;
        // calculates whether it is my turn to relay
        my_turn = (rx_pkt->turn == MSG_TURN_BROADCAST);
      }
    }
    if(sync_slot == UINT16_MAX){ //for the initiator
      sync_slot = rx_pkt->slot;
    }
    if(!joined){
      joined = 1;
    }
  }
}

/*-------------------------------------------------------------------*/

/*
* This is the part of the code where the logging stuff begins
* Therefore we also calculate some stats here
*/
void
rx_logging()
  {

  /* Log the reception to array */
  #if PRINT_TX_STATUS
  if(rx_latest_ok && rx_latest_crc_ok) {
    tx_status[logslot] = '-';             /* received packet was as expected */
  } else if(!slot_started) {
    tx_status[logslot] = 'M';             /* missed the slot*/
  }  else if(!got_address_event) {
    tx_status[logslot] = 'A';             /* Nothing happend here */
  } else if(!got_payload_event) {
    tx_status[logslot] = 'P';             /* got no payload */
  } else if(!got_end_event) {
    tx_status[logslot] = 'E';             /* missed the end of the packet */
  } else if(!rx_latest_crc_ok) {
    tx_status[logslot] = 'C';             /* received packet was corrupted */
  } else if(!rx_latest_ok) {
    tx_status[logslot] = 'W';             /* wrong address */
  } else {
    tx_status[logslot] = '?';             /* something unusual happend here */
  }
  #endif /* PRINT_TX_STATUS */

  /* Calculate the most useful stats */
  rx_ok += rx_latest_ok && rx_latest_crc_ok;
  if(CRC_LEN > 0){
    rx_crc_failed += got_address_event && !rx_latest_crc_ok;
  } else {
    rx_crc_failed += memcmp(&my_rx_buffer, &msg, msg.radio_len - 5) != 0;
  }
  rx_none += (!got_address_event || !got_end_event) && !rx_latest_ok;

  /* Log to array */
  #if PRINT_TS_DELTA
  rx_ts_delta[logslot] = get_rx_ts() - TX_CHAIN_DELAY - tt;
  #endif 
  #if PRINT_RSSI
  rx_rssi[logslot] = get_rx_rssi(&my_rx_buffer);
  #endif

  if(rx_latest_ok && !rx_latest_crc_ok){
    corrupt_msg_index |= (1UL << logslot);
  }
}

/*-------------------------------------------------------------------*/
/*------------------ Controlling component --------------------------*/
/*-------------------------------------------------------------------*/

void blueflood_start(uint16_t initiator_value, 
                    ble_beacon_t *payload, 
                    uint8_t payload_len,
                    uint8_t n_tx_max,
                    uint8_t already_joined,
                    uint16_t round_nr)
{
  // copy function arguments
  msg = *payload;
  msg_len = payload_len;
  initiator = initiator_value;
  joined = already_joined;
  tx_max = n_tx_max;
  round  = round_nr;

  // reset variables
  guard_time = GUARD_TIME;
  synced = 0;
  sync_slot = UINT16_MAX;
  my_turn = 0;
  corrupt_msg_index = 0;
  rx_ok = 0;
  tt = t_start_round + slot * SLOT_LEN;
  got_payload_event=0, got_address_event=0, got_end_event = 0, slot_started = 0, rx_latest_crc_ok = 0, rx_latest_ok = 0;
  ts_off = 0;

  // reset tx_status only if neccessary
  #if PRINT_TX_STATUS
  memset(tx_status, '.', sizeof(tx_status));
  tx_status[sizeof(tx_status)-1] = '\0';
  tx_status[0] = ':';
  #endif

  /*
  * Start the actual round
  */
  static uint8_t do_tx, do_rx;
  rx_crc_failed = 0, rx_none = 0;

  for(slot = 0; ROUND_LEN_RULE; slot++){
    // reset these variables for each slot
    logslot = slot + 1;
    tt = t_start_round + slot * SLOT_LEN;
    do_tx = (initiator && !synced && (slot % 2 == 0)) || (!initiator && synced && (slot > 0) && my_turn);
    do_rx = !do_tx;
    slot_len = SLOT_LEN;

    if(do_tx){
      transmit();
      //Logging
      #if PRINT_TS_DELTA
      rx_ts_delta[logslot] = get_rx_ts() - tt;
      #endif
      #if PRINT_RSSI
      rx_rssi[logslot] = get_radio_rssi();
      #endif

      //if not do_tx then listen for incoming pakets
    } else if(do_rx){
      join_trial = 0;
      do{
        //set variables
        got_payload_event=0, got_address_event=0, got_end_event = 0, slot_started = 0, rx_latest_crc_ok = 0, rx_latest_ok = 0;

        /* Join the network if not already joined */
        if(!joined){
          join();
        } else {
          receive();
        }
        rx_evaluate();

        /* check if it is a valid packet: a. our uuid and b. CRC ok */
        if(rx_latest_ok && rx_latest_crc_ok){
          rx_validate();
        } else {
          /* if the radio got stuck in bootstrap mode, then turn it off and on again. it is needed when we get a crc error */
          my_radio_off_completely();
          #if PRINT_RADIO
          ts_off = RTIMER_NOW();
          #endif
        }
      } while(!joined); /* if not joined yet, reapeat the do part */

      rx_logging();
      
    }
  } /* End of for-loop */
}

/*-------------------------------------------------------------------*/

uint16_t blueflood_stop(void)
{
  my_radio_off_completely();
  #if PRINT_RADIO
  ts_off = RTIMER_NOW();
  #endif

  // n_rx should at max be 1 in a flood, so the question would be: Is the rx ok?
  return rx_ok;
}

/*-------------------------------------------------------------------*/

void blueflood_set_t_start_round(rtimer_clock_t time){
  t_start_round = time;
}

/*-------------------------------------------------------------------*/
/*----------------------- Synchronization component -----------------*/
/*-------------------------------------------------------------------*/

rtimer_clock_t blueflood_get_t_start_round(){
  return t_start_round;
}

/*-------------------------------------------------------------------*/

uint8_t blueflood_get_joined(void){
  return joined;
}

/*-------------------------------------------------------------------*/
/*----------------------- Logging component -------------------------*/
/*-------------------------------------------------------------------*/

uint16_t blueflood_get_round(){
  return round;
}

/*-------------------------------------------------------------------*/

uint16_t blueflood_get_rx_ok(){
  return rx_ok;
}

/*-------------------------------------------------------------------*/

uint8_t blueflood_get_payload_len(){
  return msg_len;
}

/*-------------------------------------------------------------------*/

void blueflood_set_tx_status(char *array){
  #if PRINT_TX_STATUS
  tx_status = array;
  #endif /* PRINT_TX_STATUS */
}

/*-------------------------------------------------------------------*/

void blueflood_set_rx_ts_delta_status(int32_t *array){
  #if PRINT_TS_DELTA
  rx_ts_delta = array;
  #endif /* PRINT_TS_DELTA */
}

/*-------------------------------------------------------------------*/

void blueflood_set_rx_rssi(int8_t *array){
  #if PRINT_RSSI
  rx_rssi = array;
  #endif /* PRINT_RSSI */
}

/*-------------------------------------------------------------------*/

#if PRINT_RX_STATS
uint16_t blueflood_get_crc_failed(){
  return rx_crc_failed;
}
#endif

/*-------------------------------------------------------------------*/
#if PRINT_RX_STATS
uint16_t blueflood_get_rx_none(){
  return rx_none;
}
#endif

/*-------------------------------------------------------------------*/
#if PRINT_RADIO
void blueflood_set_ts_radio(rtimer_clock_t *array){
  ts_radio = array;
}
#endif

/*-------------------------------------------------------------------*/
#if PRINT_RADIO
rtimer_clock_t blueflood_get_ts_radio_off(){
  return ts_off;
}
#endif
/*-------------------------------------------------------------------*/