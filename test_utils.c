#include "test_utils.h"

#include "pico/stdlib.h"

#if ( ( configUSE_UP == 1U ) && ( configUSE_EDF == 1U ) && ( configUSE_SRP == 1U ) && ( configUSE_SRP_SHARED_STACKS == 1U ) && ( configENABLE_TEST_SRP_STACK_REPORT == 1U ) )
    #include "FreeRTOS.h"
    #include "task.h"
    #include "task_trace.h"

    static TickType_t xNextStackReportTick = 0U;
    static BaseType_t xStackReportInitialized = pdFALSE;

    void vSRPReportStackUsageIfDue(void)
    {
        BaseType_t xPrintTheoretical = pdFALSE;
        BaseType_t xPrintPeriodic = pdFALSE;
        const TickType_t xNow = xTaskGetTickCount();

        taskENTER_CRITICAL();
        {
            if( xStackReportInitialized == pdFALSE )
            {
                xStackReportInitialized = pdTRUE;
                xNextStackReportTick = xNow + pdMS_TO_TICKS( 10000u );
                xPrintTheoretical = pdTRUE;
                xPrintPeriodic = pdTRUE;
            }
            else if( xNow >= xNextStackReportTick )
            {
                xNextStackReportTick += pdMS_TO_TICKS( 10000u );
                xPrintPeriodic = pdTRUE;
            }
        }
        taskEXIT_CRITICAL();

        if( ( xPrintTheoretical != pdFALSE ) || ( xPrintPeriodic != pdFALSE ) )
        {
            size_t uxCurrentSharedBytes = 0u;
            size_t uxCurrentNonSharedBytes = 0u;
            size_t uxMaxSharedBytes = 0u;
            size_t uxMaxNonSharedBytes = 0u;
            size_t uxTheoreticalSharedBytes = 0u;
            size_t uxTheoreticalNonSharedBytes = 0u;

            vTaskGetSRPStackUsageRuntimeStats( &uxCurrentSharedBytes,
                                               &uxCurrentNonSharedBytes,
                                               &uxMaxSharedBytes,
                                               &uxMaxNonSharedBytes,
                                               &uxTheoreticalSharedBytes,
                                               &uxTheoreticalNonSharedBytes );

            if( xPrintTheoretical != pdFALSE )
            {
                vTraceUsbPrint( "[SRP stack] theoretical: shared=%luB non-shared=%luB\r\n",
                                ( unsigned long ) uxTheoreticalSharedBytes,
                                ( unsigned long ) uxTheoreticalNonSharedBytes );
            }

            if( xPrintPeriodic != pdFALSE )
            {
                vTraceUsbPrint( "[SRP stack] current: shared=%luB non-shared=%luB | max: shared=%luB non-shared=%luB\r\n",
                                ( unsigned long ) uxCurrentSharedBytes,
                                ( unsigned long ) uxCurrentNonSharedBytes,
                                ( unsigned long ) uxMaxSharedBytes,
                                ( unsigned long ) uxMaxNonSharedBytes );
            }
        }
    }
#endif

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
