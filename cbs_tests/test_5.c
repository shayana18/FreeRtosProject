#include "cbs_tests/test_5.h"

#include "FreeRTOS.h"

#if ( ( configUSE_UP == 1 ) && ( configUSE_EDF == 1 ) && ( configUSE_CBS == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "task.h"
#include "cbs.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * CBS Test 5:
 * - One normal periodic EDF task intentionally exceeds both WCET and deadline.
 * - One CBS server/worker pair runs short background jobs that stay within the
 *   reserved server budget.
 *
 * The goal is to confirm that ordinary periodic overrun and deadline-miss
 * handling still works correctly when CBS support is enabled.
 */

#define CBS5_STACK_DEPTH                256u

#define CBS5_PERIODIC_PERIOD_MS        6000u
#define CBS5_PERIODIC_DEADLINE_MS      1800u
#define CBS5_PERIODIC_WCET_MS           700u
#define CBS5_PERIODIC_WORK_MS          2200u

#define CBS5_SERVER_PERIOD_MS          5000u
#define CBS5_SERVER_BUDGET_MS          1200u
#define CBS5_SERVER_JOB_WORK_MS         350u
#define CBS5_SERVER_JOB_PERIOD_MS      5000u
#define CBS5_SERVER_JOB_OFFSET_MS      1000u
#define CBS5_SOURCE_DEADLINE_MS         100u

#define CBS5_TRACE_PERIODIC              1u
#define CBS5_TRACE_SERVER                2u
#define CBS5_TRACE_SOURCE                4u

static CBS_Server_t * pxCBS5Server = NULL;
static TaskHandle_t xCBS5PeriodicHandle = NULL;
static TaskHandle_t xCBS5WorkerHandle = NULL;
static TaskHandle_t xCBS5SourceHandle = NULL;
static TickType_t xCBS5AnchorTick = 0u;

static volatile uint32_t ulCBS5PeriodicReleases = 0u;
static volatile uint32_t ulCBS5ServerJobsCompleted = 0u;
static volatile uint32_t ulCBS5ServerSubmitFailures = 0u;

static void vCBS5PeriodicOverrunTask( void * pvParameters )
{
    TickType_t xLastWakeTime = xCBS5AnchorTick;

    ( void ) pvParameters;

    for( ;; )
    {
        ulCBS5PeriodicReleases++;
        spin_ms( CBS5_PERIODIC_WORK_MS );

        if( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( CBS5_PERIODIC_PERIOD_MS ) ) == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void vCBS5WorkerTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        if( xCBSWaitForJob( portMAX_DELAY ) == pdTRUE )
        {
            spin_ms( CBS5_SERVER_JOB_WORK_MS );
            ulCBS5ServerJobsCompleted++;
            configASSERT( xCBSCompleteJob() == pdPASS );

            vTraceFlushWcetOverrunEvents();
            vTraceFlushDeadlineMissEvents();
        }
    }
}

static void vCBS5SourceTask( void * pvParameters )
{
    TickType_t xLastWakeTime = xCBS5AnchorTick;

    ( void ) pvParameters;

    for( ;; )
    {
        if( ( pxCBS5Server != NULL ) && ( xCBS5WorkerHandle != NULL ) )
        {
            if( xCBSSubmitJob( pxCBS5Server, xCBS5WorkerHandle ) == pdFAIL )
            {
                ulCBS5ServerSubmitFailures++;
            }
        }

        vTraceFlushWcetOverrunEvents();
        vTraceFlushDeadlineMissEvents();
        vTraceUsbPrint( "[CBS5] periodic_releases=%lu server_jobs=%lu submit_failures=%lu server_deadline=%lu\r\n",
                        ( unsigned long ) ulCBS5PeriodicReleases,
                        ( unsigned long ) ulCBS5ServerJobsCompleted,
                        ( unsigned long ) ulCBS5ServerSubmitFailures,
                        ( unsigned long ) ( ( pxCBS5Server != NULL ) ? pxCBS5Server->xAbsDeadline : 0u ) );

        if( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( CBS5_SERVER_JOB_PERIOD_MS ) ) == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void cbs_5_run( void )
{
    stdio_init_all();
    vTraceTaskPinsInit();

    pxCBS5Server = NULL;
    xCBS5PeriodicHandle = NULL;
    xCBS5WorkerHandle = NULL;
    xCBS5SourceHandle = NULL;
    ulCBS5PeriodicReleases = 0u;
    ulCBS5ServerJobsCompleted = 0u;
    ulCBS5ServerSubmitFailures = 0u;

    xCBS5AnchorTick = xTaskGetTickCount();

    pxCBS5Server = xCBSServerCreate( pdMS_TO_TICKS( CBS5_SERVER_BUDGET_MS ),
                                     pdMS_TO_TICKS( CBS5_SERVER_PERIOD_MS ),
                                     "CBS5" );
    configASSERT( pxCBS5Server != NULL );

    configASSERT( xTaskCreate( vCBS5PeriodicOverrunTask,
                               "CBS5_PER",
                               CBS5_STACK_DEPTH,
                               NULL,
                               &xCBS5PeriodicHandle,
                               CBS5_PERIODIC_PERIOD_MS,
                               CBS5_PERIODIC_WCET_MS,
                               CBS5_PERIODIC_DEADLINE_MS ) == pdPASS );

    configASSERT( xTaskCreateCBSWorker( vCBS5WorkerTask,
                                        "CBS5_APER",
                                        CBS5_STACK_DEPTH,
                                        NULL,
                                        &xCBS5WorkerHandle,
                                        pxCBS5Server ) == pdPASS );

    configASSERT( xTaskCreate( vCBS5SourceTask,
                               "CBS5_SRC",
                               CBS5_STACK_DEPTH,
                               NULL,
                               &xCBS5SourceHandle,
                               CBS5_SERVER_JOB_PERIOD_MS,
                               50u,
                               CBS5_SOURCE_DEADLINE_MS ) == pdPASS );

    vTaskSetApplicationTaskTag( xCBS5PeriodicHandle, ( TaskHookFunction_t ) CBS5_TRACE_PERIODIC );
    vTaskSetApplicationTaskTag( xCBS5WorkerHandle, ( TaskHookFunction_t ) CBS5_TRACE_SERVER );
    vTaskSetApplicationTaskTag( xCBS5SourceHandle, ( TaskHookFunction_t ) CBS5_TRACE_SOURCE );

    vTraceUsbPrint( "[CBS5] periodic task should overrun WCET and miss its deadline; CBS worker should keep completing short jobs\r\n" );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#else

void cbs_5_run( void )
{
}

#endif
