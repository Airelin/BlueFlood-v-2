#include "nrf_delay.h"

void nrf_delay_ms(uint32_t ms_time)
{
    if (ms_time == 0)
    {
        return;
    }

    do {
        nrf_delay_us(1000);
    } while (--ms_time);
}