#include <inttypes.h>
#include <string.h>
#include <random.h>
#include "contiki.h"
#include "dev/leds.h"
#include "simple-uart.h"
#include "nrf-radio-driver.h"
#include "watchdog.h"

#include "uuids.h"
#include "ble-beacon-header.h"
#include "encode-decode-hamming-crc24.h"

#include "testbed.h"
#include "nrf-gpio.h"
/*---------------------------------------------------------------------------*/
/*
* If you want to just have the Hello World Testbed, uncomment the define below
*/
//#define TEST_HELLO_WORLD 1

/*
 * NTX is defined in the Makefile.
 */
#ifndef NTX
#define ROUND_LEN (4U)
#define ROUND_RX_LEN (2*TESTBED_DIAMETER+ROUND_LEN)
#else
#define ROUND_LEN ((NTX))
#define ROUND_RX_LEN (2*TESTBED_DIAMETER+NTX)
#endif /* NTX */
#ifndef ROUND_PERIOD_CONF_US
#error "Define round period!"
#endif
#define ROUND_LEN_MAX (ROUND_RX_LEN+ROUND_LEN)
#define ROUND_PERIOD_CONF_RTICKS US_TO_RTIMERTICKS(ROUND_PERIOD_CONF_US)
#define ROUND_PERIOD (ROUND_PERIOD_CONF_RTICKS - ROUND_LEN_MAX * SLOT_LEN)
/*---------------------------------------------------------------------------*/
#define LOG_STATE_SIZE (ROUND_LEN_MAX)
/*---------------------------------------------------------------------------*/
#define IBEACON_SIZE  (sizeof(ble_beacon_t))
#define BLUETOOTH_BEACON_PDU(S) (8+(S))
#define PACKET_AIR_TIME_MIN (PACKET_AIR_TIME(BLUETOOTH_BEACON_PDU(IBEACON_SIZE),RADIO_MODE_CONF))
#define PAYLOAD_AIR_TIME_MIN (PACKET_AIR_TIME(IBEACON_SIZE,RADIO_MODE_CONF))
#define RX_SLOT_LEN (SLOT_PROCESSING_TIME+TX_CHAIN_DELAY+ US_TO_RTIMERTICKS(MY_RADIO_RAMPUP_TIME_US) + PACKET_AIR_TIME_MIN)
#define SLOT_LEN (RX_SLOT_LEN+2*GUARD_TIME_SHORT)
#define SLOT_LEN_NOTSYNCED (RX_SLOT_LEN+GUARD_TIME)
#define FIRST_SLOT_OFFSET (SLOT_PROCESSING_TIME + GUARD_TIME + ADDRESS_EVENT_T_TX_OFFSET)
/*---------------------------------------------------------------------------*/
const uint8_t uuids_array[UUID_LIST_LENGTH][16] = UUID_ARRAY;
const uint32_t testbed_ids[] = TESTBED_IDS;
enum {MSG_TURN_BROADCAST=0xff, MSG_TURN_NONE=0xfe};
/*---------------------------------------------------------------------------*/
#define tx_node_id        (TESTBED_IDS[INITATOR_NODE_INDEX])
#define IS_INITIATOR() (my_id == tx_node_id)
/*---------------------------------------------------------------------------*/
#if PRINT_CUSTOM_DEBUG_MSG
static char dbgmsg[256]="", dbgmsg2[256]="";
#endif
static uint8_t my_tx_buffer[255] = {0};
static uint8_t my_rx_buffer[255] = {0};

static ble_beacon_t msg;
#if PRINT_LAST_RX
static ble_beacon_t msg_errors[ROUND_LEN];
#endif /* PRINT_LAST_RX */
static uint64_t corrupt_msg_index = 0;
static uint32_t my_id = 0;
/*---------------------------------------------------------------------------*/
PROCESS(tx_process, "TX process");
AUTOSTART_PROCESSES(&tx_process);
/*---------------------------------------------------------------------------*/
static int get_testbed_index(uint32_t my_id, const uint32_t *testbed_ids, uint8_t testbed_size){
  int i;
  for( i=0; i<testbed_size; i++ ){
    if( my_id == testbed_ids[i] ){
      return i;
    }
  }
  return -1;
}
/*---------------------------------------------------------------------------*/
static void init_ibeacon_packet(ble_beacon_t *pkt, const uint8_t* uuid, uint16_t round, uint16_t slot){
#if (RADIO_MODE_CONF == RADIO_MODE_MODE_Ieee802154_250Kbit)
    pkt->radio_len = sizeof(ble_beacon_t) - 1; /* execlude len field */
#else
  pkt->radio_len = sizeof(ble_beacon_t) - 2; /* len + pdu_header */ //length of the rest of the packet

#endif
  pkt->pdu_header = 0x42; //pdu type: 0x02 ADV_NONCONN_IND, rfu 0, rx 0, tx 1 //2;
  pkt->adv_address_low = MY_ADV_ADDRESS_LOW;
  pkt->adv_address_hi = MY_ADV_ADDRESS_HI;
  memcpy(pkt->uuid, uuid, sizeof(pkt->uuid));
  pkt->round = round;
  //pkt->minor = 0;
  pkt->slot = slot;
  pkt->turn = MSG_TURN_NONE;

  #if (PACKET_IBEACON_FORMAT)
  pkt->ad_flags_length = 2; //2bytes flags
  pkt->ad_flags_type = 1; //1=flags
  pkt->ad_flags_data = 6; //(non-connectable, undirected advertising, single-mode device)
  pkt->ad_length = 0x1a; //26 bytes, the remainder of the packet
  pkt->ad_type = 0xff; //manufacturer specific
  pkt->company_id = 0x004cU; //Apple ID
  pkt->beacon_type = 0x1502;//0x0215U; //proximity ibeacon
  pkt->power = 0;//256 - 60; //RSSI = -60 dBm; Measured Power = 256 – 60 = 196 = 0xC4
  #endif
}
/*---------------------------------------------------------------------------*/
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
/*---------------------------------------------------------------------------*/
#if !BLUEFLOOD_BUSYWAIT
void rtc_schedule(uint32_t ticks);
#endif
/*---------------------------------------------------------------------------*/
/*
*This is the starting point in the file.
*/
PROCESS_THREAD(tx_process, ev, data)
{
  //marks if it is my turn to do a transmission
  static uint8_t my_turn = 0;
  //counts numbers of not okay received packets (rx_ok == 0)
  static uint8_t failed_rounds = 0;
  //use this to decide if it is your turn
  static int8_t my_index = -1;
  //Some paket ingredience
  static uint16_t round = 0, slot = 0, 
  // logging
  logslot = 0, 
  //joined the round? When synced last time?
  join_round = -1, sync_slot = UINT16_MAX;
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
  #if PRINT_LAST_RX
  // static uint8_t berr_pkts[LOG_STATE_SIZE]={0};
  #endif 
  #if PRINT_TS_DELTA
  static int32_t rx_ts_delta[LOG_STATE_SIZE]={0UL};
  #endif //PRINT_TS_DELTA
  #if PRINT_TX_STATUS
  static char tx_status[LOG_STATE_SIZE+1]={0};
  #endif /* PRINT_TX_STATUS */

  //Locale clock
  volatile static rtimer_clock_t tt = 0, t_start_round = 0;
  static bool do_tx = 0, do_rx = 0, synced = 0, joined = 0;
  static volatile bool  last_crc_is_ok = 0;
  // initialize guard_time. Later set to GUARD_TIME.
  uint32_t guard_time = 0;
  uint8_t last_rx_ok = 0;

  //index for for-loops in print-logging
  int i;
  
  PROCESS_BEGIN();

  testbed_cofigure_pins();
  testbed_clear_debug_pins();
  my_radio_init(&my_id, my_tx_buffer);
  // Get my position in the testbed.h
  my_index = get_testbed_index(my_id, testbed_ids, TESTBED_SIZE);
  init_ibeacon_packet(&msg, &uuids_array[0][0], round, slot);
  watchdog_periodic();

// This is basically also guardtime
  if(IS_INITIATOR()){
    nrf_delay_ms(2000);
  } else{
    nrf_delay_ms(1000);
  }

  t_start_round = RTIMER_NOW() + FIRST_SLOT_OFFSET;
  joined = 0;
  join_round = UINT16_MAX;

  while(1)
  {
    // reset booleans
    rx_ok = 0, rx_crc_failed = 0, rx_none = 0; tx_done=0; berr = 0; 
    // reset a lot of variables for the new round
    berr_per_pkt_max = 0, berr_per_byte_max = 0; corrupt_msg_index = 0;

    #if PRINT_TX_STATUS
    tx_status[0] = ':';
    #endif /* PRINT_TX_STATUS */

    guard_time = GUARD_TIME;
    synced = 0;
    sync_slot = UINT16_MAX;
    my_turn = 0;

    #define ROUND_LEN_RULE (((!IS_INITIATOR()) && synced && (slot < sync_slot + ROUND_LEN)) || ((IS_INITIATOR() || !synced) && (slot < ROUND_RX_LEN)) )

    nrf_gpio_pin_toggle(ROUND_INDICATOR_PIN);

    // for each slot do:
    for(slot = 0; ROUND_LEN_RULE; slot++){
      logslot = slot + 1;
      tt = t_start_round + slot * SLOT_LEN;
      do_tx = (IS_INITIATOR() && !synced && (slot % 2 == 0)) || (!IS_INITIATOR() && synced && (slot > 0) && my_turn);
      
      //do_tx = my_id == tx_node_id;
      do_rx = !do_tx;
      if(do_tx){
        msg.slot = slot;
        msg.round = round;
        joined = 1;
        guard_time = GUARD_TIME_SHORT;
        msg.turn = MSG_TURN_BROADCAST; //all nodes are allowed to send

        uint8_t *tx_msg = (uint8_t *)&msg;

        //schedule transmission for absolute time
        schedule_tx_abs(tx_msg, GET_CHANNEL(round,slot), tt - ADDRESS_EVENT_T_TX_OFFSET + ARTIFICIAL_TX_OFFSET);
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
            tx_done++;
            #if PRINT_TX_STATUS
            tx_status[logslot] =  ( msg.turn == MSG_TURN_NONE ) ? 'X' : ( ( msg.turn == MSG_TURN_BROADCAST ) ? 'B' : ( ( msg.turn <= TESTBED_SIZE ) ? HEXC(msg.turn) : 'U' ) );
            #endif /* PRINT_TX_STATUS */
          }
        }

        //Logging
        #if PRINT_TS_DELTA
        rx_ts_delta[logslot] = get_rx_ts() - tt;
        #endif //PRINT_TS_DELTA
        #if PRINT_RSSI
        rx_rssi[logslot] = get_radio_rssi();
        #endif

        //if not do_tx then listen for incoming pakets
      } else if(do_rx){
        static int join_trial = 0;
        uint8_t got_payload_event, got_address_event, got_end_event, slot_started;
        do{
          //set variables
          got_payload_event=0, got_address_event=0, got_end_event = 0, slot_started = 0, last_crc_is_ok = 0, last_rx_ok = 0;
          uint8_t channel = 0;

          /* slave bootstrap code */
          if(!joined){
            /* hop the channel when we have waited long enough on one channel: 2*N/(NTX/2) rounds */
            if( join_trial++ % (MAX(12,2*NUMBER_OF_CHANNELS)/NTX) == 0 ){
              channel=GET_CHANNEL(random_rand(),0);
            }
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
          } else {
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
              schedule_rx_abs(my_rx_buffer, GET_CHANNEL(round, slot), rx_target_time);
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
              #if PRINT_CUSTOM_DEBUG_MSG
              SPRINTF(dbgmsg, "t %" PRIu32 " %" PRIu32 " n %" PRIu32 " p %" PRIu32 " m %d %d", (t_start_round), rx_target_time, rx_tn, t_proc, rx_missed_slot, slot_started );
              #endif
            }
          }

          if(got_address_event) {
            #if (RADIO_MODE_CONF == RADIO_MODE_MODE_Ieee802154_250Kbit)
            //no EVENTS_PAYLOAD is emitted
            // PAYLOAD_AIR_TIME_MIN includes CRC
            BUSYWAIT_UNTIL_ABS(NRF_RADIO->EVENTS_END != 0U, get_rx_ts() + PAYLOAD_AIR_TIME_MIN + SLOT_PROCESSING_TIME_PKT_END);

            got_end_event = NRF_RADIO->EVENTS_END;
            last_rx_ok = got_payload_event = got_end_event;
            last_crc_is_ok = USE_HAMMING_CODE || ((got_end_event != 0U) && (NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_CRCOk));
            #else
            BUSYWAIT_UNTIL_ABS(NRF_RADIO->EVENTS_PAYLOAD != 0U, get_rx_ts() + PAYLOAD_AIR_TIME_MIN);
            got_payload_event = NRF_RADIO->EVENTS_PAYLOAD;
            last_rx_ok = got_payload_event;
            if(got_payload_event){
              BUSYWAIT_UNTIL_ABS(NRF_RADIO->EVENTS_END != 0U, get_rx_ts() + PAYLOAD_AIR_TIME_MIN + CRC_AIR_T + SLOT_PROCESSING_TIME_PKT_END);
              got_end_event = NRF_RADIO->EVENTS_END;
              last_crc_is_ok = USE_HAMMING_CODE || ((got_end_event != 0U) && (NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_CRCOk));
            }
            #endif /* (RADIO_MODE_CONF == RADIO_MODE_MODE_Ieee802154_250Kbit) */
          }
          /* check if it is a valid packet: a. our uuid and b. CRC ok */
          if(last_rx_ok && last_crc_is_ok){
            ble_beacon_t *rx_pkt = (ble_beacon_t *) my_rx_buffer;

            /* check if it is our beacon packet */
            last_rx_ok = last_crc_is_ok ? (( rx_pkt->adv_address_low == MY_ADV_ADDRESS_LOW ) && ( rx_pkt->adv_address_hi == MY_ADV_ADDRESS_HI )) : 0;
            // last_rx_ok = last_crc_is_ok; //XXX!

            if(last_rx_ok){
              memcpy(&msg, &my_rx_buffer, rx_pkt->radio_len + 1);
              if(!synced){
                guard_time = GUARD_TIME_SHORT;
                synced = 1;
                if(!IS_INITIATOR()){
                  nrf_gpio_pin_toggle(SLOT1_INDICATOR_PIN);
                  slot = rx_pkt->slot;
                  logslot = slot + 1;
                  round = rx_pkt->round;
                  sync_slot = slot;
                  t_start_round = get_rx_ts() - TX_CHAIN_DELAY - slot * SLOT_LEN;
                  // calculates whether it is my turn to relay
                  my_turn = (rx_pkt->turn == my_index) || (rx_pkt->turn == MSG_TURN_BROADCAST);
                }
              }
              if(sync_slot == UINT16_MAX){ //for the initiator
                sync_slot = rx_pkt->slot;
              }
              if(!joined || join_round == UINT16_MAX){
                join_round = round;
                joined = 1;
              }
            }
          } else {
            /* if the radio got stuck in bootstrap mode, then turn it off and on again. it is needed when we get a crc error */
            my_radio_off_completely();
          }
        } while(!joined); /* if not joined yet, reapeat the do part */

        //Logging
        #if PRINT_TX_STATUS
        if(last_rx_ok && last_crc_is_ok) {
          tx_status[logslot] = '-'; //received packet was as expected
        } else if(!slot_started) {
          tx_status[logslot] = 'M';
        }  else if(!got_address_event) {
          tx_status[logslot] = 'A'; //Nothing happend here
        } else if(!got_payload_event) {
          tx_status[logslot] = 'P';
        } else if(!got_end_event) {
          tx_status[logslot] = 'E';
        } else if(!last_crc_is_ok) {
          tx_status[logslot] = 'C'; //received packet was corrupted
        } else if(!last_rx_ok) {
          tx_status[logslot] = 'W'; //wrong address
        } else {
          tx_status[logslot] = '?';
        }
        #endif /* PRINT_TX_STATUS */


        rx_ok += last_rx_ok && last_crc_is_ok;
        // Hamming Code is sort of encryption
        if(CRC_LEN > 0 || USE_HAMMING_CODE){
          rx_crc_failed += got_address_event && !last_crc_is_ok;
        } else {
          rx_crc_failed += memcmp(&my_rx_buffer, &msg, msg.radio_len - 5) != 0;
        }
        rx_none += (!got_address_event || !got_end_event) && !last_rx_ok;


        #if PRINT_TS_DELTA
        rx_ts_delta[logslot] = get_rx_ts() - TX_CHAIN_DELAY - tt;
        #endif //PRINT_TS_DELTA
        #if PRINT_RSSI
        rx_rssi[logslot] = get_rx_rssi(&my_rx_buffer);
        #endif //PRINT_RSSI


        if(last_rx_ok && !last_crc_is_ok){
          corrupt_msg_index |= (1UL << logslot);

          #if PRINT_LAST_RX
          uint8_t * pmsg = (uint8_t*)&msg;
          uint8_t * pmsg_errors = (uint8_t*)&msg_errors[logslot];
          int i; 
          uint8_t berr_byte, berr_per_pkt, berr_xor;
          berr_byte = 0;
          for(i = 0; i < sizeof(ble_beacon_t); i++){
            berr_xor = my_rx_buffer[i] ^ pmsg[i]; /* berr_xor = ones if there is a difference */
            pmsg_errors[i] = berr_xor;
            if(berr_xor){ 
              berr_byte = ((berr_xor & 1) != 0) + ((berr_xor & 2) != 0) + ((berr_xor & 4) != 0) + ((berr_xor & 8) != 0)
              + ((berr_xor & 16)!= 0) + ((berr_xor & 32)!= 0) + ((berr_xor & 64)!= 0) + ((berr_xor & 128)!= 0);
              berr_per_pkt += berr_byte;
              berr_per_byte_max = berr_per_byte_max >= berr_byte ? berr_per_byte_max : berr_byte;
            }
          }
          // berr_pkts[logslot] = berr_per_pkt;
          berr += berr_per_pkt;
          berr_per_pkt_max = berr_per_pkt_max >= berr_per_pkt ? berr_per_pkt_max : berr_per_pkt;
          #endif /* PRINT_LAST_RX */
        }
      }
    } /* End of big for-loop */

    my_radio_off_completely();
    nrf_gpio_pin_toggle(ROUND_INDICATOR_PIN);
    if(synced){
      nrf_gpio_pin_toggle(SLOT1_INDICATOR_PIN);
    }
    rx_ok_total += rx_ok;
    berr_total += berr;
    rx_failed_total += rx_crc_failed + rx_none;
    uint32_t rx_ok_percent = (rx_ok_total*100) / (MAX(1, rx_ok_total+rx_failed_total));

#if ENABLE_BLUEFLOOD_LOGS
  #if PRINT_LAST_RX
  /*PRINTF("BERRs: ");
  for(i=0; i<sizeof(berr_pkts)/sizeof(berr_pkts[0]); i++){
    PRINTF("%d, ", berr_pkts[i]);
  }
  PRINTF("bits per packet\n");*/
  // memset(berr_pkts, 0, sizeof(berr_pkts));
  #endif /* PRINT_LAST_RX */
  
  #if TESTBED_LOG_STYLE
    #if PRINT_RX_STATS
    PRINTF("{rx-%d} %u, %u, %u, %u, %lu, %lu, %u, %u, %u, %lu, %d\n", round, rx_ok, rx_crc_failed, rx_none, tx_done, rx_ok_total, rx_ok_total+rx_failed_total, berr_per_byte_max, berr_per_pkt_max, berr /* bit errors per round */, berr_total, sync_slot);
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
    PRINTF("rx_ok %u, crc %u, none %u, tx %u: OK %lu of %lu, berr b%u p%u r%u %lu, sync %d\n", rx_ok, rx_crc_failed, rx_none, tx_done, rx_ok_total, rx_ok_total+rx_failed_total, berr_per_byte_max, berr_per_pkt_max, berr /* bit errors per round */, berr_total, sync_slot);

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

  #if PRINT_LAST_RX
    PRINTF("{err-%d} ", round);
    uint8_t *pmsg = (uint8_t *)&msg;
    for(i=0; i<=msg.radio_len; i++){
      PRINTF("%02x ", pmsg[i]);
    }
    PRINTF("CRC: %lx.", NRF_RADIO->RXCRC);
    PRINTF("\n");
    if(corrupt_msg_index == 0){
      PRINTF("No errors.\n");
    } else {
      int s;
      for(s=1; s<ROUND_LEN; s++){
        if(corrupt_msg_index & (1UL << s)){
          PRINTF("[%2d] ", s);
          uint8_t *pmsg_errors = (uint8_t *)&msg_errors[s];
          for(i=0; i<=msg.radio_len; i++){
            PRINTF("%02x ", pmsg_errors[i]);
          }
          PRINTF("\n");
        }
      }
    }
    memset(&msg_errors, 0, ROUND_LEN*msg.radio_len);
  #endif /* PRINT_LAST_RX */

  #if PRINT_NODE_CONFIG
    if(round % 1024 == 0){
      PRINTF("#R %u, ID: 0x%lx %d, master: 0x%lx, tx power: %d dBm, channel %u = %u MHz (%s), msg: %d bytes, mode: %s, CE: %d, @ %s\n", 
              round, my_id, my_index, tx_node_id, (int8_t)BLE_DEFAULT_RF_POWER, BLE_DEFAULT_CHANNEL, 2400u+ble_hw_frequency_channels[BLE_DEFAULT_CHANNEL], OVERRIDE_BLE_CHANNEL_37 ? "not std" : "std", sizeof(ble_beacon_t), RADIO_MODE_TO_STR(RADIO_MODE_CONF), TEST_CE, FIRMWARE_TIMESTAMP_STR);
    }
  #endif /* PRINT_NODE_CONFIG */
#endif /* ENABLE_BLUEFLOOD_LOGS */

    /* If the received packet was not okay: */
    if(rx_ok == 0){
      //did not receive for X round: resync
      if(failed_rounds > 10){
        synced = 0;
        joined = 0;
        #if PRINT_NODE_REJOIN_WARNING
        PRINTF("{fr-%d} Rejoining: failed rounds %d, joined %d, synced %d\n", (int)round, (int)failed_rounds, (int)joined, (int)synced);
        #endif
      }
      failed_rounds++;
    } else {
      failed_rounds = 0;
      rx_ok = 0;
    }

    round++;
    init_ibeacon_packet(&msg, &uuids_array[0][0], round, slot);
    memset(my_rx_buffer, 0, msg.radio_len);
    rtimer_clock_t now, t_start_round_old;
    now = RTIMER_NOW();
    #define TIMER_GUARD 16
    uint8_t round_is_late = check_timer_miss(t_start_round, ROUND_PERIOD-TIMER_GUARD, now);
    t_start_round += ROUND_PERIOD;

    if(round_is_late){
      #if PRINT_GO_LATE_WARNING
      PRINTF("#!{%d}PRE GO late: %ld\n", round, now - t_start_round);
      #endif /* PRINT_GO_LATE_WARNING */
      round_is_late = check_timer_miss(t_start_round, ROUND_PERIOD-TIMER_GUARD, now);
      t_start_round += ROUND_PERIOD;
    }
    
  #if BLUEFLOOD_BUSYWAIT
    /* wait at the end of the round */
    NRF_TIMER0->CC[0] = t_start_round - FIRST_SLOT_OFFSET;;
    while(!NRF_TIMER0->EVENTS_COMPARE[0]){
      watchdog_periodic();
    }
  #else
    rtimer_clock_t tnow = RTIMER_NOW();
    uint32_t rtc_ticks = RTIMER_TO_RTC((t_start_round - tnow))-RTC_GUARD; //save one RTC tick for preprocessing!
    rtimer_clock_t sleep_period = (t_start_round - tnow);
    // PRINTF("going to sleep: now %lu for %" PRId32 " hf = %lu hf %lu lf\n", tnow, sleep_period, RTC_TO_RTIMER(rtc_ticks), rtc_ticks);
    rtc_schedule(rtc_ticks);
    /* go to sleep mode: put prepherals to sleep then sleep the CPU */
    // NRF_RADIO->POWER = 0;
    // /* Unonfigure the channel as the caller expects */
    // for (int i = 0; i < 8; i++)
    // {
    //   NRF_GPIOTE->CONFIG[i] = (GPIOTE_CONFIG_MODE_Disabled << GPIOTE_CONFIG_MODE_Pos) |
    //                           (31UL << GPIOTE_CONFIG_PSEL_Pos) |
    //                           (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos);
    // }
    // /* Three NOPs are required to make sure configuration is written before setting tasks or getting events */
    // __NOP();
    // __NOP();
    // __NOP();
    /* Go to sleep sequence: 
     * SEV Set event and WFE wait for event first to consume any previously set event if any, then wait for event to sleep the CPU until an event happens. 
    */
    __SEV();
    __WFE();
    __WFE();
    // testbed_cofigure_pins();
    // my_radio_init(&my_id, my_tx_buffer);    /* turn LEDs off: active low, so set the pins */
    // rtimer_clock_t tnow2 = RTIMER_NOW();
    // rtimer_clock_t sleep_period2 = (tnow2 - tnow);
    // //correct the round timer based on the sleep time, because timer0 was sleeping // NO!
    // //XXX Timer0 is still counting. No need to adjust it.
    // // t_start_round -= RTC_TO_RTIMER(rtc_ticks);
    // // PRINTF("wakeup: now %lu for %" PRId32 " hf = %lu hf %lu lf\n", tnow2, sleep_period2, RTIMER_TO_RTC(sleep_period2), rtc_ticks);
    guard_time = GUARD_TIME;
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
  NRF_RTC1->PRESCALER = RTC_PRESCALER-1; //fRTC [kHz] = 32.768 / (PRESCALER + 1 )
  NRF_RTC1->TASKS_CLEAR=1;
  NRF_RTC1->CC[1]=ticks;
  /* Enable compare event and compare interrupt */
  NRF_RTC1->EVTENSET      = RTC_EVTENSET_COMPARE1_Msk;;
  NRF_RTC1->INTENSET      = RTC_INTENSET_COMPARE1_Msk;
  /* Enable Interrupt for RTC1 in the core */
  NVIC_SetPriority(RTC1_IRQn, 3);
  NVIC_EnableIRQ(RTC1_IRQn);
  NRF_RTC1->TASKS_START = 1;
  //poll the WDT so it does not fire early. we keep it running though so it wakes us up if something wrong happened...
  watchdog_periodic();
}

/** \brief Function for handling the RTC1 interrupts.
 */
void RTC1_IRQHandler()
{
  if(NRF_RTC1->EVENTS_COMPARE[1] == 1){
    #ifdef RTC_SCHEDULE_PIN
    nrf_gpio_cfg_output(RTC_SCHEDULE_PIN);
    nrf_gpio_pin_toggle(RTC_SCHEDULE_PIN);
    #endif
    NRF_RTC1->EVENTS_COMPARE[1] = 0;
    // Disable COMPARE1 event and COMPARE1 interrupt:
    NRF_RTC1->EVTENCLR      = RTC_EVTENSET_COMPARE1_Msk;
    NRF_RTC1->INTENCLR      = RTC_INTENSET_COMPARE1_Msk;
    //PRINTF("poll\n");
    NRF_RTC1->TASKS_STOP = 1;
    NVIC_DisableIRQ(RTC1_IRQn);
  }
}
#endif