#include "edf_tests/test_2.h"

#if ( (configUSE_UP == 1) && ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"

#include "test_utils.h"

/* Higher (but < 1.0) total utilization with implicit deadlines (D = T).
 * Harmonic periods make hand-verification easier.
 * Total utilization: 1/2 + 1/4 + 1/8 + 1/16 = 15/16 = 0.9375.
 */
#define T1_PERIOD_MS    2000u
#define T1_WCET_MS      1000u

#define T2_PERIOD_MS    4000u
#define T2_WCET_MS      1000u

#define T3_PERIOD_MS    8000u
#define T3_WCET_MS      1000u

#define T4_PERIOD_MS    16000u
#define T4_WCET_MS      1000u

static TickType_t xEdf2SharedAnchorTick = 0u;

static void Task1(void *pvParameters)
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    (void) pvParameters;
    xLastWakeTime = xEdf2SharedAnchorTick;

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
    xLastWakeTime = xEdf2SharedAnchorTick;

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
    xLastWakeTime = xEdf2SharedAnchorTick;

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
    xLastWakeTime = xEdf2SharedAnchorTick;

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

void edf_2_run(void)
{
    stdio_init_all();
    vTraceTaskPinsInit();
    xEdf2SharedAnchorTick = xTaskGetTickCount();

    TaskHandle_t xTask1Handle = NULL;
    TaskHandle_t xTask2Handle = NULL;
    TaskHandle_t xTask3Handle = NULL;
    TaskHandle_t xTask4Handle = NULL;

    (void) xTaskCreate(Task1, "Test2 Task 1", 256, NULL, &xTask1Handle, T1_PERIOD_MS, T1_WCET_MS, T1_PERIOD_MS);
    (void) xTaskCreate(Task2, "Test2 Task 2", 256, NULL, &xTask2Handle, T2_PERIOD_MS, T2_WCET_MS, T2_PERIOD_MS);
    (void) xTaskCreate(Task3, "Test2 Task 3", 256, NULL, &xTask3Handle, T3_PERIOD_MS, T3_WCET_MS, T3_PERIOD_MS);
    (void) xTaskCreate(Task4, "Test2 Task 4", 256, NULL, &xTask4Handle, T4_PERIOD_MS, T4_WCET_MS, T4_PERIOD_MS);

    vTaskSetApplicationTaskTag(xTask1Handle, (TaskHookFunction_t) 1);
    vTaskSetApplicationTaskTag(xTask2Handle, (TaskHookFunction_t) 2);
    vTaskSetApplicationTaskTag(xTask3Handle, (TaskHookFunction_t) 4);
    vTaskSetApplicationTaskTag(xTask4Handle, (TaskHookFunction_t) 8);

    vTaskStartScheduler();

    for (;;)
    {
    }
}

#endif /* configUSE_EDF */
