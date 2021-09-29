#ifndef BLUEFLOOD_H_
#define BLUEFLOOD_H_

#include "contiki.h"
#include "ble-beacon-header.h"

/*-------------------------------------------------------------------*/
/*------------------------ Makros and defines -----------------------*/
/*-------------------------------------------------------------------*/

#define IBEACON_SIZE  (sizeof(ble_beacon_t))
#define PACKET_AIR_TIME_MIN (PACKET_AIR_TIME(BLUETOOTH_BEACON_PDU(IBEACON_SIZE),RADIO_MODE_CONF))

#define ROUND_LEN_MAX (ROUND_RX_LEN+ROUND_LEN)
#define ROUND_PERIOD_CONF_RTICKS US_TO_RTIMERTICKS(ROUND_PERIOD_CONF_US)
#define ROUND_PERIOD (ROUND_PERIOD_CONF_RTICKS - ROUND_LEN_MAX * SLOT_LEN)

#define RX_SLOT_LEN (SLOT_PROCESSING_TIME+TX_CHAIN_DELAY+US_TO_RTIMERTICKS(MY_RADIO_RAMPUP_TIME_US) + PACKET_AIR_TIME_MIN)
#define SLOT_LEN (RX_SLOT_LEN+2*GUARD_TIME_SHORT)

#define FIRST_SLOT_OFFSET (SLOT_PROCESSING_TIME + GUARD_TIME + ADDRESS_EVENT_T_TX_OFFSET)

#define TX_TIME_ABS (tt - ADDRESS_EVENT_T_TX_OFFSET + ARTIFICIAL_TX_OFFSET)

/*-------------------------------------------------------------------*/
/*------------------ Controlling component --------------------------*/
/*-------------------------------------------------------------------*/

/*
* This is the function to start a flood. 
* Set the time t_start_round to a time in the future before calling this method.
*/
void blueflood_start(uint16_t initiator_value, 
                    ble_beacon_t *payload, 
                    uint8_t payload_len,
                    uint8_t n_tx_max,
                    uint8_t already_joined,
                    uint16_t round_nr);

/*
* Turns off the radio after a flood is done.
* Returns if there was a successful rx this flood.
*/
uint16_t blueflood_stop(void);

/*
* Set the time to start the round. 
* Make sure that the time is in the future.
* Use this before calling blueflood_start(...)
*/
void blueflood_set_t_start_round(rtimer_clock_t time);

/*-------------------------------------------------------------------*/
/*----------------------- Synchronization component -----------------*/
/*-------------------------------------------------------------------*/

/*
* Get the modified reference time.
* Returns the point in time the initiator started the round.
*/
rtimer_clock_t blueflood_get_t_start_round();

/*
* Get the current state of synchronization.
* Returns 0, if we did not successfully joined the network
* Returns 1, if we successfully joined the network
*/
uint8_t blueflood_get_joined(void);

/*-------------------------------------------------------------------*/
/*----------------------- Logging component -------------------------*/
/*-------------------------------------------------------------------*/

/*
* Get the current round number
* This step is especially neccessary for the receiving nodes
*/
uint16_t blueflood_get_round();

/*
* Get whether we received something during the last flood.
*/
uint16_t blueflood_get_rx_ok();

/*
* Get the length of the message
*/
uint8_t blueflood_get_payload_len();

#if PRINT_RX_STATS
uint16_t blueflood_get_crc_failed();
uint16_t blueflood_get_rx_none();
#endif

/*-------------------------------------------------------------------*/

/*
* Pass where to store the ts_status logging data
*/
void blueflood_set_tx_status(char *array);

/*
* Pass where to store the rx_rssi logging data
*/
void blueflood_set_rx_rssi(int8_t *array);

/*
* Pass where to store the rx_ts_delta logging data
*/
void blueflood_set_rx_ts_delta_status(int32_t *array);


#if PRINT_RADIO
/*
* Pass where to store the radio timestamp logging data
*/
void blueflood_set_ts_radio(rtimer_clock_t *array);

/*
* Get the time the radio was turned off
*/
rtimer_clock_t blueflood_get_ts_radio_off();
#endif

/*-------------------------------------------------------------------*/

#endif /*BLUEFLOOD_H_*/