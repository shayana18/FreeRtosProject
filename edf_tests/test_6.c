#include "edf_tests/test_6.h"
#include "projdefs.h"

#if ( ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"

#include "test_utils.h"

/* Admission control test using constrained deadlines (D < T).
 *
 * Baseline (schedulable):
 *  - B1: C=1200, T=4000,  D=1600
 *  - B2: C=800,  T=8000,  D=2000
 *
 * Bad candidate (expected admission FAIL):
 *  - BAD: C=3200, T=10000, D=3600
 *    Quick DBF sanity check at t=3600:
 *      dbf_B1(3600)=1200, dbf_B2(3600)=800, dbf_BAD(3600)=3200 => 5200 > 3600 (fails)
 *
 * Good candidate (expected admission PASS):
 *  - GOOD: C=3200, T=10000, D=6400
 *    The task set hyperperiod is lcm(4000, 8000, 10000) = 40000 ms.
 */

#define STACK_DEPTH         256u
#define B1_PERIOD_MS        4000u
#define B1_WCET_MS          1200u
#define B1_DEADLINE_MS      1600u

#define B2_PERIOD_MS        8000u
#define B2_WCET_MS           800u
#define B2_DEADLINE_MS      2000u

#define BAD_PERIOD_MS      10000u
#define BAD_WCET_MS         3200u
#define BAD_DEADLINE_MS     3600u

#define GOOD_PERIOD_MS     10000u
#define GOOD_WCET_MS        3200u
#define GOOD_DEADLINE_MS    6400u /* Increased relative deadline so the candidate passes the DBF test. */

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


void edf_6_run(void)
{
    stdio_init_all();
    vTraceTaskPinsInit();

    TaskHandle_t xB1 = NULL;
    TaskHandle_t xB2 = NULL;
    TaskHandle_t xBadTaskHandle = NULL;
    TaskHandle_t xGoodTaskHandle = NULL; 


    (void) xTaskCreate(BaselineTask1, "Base 1", STACK_DEPTH, NULL, &xB1, B1_PERIOD_MS, B1_WCET_MS, B1_DEADLINE_MS);
    (void) xTaskCreate(BaselineTask2, "Base 2", STACK_DEPTH, NULL, &xB2, B2_PERIOD_MS, B2_WCET_MS, B2_DEADLINE_MS);

    vTaskSetApplicationTaskTag(xB1, (TaskHookFunction_t) 1);
    vTaskSetApplicationTaskTag(xB2, (TaskHookFunction_t) 2);

    /* Attempt to add an unschedulable task first, then a schedulable replacement. */
    if(xTaskCreate(BadTask, "Bad Task", STACK_DEPTH, NULL, &xBadTaskHandle, BAD_PERIOD_MS, BAD_WCET_MS, BAD_DEADLINE_MS) == pdFAIL)
    {
        /* If the bad task is rejected, try the schedulable candidate. */
        if(xTaskCreate(GoodTask, "Good Task", STACK_DEPTH, NULL, &xGoodTaskHandle, GOOD_PERIOD_MS, GOOD_WCET_MS, GOOD_DEADLINE_MS) == pdTRUE)
        {
            vTaskSetApplicationTaskTag(xGoodTaskHandle, (TaskHookFunction_t) 4U);
        }
    }
    else /* The bad task was accepted unexpectedly. */
    {
        vTaskSetApplicationTaskTag(xBadTaskHandle, (TaskHookFunction_t) 8U); /* Channel 4 should stay quiet unless admission test is faulty wrong. */
    }

    vTaskStartScheduler();

    for (;;)
    {
    }
}

#endif /* configUSE_EDF */
