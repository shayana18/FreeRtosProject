#include "edf_tests/test_3.h"

#if ( ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"

#include "test_utils.h"

/* Hand-verifiable harmonic timing with implicit deadlines (D = T).
 * Baseline utilization: 1/2 + 1/4 + 1/10 = 0.85.
 */
#define B1_PERIOD_MS    2000u
#define B1_WCET_MS      1000u

#define B2_PERIOD_MS    4000u
#define B2_WCET_MS      1000u

#define B3_PERIOD_MS    8000u
#define B3_WCET_MS      800u

/* Candidate that would push U_total > 1.0: adds 1/5 => 1.05 (unschedulable). */
#define BAD_PERIOD_MS   2000u
#define BAD_WCET_MS     400u

/* Candidate that still keeps U_total < 1.0: adds 1/20 => 0.90 (schedulable). */
#define GOOD_PERIOD_MS  8000u
#define GOOD_WCET_MS    400u

static volatile BaseType_t xBadCreateResultInitial = pdFAIL;
static volatile BaseType_t xBadCreateResultAfter10s = pdFAIL;
static volatile BaseType_t xGoodCreateResultAfter10s = pdFAIL;

static void BaselineTask1(void *pvParameters)
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    (void) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        spin_ms(B1_WCET_MS);

        xDelayResult = xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(B1_PERIOD_MS));

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void BaselineTask2(void *pvParameters)
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    (void) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        spin_ms(B2_WCET_MS);

        xDelayResult = xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(B2_PERIOD_MS));

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void BaselineTask3(void *pvParameters)
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    (void) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        spin_ms(B3_WCET_MS);

        xDelayResult = xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(B3_PERIOD_MS));

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void BadTask(void *pvParameters)
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    (void) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        spin_ms(BAD_WCET_MS);

        xDelayResult = xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(BAD_PERIOD_MS));

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void GoodTask(void *pvParameters)
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    (void) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        spin_ms(GOOD_WCET_MS);

        xDelayResult = xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(GOOD_PERIOD_MS));

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void AdmissionControllerTask(void *pvParameters)
{
    (void) pvParameters;


    vTaskDelay(pdMS_TO_TICKS(10000u));

    {
        TaskHandle_t xBadHandle = NULL;
        xBadCreateResultAfter10s = xTaskCreate(BadTask, "Bad Task", 256, NULL, &xBadHandle, BAD_PERIOD_MS, BAD_WCET_MS, BAD_PERIOD_MS);
        if( ( xBadCreateResultAfter10s == pdPASS ) && ( xBadHandle != NULL ) )
        {
            vTaskSetApplicationTaskTag(xBadHandle, (TaskHookFunction_t) 3);
        }
    }

    {
        TaskHandle_t xGoodHandle = NULL;
        xGoodCreateResultAfter10s = xTaskCreate(GoodTask, "Good Task", 256, NULL, &xGoodHandle, GOOD_PERIOD_MS, GOOD_WCET_MS, GOOD_PERIOD_MS);
        if( ( xGoodCreateResultAfter10s == pdPASS ) && ( xGoodHandle != NULL ) )
        {
            vTaskSetApplicationTaskTag(xGoodHandle, (TaskHookFunction_t) 8);
        }
    }

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000u));
    }
}

void edf_3_run(void)
{
    stdio_init_all();
    vTraceTaskPinsInit();

    TaskHandle_t xB1 = NULL;
    TaskHandle_t xB2 = NULL;
    TaskHandle_t xB3 = NULL;

    (void) xTaskCreate(BaselineTask1, "Base 1", 256, NULL, &xB1, B1_PERIOD_MS, B1_WCET_MS, B1_PERIOD_MS);
    (void) xTaskCreate(BaselineTask2, "Base 2", 256, NULL, &xB2, B2_PERIOD_MS, B2_WCET_MS, B2_PERIOD_MS);
    (void) xTaskCreate(BaselineTask3, "Base 3", 256, NULL, &xB3, B3_PERIOD_MS, B3_WCET_MS, B3_PERIOD_MS);

    vTaskSetApplicationTaskTag(xB1, (TaskHookFunction_t) 1);
    vTaskSetApplicationTaskTag(xB2, (TaskHookFunction_t) 2);
    vTaskSetApplicationTaskTag(xB3, (TaskHookFunction_t) 4);

    /* Attempt to add an unschedulable task at the end (expected to fail). */
    {
        TaskHandle_t xBadHandle = NULL;
        xBadCreateResultInitial = xTaskCreate(BadTask, "Bad Task", 256, NULL, &xBadHandle, BAD_PERIOD_MS, BAD_WCET_MS, BAD_PERIOD_MS);
    }

    /* Controller will retry after ~10s, then add a schedulable task. */
    {
        TaskHandle_t xController = NULL;
        (void) xTaskCreate(AdmissionControllerTask, "Admit Ctrl", 256, NULL, &xController, 20000u, 10u, 20000u);
        vTaskSetApplicationTaskTag(xController, (TaskHookFunction_t) 64);
    }

    vTaskStartScheduler();

    for (;;)
    {
    }
}

#endif /* configUSE_EDF */
