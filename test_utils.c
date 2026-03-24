#include "test_utils.h"

#if ( ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )

#include "pico/stdlib.h"

void spin_ms(uint32_t target_ms)
{
    uint32_t executed_us = 0;
    uint32_t prev_us = time_us_32();

    while (executed_us < target_ms * 1000u)
    {
        uint32_t now_us = time_us_32();
        uint32_t delta_us = now_us - prev_us;

        if (delta_us < 100u) // if delta is larger than 100u that means a preemption happened so we stop accumulaing 
        {
            executed_us += delta_us;
        }

        prev_us = now_us;
    }
}

#endif /* configUSE_EDF */
