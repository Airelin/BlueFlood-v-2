/**
 * Notes while programming:
 * Two frequencies on hardware:
 * 64MHz
 * 32.768 kHz
 * 
 * you kno what?
 * I'll just build this on the existing rtimers.
 * And hope that wont go wrong.
 * 
 */

// TODO: everything

#include "contiki.h"
#include "assert.h"
#include "rtimer-ext.h"
#include "nrf_timer.h"
#include "nrf_rtc.h"
#include "leds.h"

#include "sys/log.h"
#define LOG_MODULE "rtimer-ext"
#define LOG_LEVEL LOG_LEVEL_INFO

#define HF_TIMER NRF_TIMER1
#define LF_TIMER NRF_RTC1

rtimer_ext_clock_t hf_sw_ext = 0;
rtimer_ext_clock_t lf_sw_ext = 0;

rtimer_ext_t rtimers[NUM_OF_RTIMER_EXTS];


void hf_timer_activate()
{
  nrf_timer_int_enable(HF_TIMER, NRF_TIMER_INT_COMPARE1_MASK);
  rtimers[RTIMER_EXT_HF_0].state = RTIMER_EXT_SCHEDULED;
}
void hf_timer_deactivate()
{
  nrf_timer_int_disable(HF_TIMER, NRF_TIMER_INT_COMPARE1_MASK);
  rtimers[RTIMER_EXT_HF_0].state = RTIMER_EXT_INACTIVE;
}
void lf_timer_activate()
{
  nrf_rtc_int_enable(LF_TIMER, NRF_RTC_INT_COMPARE0_MASK);
  rtimers[RTIMER_EXT_LF_0].state = RTIMER_EXT_SCHEDULED;
}
void lf_timer_deactivate()
{
  nrf_timer_int_disable(HF_TIMER, NRF_RTC_INT_COMPARE0_MASK);
  rtimers[RTIMER_EXT_LF_0].state = RTIMER_EXT_INACTIVE;
}

void rtimer_ext_expired(rtimer_ext_t* rt)
{
  rt->state = RTIMER_EXT_JUST_EXPIRED;
  rt->func(rt);
  if (rt->period > 0)
  {
    // periodic, calculate next time
    rt->time = rt->time + rt-> period;
  }
}

void TIMER1_IRQHandler(void)
{
  LOG_DBG("Timer Interrupt\n");
  NVIC_ClearPendingIRQ(TIMER1_IRQn);
  if (nrf_timer_event_check(HF_TIMER, NRF_TIMER_EVENT_COMPARE0))
  {
    //overflow
    nrf_timer_event_clear(HF_TIMER, NRF_TIMER_EVENT_COMPARE0);
    hf_sw_ext++;
  } else if (nrf_timer_event_check(HF_TIMER, NRF_TIMER_EVENT_COMPARE1))
  {
    // timer may be triggered
    nrf_timer_event_clear(HF_TIMER, NRF_TIMER_EVENT_COMPARE1);
    rtimer_ext_t* rt = &rtimers[RTIMER_EXT_HF_0];
    if (rtimer_ext_now_hf() >= rt->time)
    {
      rtimer_ext_expired(rt);
      if (rt->period > 0)
      {
        // need to reschedule, new time has already been calculated
        nrf_timer_cc_write(HF_TIMER, NRF_TIMER_CC_CHANNEL1, (uint32_t) rt->time);
        rt->state = RTIMER_EXT_SCHEDULED;
      } else 
      {
        hf_timer_deactivate();
      }
    }
  }
}

void RTC1_IRQHandler(void)
{
  LOG_DBG("RTC Interrupt\n");
  NVIC_ClearPendingIRQ(RTC1_IRQn);
  if (nrf_rtc_event_pending(LF_TIMER, NRF_RTC_EVENT_OVERFLOW))
  {
    //overflow
    nrf_rtc_event_clear(LF_TIMER, NRF_RTC_EVENT_OVERFLOW);
    lf_sw_ext++;
  } else if (nrf_rtc_event_pending(LF_TIMER, NRF_RTC_EVENT_COMPARE_0))
  {
    // timer may be triggered
    nrf_rtc_event_clear(LF_TIMER, NRF_RTC_EVENT_COMPARE_0);
    rtimer_ext_t* rt = &rtimers[RTIMER_EXT_LF_0];
    if (rtimer_ext_now_lf() >= rt->time)
    {
      rtimer_ext_expired(rt);
      if (rt->period > 0)
      {
        // need to reschedule, new time has already been calculated
        nrf_rtc_cc_set(LF_TIMER, NRF_TIMER_CC_CHANNEL0, (uint32_t) rt->time);
        rt->state = RTIMER_EXT_SCHEDULED;
      } else 
      {
        lf_timer_deactivate();
      }
    }
  }
}

/**
 * @brief initialize the timers TA0 and TA1
 */
void rtimer_ext_init(void)
{
  nrf_timer_task_trigger(HF_TIMER, NRF_TIMER_TASK_STOP);
  nrf_timer_bit_width_set(HF_TIMER, NRF_TIMER_BIT_WIDTH_32);
  nrf_timer_frequency_set(HF_TIMER, NRF_TIMER_FREQ_16MHz);
  nrf_timer_mode_set(HF_TIMER, NRF_TIMER_MODE_TIMER);
  nrf_timer_cc_write(HF_TIMER, NRF_TIMER_CC_CHANNEL0, UINT32_MAX);
  nrf_timer_int_enable(HF_TIMER, NRF_TIMER_INT_COMPARE0_MASK);
  nrf_timer_task_trigger(HF_TIMER, NRF_TIMER_TASK_CLEAR);

  nrf_rtc_task_trigger(LF_TIMER, NRF_RTC_TASK_STOP);
  nrf_rtc_prescaler_set(LF_TIMER, 0);
  nrf_rtc_int_enable(LF_TIMER, NRF_RTC_INT_OVERFLOW_MASK);
  nrf_rtc_task_trigger(LF_TIMER, NRF_RTC_TASK_CLEAR);
  nrf_rtc_event_enable(LF_TIMER, NRF_RTC_INT_OVERFLOW_MASK |
  NRF_RTC_INT_COMPARE0_MASK);

  NVIC_ClearPendingIRQ(TIMER1_IRQn);
  NVIC_EnableIRQ(TIMER1_IRQn);
  NVIC_ClearPendingIRQ(RTC1_IRQn);
  NVIC_EnableIRQ(RTC1_IRQn);

  //TEST
  //nrf_rtc_prescaler_set(LF_TIMER, 32000);
  // nrf_rtc_int_enable(LF_TIMER, NRF_RTC_INT_TICK_MASK);
  // nrf_rtc_event_enable(LF_TIMER, NRF_RTC_INT_TICK_MASK);
  // LOG_DBG("EVTEN: %ld\n", LF_TIMER->EVTEN);

  hf_sw_ext = 0;
  lf_sw_ext = 0;


  nrf_rtc_task_trigger(LF_TIMER, NRF_RTC_TASK_START);
  nrf_timer_task_trigger(HF_TIMER, NRF_TIMER_TASK_START);
  LOG_INFO("Initialized timers\n");
}
/**
 * For some reason, rtimer_ext don't support void* argument to callback funcs
 * 
 * @brief schedule (start) an rtimer_ext
 * @param[in] timer the ID of the rtimer_ext, must by of type rtimer_ext_id_t
 * @param[in] start the expiration time (absolute counter value)
 * @param[in] period the period; set this to zero if you don't want this timer
 * trigger an interrupt periodically
 * @param[in] func the callback function; will be executed as soon as the timer
 * has expired
 */
void rtimer_ext_schedule(rtimer_ext_id_t timer,
                         rtimer_ext_clock_t start,
                         rtimer_ext_clock_t period,
                         rtimer_ext_callback_t func)
{
  LOG_DBG("Scheduling start: %lld, period: %lld\n", start, period);
  LOG_DBG("HF now: %lld, LF now: %lld\n", rtimer_ext_now_hf(), rtimer_ext_now_lf());

  if (timer > NUM_OF_RTIMER_EXTS) return; 
  rtimer_ext_t* rt = &rtimers[timer];
  if (rt->state == RTIMER_EXT_SCHEDULED) return;
  rt->period = period;
  rt->time = start + period;
  rt->func = func;

  if (timer == RTIMER_EXT_HF_0)
  {
    nrf_timer_cc_write(HF_TIMER, NRF_TIMER_CC_CHANNEL1, (uint32_t) rt->time);
    hf_timer_activate();
    LOG_DBG("Scheduled HF\n");
    rtimer_ext_clock_t now = rtimer_ext_now_hf();
    if (now >= rt->time && rt->state == RTIMER_EXT_SCHEDULED)
    {
      LOG_WARN("Scheduling happened in the past, (now: %lld, time: %lld)\n", now, rt->time);
      hf_timer_deactivate();
      rt->func(rt);
    }
  }
  else if (timer == RTIMER_EXT_LF_0)
  {
    nrf_rtc_cc_set(LF_TIMER, 0, (uint32_t) rt->time);
    lf_timer_activate();
    LOG_DBG("Scheduled LF\n");
    rtimer_ext_clock_t now = rtimer_ext_now_lf();
    if (now >= rt->time && rt->state == RTIMER_EXT_SCHEDULED)
    {
      LOG_WARN("Scheduling happened in the past, (now: %lld, time: %lld)\n", now, rt->time);
      lf_timer_deactivate();
      rt->func(rt);
    }
  }
}

/**
 * @brief set a CCR of a timer to event mode (capture input)
 */
void rtimer_ext_wait_for_event(rtimer_ext_id_t timer, rtimer_ext_callback_t func)
{
  // what is this even supposed to do?
  // no matter, noone uses it anyway
}

/**
 * @brief stop an rtimer_ext
 * @param[in] timer the ID of the rtimer_ext, must by of type rtimer_ext_id_t
 */
void rtimer_ext_stop(rtimer_ext_id_t timer)
{
  hf_timer_deactivate();
  lf_timer_deactivate();
}

/**
 * @brief resets the rtimer_ext values (both LF and HF) to zero
 */
void rtimer_ext_reset(void)
{
  nrf_timer_task_trigger(HF_TIMER, NRF_TIMER_TASK_CLEAR);
  nrf_rtc_task_trigger(LF_TIMER, NRF_RTC_TASK_CLEAR);
  hf_sw_ext = 0;
  lf_sw_ext = 0;
  // LOG_WARN("RESET1, LF: %lld, HF: %lld\n", rtimer_ext_now_lf(), rtimer_ext_now_hf());
  // LOG_WARN("RESET2, LF: %lld, HF: %lld\n", rtimer_ext_now_lf(), rtimer_ext_now_hf());
  // LOG_WARN("RESET3, LF: %lld, HF: %lld\n", rtimer_ext_now_lf(), rtimer_ext_now_hf());
}

/**
 * @brief get the current timer value of (high frequency)
 * @return timer value (32 bits)
 */
uint32_t rtimer_ext_now_hf_hw(void)
{
    nrf_timer_task_trigger(HF_TIMER, NRF_TIMER_TASK_CAPTURE2);
    return nrf_timer_cc_read(HF_TIMER, NRF_TIMER_CC_CHANNEL2); 
}

/**
 * @brief get the current timer value of (low frequency)
 * @return timer value (32 bits)
 */
uint32_t rtimer_ext_now_lf_hw(void)
{
    return nrf_rtc_counter_get(LF_TIMER);
}

/**
 * @brief get the current timer value of TA0 (high frequency)
 * @return timer value in timer clock ticks (timestamp)
 */
rtimer_ext_clock_t rtimer_ext_now_hf(void)
{
  // disable overflow int for now;
  nrf_timer_int_disable(HF_TIMER, NRF_TIMER_INT_COMPARE0_MASK);
  // take times
  //rtimer_ext_clock_t sw_time = hf_sw_ext;
  uint32_t hw_time = rtimer_ext_now_hf_hw();
  // did an interrupt happen in the mean time?
  if (nrf_timer_event_check(HF_TIMER, NRF_TIMER_EVENT_COMPARE0))
  {
    nrf_timer_event_clear(HF_TIMER, NRF_TIMER_EVENT_COMPARE0);
    // if yes, manual increment
    hf_sw_ext++;
    // and new hw time
    hw_time = rtimer_ext_now_hf_hw();
  }
  // compute total time
  rtimer_ext_clock_t res = hf_sw_ext << 32 | hw_time;
  // re-enable interrupt
  nrf_timer_int_enable(HF_TIMER, NRF_TIMER_INT_COMPARE0_MASK);
  return res;
}

/**
 * @brief get the current timer value of TA1 (low frequency) with SW extension
 * @return timestamp (64 bits)
 */
rtimer_ext_clock_t rtimer_ext_now_lf(void)
{
  // disable overflow int for now;
  nrf_rtc_int_disable(LF_TIMER, NRF_RTC_INT_OVERFLOW_MASK);
  // take times
  //rtimer_ext_clock_t sw_time = lf_sw_ext;
  uint32_t hw_time = rtimer_ext_now_lf_hw();
  // did an interrupt happen in the mean time?
  if (nrf_rtc_event_pending(LF_TIMER, NRF_RTC_EVENT_OVERFLOW))
  {
    nrf_rtc_event_clear(LF_TIMER, NRF_RTC_EVENT_OVERFLOW);
    // if yes, manual increment
    lf_sw_ext++;
    // and new hw time
    hw_time = rtimer_ext_now_lf_hw();
  }
  // compute total time
  rtimer_ext_clock_t res = lf_sw_ext << 24 | hw_time;
  // re-enable interrupt
  nrf_rtc_int_enable(LF_TIMER, NRF_RTC_INT_OVERFLOW_MASK);
  return res;
}


/**
 * @brief get the current timer value of both, the high and low frequency
 * timer
 * @param[in] hf_val value in timer clock ticks (timestamp)
 * @param[in] lf_val value in timer clock ticks (timestamp)
 * @remark This function will only work properly if the CPU is running on the
 * same clock source as the timer TA0 (HF) and this function can be executed
 * within one TA0 period (i.e. ~20ms @ 3.25 MHz).
 */
void rtimer_ext_now(rtimer_ext_clock_t* const hf_val, rtimer_ext_clock_t* const lf_val)
{
  // sky does some fany synchronization.
  // Let's hope this is good enough instead
  *hf_val = rtimer_ext_now_hf();
  *lf_val = rtimer_ext_now_lf();
}

/**
 * @brief get the address of the software extensions of the timer module
 * @param[in] timer the ID of an rtimer_ext
 * @return the address of the software extension
 */
rtimer_ext_clock_t* rtimer_ext_swext_addr(rtimer_ext_id_t timer)
{
    if (timer == RTIMER_EXT_HF_0)
    {
      return &hf_sw_ext;
    }
    else if (timer == RTIMER_EXT_LF_0)
    {
      return &lf_sw_ext;
    }
    else
    {
      return NULL;
    }
}

/**
 * @brief get the timestamp of the next scheduled expiration
 * @param[in] timer the ID of an rtimer_ext
 * @param[out] exp_time expiration time
 * @return 1 if the timer is still active, 0 otherwise
 */
uint8_t rtimer_ext_next_expiration(rtimer_ext_id_t timer, rtimer_ext_clock_t* exp_time)
{
    *exp_time = rtimers[timer].time;
    return rtimers[timer].state == RTIMER_EXT_SCHEDULED;
}

