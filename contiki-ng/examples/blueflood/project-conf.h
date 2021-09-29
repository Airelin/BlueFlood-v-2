#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/*
 * This file should be used to override default settings if necessary
 */

/* Testbed configuration */
#define TESTBED_CONF HOME_TESTBED
#ifdef TESTBED_CONF
#define TESTBED   TESTBED_CONF
#endif

/* Flooding configuration */
#define INITATOR_NODE_INDEX       1         /* Index in the liist from testbed.h */
#define NTX                       4         /* Set the maximum number of retransmissions for our test-blueflood.c here */

/* Timing configuration */
#define ROUND_PERIOD_MS_PARAM     200       /* in ms */

/* Radio configuration */
#define NUMBER_OF_CHANNELS        40        /* 40 is the maximum number of channels that can be set*/
#define BLE_DEFAULT_RF_POWER      -16       /* in dB */
#define RADIO_MODE_CONF           4         /* possible modes: Ble_LR125Kbit=5, Ble_LR500Kbit=6, Ble_1Mbit=3, Ble_2Mbit=4 */
#define OVERRIDE_BLE_CHANNEL_37   false     /* use a special frequency for channel 37 to avoid Bluetooth traffic */

/* Logging configuration */
#define PRINT_FIXED_ROUND         true      /* test the fixed round scenario while sending upcounting roundnumbers, for testing and evaluation purposes only */
#define ENABLE_BLUEFLOOD_LOGS     true

/* Necessary defaults */
#define BLUEFLOOD_BUSYWAIT        false 
#define PACKET_TEST_SIZE_CONF     38        /* Packet size in bytes, default PDU for standard ibeacon */

#endif /* PROJECT_CONF_H_ */