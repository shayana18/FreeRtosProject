#include "edf_tests/test_7.h"

#if ( (configUSE_UP == 1) && ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"

#include "test_utils.h"

/* Admission control test for implicit deadlines (D = T).
 *
 * Because every task in this test has D = T, the kernel should take the
 * implicit-deadline admission path and use total utilization, not DBF.
 *
 * Baseline (schedulable):
 *  - B1: C=1500, T=6000,  D=6000   => U = 0.25
 *  - B2: C=2000, T=9000,  D=9000   => U = 2/9
 *    Baseline total utilization = 17/36 ~= 0.4722
 *
 * Bad candidate (expected admission FAIL by utilization):
 *  - BAD: C=7000, T=12000, D=12000 => U = 7/12 ~= 0.5833
 *    Total utilization would become 19/18 ~= 1.0556 > 1.0, so admission should fail.
 *
 * Good candidate (expected admission PASS by utilization):
 *  - GOOD: C=5000, T=12000, D=12000 => U = 5/12 ~= 0.4167
 *    Total utilization becomes 8/9 ~= 0.8889 <= 1.0, so admission should pass.
 */

#define STACK_DEPTH         256u

#define B1_PERIOD_MS        6000u
#define B1_WCET_MS          1500u
#define B1_DEADLINE_MS      B1_PERIOD_MS

#define B2_PERIOD_MS        9000u
#define B2_WCET_MS          2000u
#define B2_DEADLINE_MS      B2_PERIOD_MS

#define BAD_PERIOD_MS       12000u
#define BAD_WCET_MS         7000u
#define BAD_DEADLINE_MS     BAD_PERIOD_MS

#define GOOD_PERIOD_MS      12000u
#define GOOD_WCET_MS        5000u
#define GOOD_DEADLINE_MS    GOOD_PERIOD_MS

static volatile BaseType_t xBadCreateResult = pdFAIL;
static volatile BaseType_t xGoodCreateResult = pdFAIL;

static void BaselineTask1( void * pvParameters )
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    ( void ) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for( ;; )
    {
        spin_ms( B1_WCET_MS );

        xDelayResult = xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( B1_PERIOD_MS ) );

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void BaselineTask2( void * pvParameters )
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    ( void ) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for( ;; )
    {
        spin_ms( B2_WCET_MS );

        xDelayResult = xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( B2_PERIOD_MS ) );

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void BadTask( void * pvParameters )
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    ( void ) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for( ;; )
    {
        spin_ms( BAD_WCET_MS );

        xDelayResult = xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( BAD_PERIOD_MS ) );

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void GoodTask( void * pvParameters )
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    ( void ) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for( ;; )
    {
        spin_ms( GOOD_WCET_MS );

        xDelayResult = xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( GOOD_PERIOD_MS ) );

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void edf_7_run( void )
{
    stdio_init_all();
    vTraceTaskPinsInit();

    TaskHandle_t xB1 = NULL;
    TaskHandle_t xB2 = NULL;
    TaskHandle_t xBadTaskHandle = NULL;
    TaskHandle_t xGoodTaskHandle = NULL;

    ( void ) xTaskCreate( BaselineTask1, "Test7 B1", STACK_DEPTH, NULL, &xB1, B1_PERIOD_MS, B1_WCET_MS, B1_DEADLINE_MS );
    ( void ) xTaskCreate( BaselineTask2, "Test7 B2", STACK_DEPTH, NULL, &xB2, B2_PERIOD_MS, B2_WCET_MS, B2_DEADLINE_MS );

    vTaskSetApplicationTaskTag( xB1, ( TaskHookFunction_t ) 1 );
    vTaskSetApplicationTaskTag( xB2, ( TaskHookFunction_t ) 2 );

    /* Because all deadlines are implicit here, admission should be decided by utilization. */
    xBadCreateResult = xTaskCreate( BadTask, "Test7 Bad", STACK_DEPTH, NULL, &xBadTaskHandle, BAD_PERIOD_MS, BAD_WCET_MS, BAD_DEADLINE_MS );

    if( xBadCreateResult == pdFAIL )
    {
        /* Retry with a lower-utilization candidate that should now be admitted. */
        xGoodCreateResult = xTaskCreate( GoodTask, "Test7 Good", STACK_DEPTH, NULL, &xGoodTaskHandle, GOOD_PERIOD_MS, GOOD_WCET_MS, GOOD_DEADLINE_MS );

        if( ( xGoodCreateResult == pdPASS ) && ( xGoodTaskHandle != NULL ) )
        {
            vTaskSetApplicationTaskTag( xGoodTaskHandle, ( TaskHookFunction_t ) 4 );
        }
    }
    else if( xBadTaskHandle != NULL )
    {
        /* If this toggles, admission control on the implicit-deadline path is wrong. */
        vTaskSetApplicationTaskTag( xBadTaskHandle, ( TaskHookFunction_t ) 8 );
    }

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#endif /* configUSE_EDF */
