#include "edf_tests/test_4.h"

#if ( ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"

#include "test_utils.h"

/* Constrained deadlines (D != T) with moderate utilization.
 *
 * Task set:
 *  - T1: C=1000, T=5000, D=3000
 *  - T2: C=1500, T=7000, D=3500
 *  - T3: C=1000, T=10000, D=8000
 *
 * Suggested DBF check points (absolute deadlines):
 *  3000, 3500, 8000, 13000, 16000, 18000, ...
 */
#define T1_PERIOD_MS      5000u
#define T1_WCET_MS        1000u
#define T1_DEADLINE_MS    3000u

#define T2_PERIOD_MS      7000u
#define T2_WCET_MS        1500u
#define T2_DEADLINE_MS    3500u

#define T3_PERIOD_MS      10000u
#define T3_WCET_MS        1000u
#define T3_DEADLINE_MS    8000u

static void Task1(void *pvParameters)
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    (void) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        spin_ms(T1_WCET_MS);

        xDelayResult = xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(T1_PERIOD_MS));

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void Task2(void *pvParameters)
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    (void) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        spin_ms(T2_WCET_MS);

        xDelayResult = xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(T2_PERIOD_MS));

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void Task3(void *pvParameters)
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    (void) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        spin_ms(T3_WCET_MS);

        xDelayResult = xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(T3_PERIOD_MS));

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void edf_4_run(void)
{
    stdio_init_all();
    vTraceTaskPinsInit();

    TaskHandle_t xT1 = NULL;
    TaskHandle_t xT2 = NULL;
    TaskHandle_t xT3 = NULL;

    (void) xTaskCreate(Task1, "Test4 T1", 256, NULL, &xT1, T1_PERIOD_MS, T1_WCET_MS, T1_DEADLINE_MS);
    (void) xTaskCreate(Task2, "Test4 T2", 256, NULL, &xT2, T2_PERIOD_MS, T2_WCET_MS, T2_DEADLINE_MS);
    (void) xTaskCreate(Task3, "Test4 T3", 256, NULL, &xT3, T3_PERIOD_MS, T3_WCET_MS, T3_DEADLINE_MS);

    vTaskSetApplicationTaskTag(xT1, (TaskHookFunction_t) 1);
    vTaskSetApplicationTaskTag(xT2, (TaskHookFunction_t) 2);
    vTaskSetApplicationTaskTag(xT3, (TaskHookFunction_t) 4);

    vTaskStartScheduler();

    for (;;)
    {
    }
}

#endif /* configUSE_EDF */
