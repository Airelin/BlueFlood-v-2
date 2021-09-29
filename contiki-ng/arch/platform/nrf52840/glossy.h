#include "contiki.h"
#include "rtimer-ext.h"

#if BALOO
#include "gmw.h"
#endif

#ifndef GLOSSY_CONF_PAYLOAD_LEN
#define GLOSSY_CONF_PAYLOAD_LEN         100
#endif /* GLOSSY_CONF_PACKET_SIZE */

/* magic number / identifier for a Glossy packet (4 bits only) */
#ifndef GLOSSY_CONF_HEADER_BYTE
#define GLOSSY_CONF_HEADER_BYTE         0x0a
#endif /* GLOSSY_CONF_HEADER_BYTE */

/* Try to keep the setup time in glossy_start() constant to allow for precide
 * drift compensation on the source nodes. The flood initiator will wait until
 * the specified amount has passed before issuing the start_tx command. 
 * Note: This define only has an effect on the initiator if sync is enabled.
 *       This feature can be disabled by setting the value to zero. */
#ifndef GLOSSY_CONF_SETUPTIME_WITH_SYNC
#define GLOSSY_CONF_SETUPTIME_WITH_SYNC 1000UL    /* in us */
#endif /* GLOSSY_CONF_SETUPTIME_WITH_SYNC */

/* do not change */
#define GLOSSY_MAX_HEADER_LEN               4

#define GLOSSY_CONF_COLLECT_STATS 1

/**
 * \brief               Start Glossy and stall all other application tasks.
 *
 * \param initiator_id  Node ID of the flood initiator.
 * \param payload       A pointer to the data.
 *                      At the initiator, Glossy reads from the given memory
 *                      location data provided by the application.
 *                      At a receiver, Glossy writes to the given memory
 *                      location data for the application.
 * \param payload_len   Length of the flooding data, in bytes.
 * \param n_tx_max      Maximum number of transmissions (N).
 * \param with_sync     Not zero if Glossy must provide time synchronization,
 *                      zero otherwise.
 * \param dco_cal       Non-zero value => do DCO calibration.
 */
void glossy_start(uint16_t initiator_id,
                  uint8_t *payload,
                  uint8_t payload_len,
                  uint8_t n_tx_max,
                  uint8_t with_sync,
                  uint8_t dco_cal);

/**
 * \brief            Stop Glossy and resume all other application tasks.
 * \returns          Number of times the packet has been received during
 *                   last Glossy phase.
 *                   If it is zero, the packet was not successfully received.
 */
uint8_t glossy_stop(void);

/**
 * \brief            Get the last received counter.
 * \returns          Number of times the packet has been received during
 *                   last Glossy phase.
 *                   If it is zero, the packet was not successfully received.
 */
uint8_t glossy_get_rx_cnt(void);

/**
 * \brief            Get the number of started receptions
 * \returns          Number of times (preamble + sync detection)
 *                   has been received during the last flood
 */
uint8_t glossy_get_rx_try_cnt(void);

/**
 * \brief            Get the current Glossy state.
 * \return           Current Glossy state, one of the possible values
 *                   of \link glossy_state \endlink.
 */
uint8_t glossy_get_state(void);

/**
 * \brief            Get the last relay counter.
 * \returns          Value of the relay counter embedded in the first packet
 *                   received during the last Glossy phase.
 */
uint8_t glossy_get_relay_cnt(void);

/**
 * \brief            Provide information about current synchronization status.
 * \returns          Not zero if the synchronization reference time was
 *                   updated during the last Glossy phase, zero otherwise.
 */
uint8_t glossy_is_t_ref_updated(void);

/**
 * \brief            Get the sync reference time.
 * \returns          Timestamp in rtimer-ext clock ticks
 */
rtimer_ext_clock_t glossy_get_t_ref(void);

/**
 * \brief get the received payload length of the last flood
 */
uint8_t glossy_get_payload_len(void);

/**
 * \brief            get the average flood success rate
 * \returns          percentage * 100 of successful floods, i.e. at least 1
 *                   packet received
 */
uint16_t glossy_get_fsr(void);

/**
 * \brief            get the average packet error rate
 * \return           percentage * 100 of successfully received packets
 */
uint16_t glossy_get_per(void);

/**
 * \brief timer B callback function for RF interrupt handling
 */
void glossy_timer_int_cb(void);