#include "edf_tests/test_5.h"

#if ( configUSE_EDF == 1 )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"

#include "test_utils.h"

/* Constrianed deadlines (D != T) with higher utilization.
 *
 * Task set:
 *  - T1: C=1200, T=4000,  D=3000
 *  - T2: C=1500, T=6000,  D=5000
 *  - T3: C=1800, T=9000,  D=7000
 *  - T4: C=1200, T=12000, D=10000
 *
 * Suggested DBF check points (absolute deadlines):
 *  3000, 5000, 7000, 10000, 11000, 15000, 16000, 17000, 22000, ...
 */
#define T1_PERIOD_MS      4000u
#define T1_WCET_MS        1200u
#define T1_DEADLINE_MS    3000u

#define T2_PERIOD_MS      6000u
#define T2_WCET_MS        1500u
#define T2_DEADLINE_MS    5000u

#define T3_PERIOD_MS      9000u
#define T3_WCET_MS        1800u
#define T3_DEADLINE_MS    7000u

#define T4_PERIOD_MS      12000u
#define T4_WCET_MS        1200u
#define T4_DEADLINE_MS    10000u

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

static void Task4(void *pvParameters)
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    (void) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        spin_ms(T4_WCET_MS);

        xDelayResult = xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(T4_PERIOD_MS));

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void edf_5_run(void)
{
    stdio_init_all();
    vTraceTaskPinsInit();

    TaskHandle_t xT1 = NULL;
    TaskHandle_t xT2 = NULL;
    TaskHandle_t xT3 = NULL;
    TaskHandle_t xT4 = NULL;

    (void) xTaskCreate(Task1, "Test5 T1", 256, NULL, &xT1, T1_PERIOD_MS, T1_WCET_MS, T1_DEADLINE_MS);
    (void) xTaskCreate(Task2, "Test5 T2", 256, NULL, &xT2, T2_PERIOD_MS, T2_WCET_MS, T2_DEADLINE_MS);
    (void) xTaskCreate(Task3, "Test5 T3", 256, NULL, &xT3, T3_PERIOD_MS, T3_WCET_MS, T3_DEADLINE_MS);
    (void) xTaskCreate(Task4, "Test5 T4", 256, NULL, &xT4, T4_PERIOD_MS, T4_WCET_MS, T4_DEADLINE_MS);

    vTaskSetApplicationTaskTag(xT1, (TaskHookFunction_t) 1);
    vTaskSetApplicationTaskTag(xT2, (TaskHookFunction_t) 2);
    vTaskSetApplicationTaskTag(xT3, (TaskHookFunction_t) 4);
    vTaskSetApplicationTaskTag(xT4, (TaskHookFunction_t) 8);

    vTaskStartScheduler();

    for (;;)
    {
    }
}

#endif /* configUSE_EDF */
