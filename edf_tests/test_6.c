#include "edf_tests/test_6.h"

#if ( configUSE_EDF == 1 )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"

#include "edf_tests/test_utils.h"

/* Admission control test using explicit deadlines (D != T).
 *
 * Baseline (schedulable):
 *  - B1: C=800,  T=4000, D=2000
 *  - B2: C=1200, T=6000, D=4000
 *  - B3: C=1000, T=9000, D=7000
 *
 * Bad candidate (expected admission FAIL):
 *  - BAD: C=1500, T=5000, D=2000
 *    Quick DBF sanity check at t=2000:
 *      dbf_B1(2000)=800, dbf_BAD(2000)=1500 => 2300 > 2000 (fails)
 *
 * Good candidate (expected admission PASS):
 *  - GOOD: C=500,  T=8000, D=5000
 */
#define B1_PERIOD_MS        4000u
#define B1_WCET_MS          800u
#define B1_DEADLINE_MS      2000u

#define B2_PERIOD_MS        6000u
#define B2_WCET_MS          1200u
#define B2_DEADLINE_MS      4000u

#define B3_PERIOD_MS        9000u
#define B3_WCET_MS          1000u
#define B3_DEADLINE_MS      7000u

#define BAD_PERIOD_MS       5000u
#define BAD_WCET_MS         1500u
#define BAD_DEADLINE_MS     2000u

#define GOOD_PERIOD_MS      8000u
#define GOOD_WCET_MS        500u
#define GOOD_DEADLINE_MS    5000u

static volatile BaseType_t xBadCreateResultInitial = pdFAIL;
static volatile BaseType_t xBadCreateResultAfter10s = pdFAIL;
static volatile BaseType_t xGoodCreateResultAfter10s = pdFAIL;

static void BaselineTask1(void *pvParameters)
{
    (void) pvParameters;

    vTaskSetApplicationTaskTag(NULL, (TaskHookFunction_t) 1);

    for (;;)
    {
        spin_ms(B1_WCET_MS);
    }
}

static void BaselineTask2(void *pvParameters)
{
    (void) pvParameters;

    vTaskSetApplicationTaskTag(NULL, (TaskHookFunction_t) 2);

    for (;;)
    {
        spin_ms(B2_WCET_MS);
    }
}

static void BaselineTask3(void *pvParameters)
{
    (void) pvParameters;

    vTaskSetApplicationTaskTag(NULL, (TaskHookFunction_t) 4);

    for (;;)
    {
        spin_ms(B3_WCET_MS);
    }
}

static void BadTask(void *pvParameters)
{
    (void) pvParameters;

    vTaskSetApplicationTaskTag(NULL, (TaskHookFunction_t) 3);

    for (;;)
    {
        spin_ms(BAD_WCET_MS);
    }
}

static void GoodTask(void *pvParameters)
{
    (void) pvParameters;

    vTaskSetApplicationTaskTag(NULL, (TaskHookFunction_t) 8);

    for (;;)
    {
        spin_ms(GOOD_WCET_MS);
    }
}

static void AdmissionControllerTask(void *pvParameters)
{
    (void) pvParameters;

    vTaskSetApplicationTaskTag(NULL, (TaskHookFunction_t) 0);

    vTaskDelay(pdMS_TO_TICKS(10000u));

    {
        TaskHandle_t xBadHandle = NULL;
        xBadCreateResultAfter10s = xTaskCreate(BadTask, "Bad Task", 256, NULL, &xBadHandle, BAD_PERIOD_MS, BAD_WCET_MS, BAD_DEADLINE_MS);
        if( ( xBadCreateResultAfter10s == pdPASS ) && ( xBadHandle != NULL ) )
        {
            vTaskSetApplicationTaskTag(xBadHandle, (TaskHookFunction_t) 3);
        }
    }

    {
        TaskHandle_t xGoodHandle = NULL;
        xGoodCreateResultAfter10s = xTaskCreate(GoodTask, "Good Task", 256, NULL, &xGoodHandle, GOOD_PERIOD_MS, GOOD_WCET_MS, GOOD_DEADLINE_MS);
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

void test_6_run(void)
{
    stdio_init_all();
    vTraceTaskPinsInit();

    TaskHandle_t xB1 = NULL;
    TaskHandle_t xB2 = NULL;
    TaskHandle_t xB3 = NULL;

    (void) xTaskCreate(BaselineTask1, "Base 1", 256, NULL, &xB1, B1_PERIOD_MS, B1_WCET_MS, B1_DEADLINE_MS);
    (void) xTaskCreate(BaselineTask2, "Base 2", 256, NULL, &xB2, B2_PERIOD_MS, B2_WCET_MS, B2_DEADLINE_MS);
    (void) xTaskCreate(BaselineTask3, "Base 3", 256, NULL, &xB3, B3_PERIOD_MS, B3_WCET_MS, B3_DEADLINE_MS);

    vTaskSetApplicationTaskTag(xB1, (TaskHookFunction_t) 1);
    vTaskSetApplicationTaskTag(xB2, (TaskHookFunction_t) 2);
    vTaskSetApplicationTaskTag(xB3, (TaskHookFunction_t) 4);

    /* Attempt to add an unschedulable task (expected to fail DBF). */
    {
        TaskHandle_t xBadHandle = NULL;
        xBadCreateResultInitial = xTaskCreate(BadTask, "Bad Task", 256, NULL, &xBadHandle, BAD_PERIOD_MS, BAD_WCET_MS, BAD_DEADLINE_MS);
    }

    /* Controller will retry after ~10s, then add a schedulable task. */
    {
        TaskHandle_t xController = NULL;
        (void) xTaskCreate(AdmissionControllerTask, "Admit Ctrl", 256, NULL, &xController, 20000u, 10u, 15000u);
        vTaskSetApplicationTaskTag(xController, (TaskHookFunction_t) 0);
    }

    vTaskStartScheduler();

    for (;;)
    {
    }
}

#endif /* configUSE_EDF */
