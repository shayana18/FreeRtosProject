#include "cbs_tests/test_4.h"

#include "FreeRTOS.h"

#if ( ( configUSE_UP == 1 ) && ( configUSE_EDF == 1 ) && ( configUSE_CBS == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "task.h"
#include "cbs.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * CBS Test 4:
 * - One normal periodic EDF task intentionally exceeds both WCET and deadline.
 * - One CBS server/worker pair runs short background jobs that stay within the
 *   reserved server budget.
 *
 * The goal is to confirm that ordinary periodic overrun and deadline-miss
 * handling still works correctly when CBS support is enabled.
 */

#define CBS4_STACK_DEPTH                256u

#define CBS4_PERIODIC_PERIOD_MS        6000u
#define CBS4_PERIODIC_DEADLINE_MS      1800u
#define CBS4_PERIODIC_WCET_MS           700u
#define CBS4_PERIODIC_WORK_MS          2200u

#define CBS4_SERVER_PERIOD_MS          5000u
#define CBS4_SERVER_BUDGET_MS          1200u
#define CBS4_SERVER_JOB_WORK_MS         350u
#define CBS4_SERVER_JOB_PERIOD_MS      5000u
#define CBS4_SERVER_JOB_OFFSET_MS      1000u
#define CBS4_SOURCE_DEADLINE_MS         100u

#define CBS4_TRACE_PERIODIC              1u
#define CBS4_TRACE_SERVER                2u
#define CBS4_TRACE_SOURCE                4u

static CBS_Server_t * pxCBS4Server = NULL;
static TaskHandle_t xCBS4PeriodicHandle = NULL;
static TaskHandle_t xCBS4WorkerHandle = NULL;
static TaskHandle_t xCBS4SourceHandle = NULL;
static TickType_t xCBS4AnchorTick = 0u;

static volatile uint32_t ulCBS4PeriodicReleases = 0u;
static volatile uint32_t ulCBS4ServerJobsCompleted = 0u;
static volatile uint32_t ulCBS4ServerSubmitFailures = 0u;

static void vCBS4PeriodicOverrunTask( void * pvParameters )
{
    TickType_t xLastWakeTime = xCBS4AnchorTick;

    ( void ) pvParameters;

    for( ;; )
    {
        ulCBS4PeriodicReleases++;
        spin_ms( CBS4_PERIODIC_WORK_MS );

        if( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( CBS4_PERIODIC_PERIOD_MS ) ) == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void vCBS4WorkerTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        if( xCBSWaitForJob( portMAX_DELAY ) == pdTRUE )
        {
            spin_ms( CBS4_SERVER_JOB_WORK_MS );
            ulCBS4ServerJobsCompleted++;
            configASSERT( xCBSCompleteJob() == pdPASS );

            vTraceFlushWcetOverrunEvents();
            vTraceFlushDeadlineMissEvents();
        }
    }
}

static void vCBS4SourceTask( void * pvParameters )
{
    TickType_t xLastWakeTime = xCBS4AnchorTick;

    ( void ) pvParameters;

    for( ;; )
    {
        if( ( pxCBS4Server != NULL ) && ( xCBS4WorkerHandle != NULL ) )
        {
            if( xCBSSubmitJob( pxCBS4Server, xCBS4WorkerHandle ) == pdFAIL )
            {
                ulCBS4ServerSubmitFailures++;
            }
        }

        vTraceFlushWcetOverrunEvents();
        vTraceFlushDeadlineMissEvents();
        vTraceUsbPrint( "[CBS4] periodic_releases=%lu server_jobs=%lu submit_failures=%lu server_deadline=%lu\r\n",
                        ( unsigned long ) ulCBS4PeriodicReleases,
                        ( unsigned long ) ulCBS4ServerJobsCompleted,
                        ( unsigned long ) ulCBS4ServerSubmitFailures,
                        ( unsigned long ) ( ( pxCBS4Server != NULL ) ? pxCBS4Server->xAbsDeadline : 0u ) );

        if( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( CBS4_SERVER_JOB_PERIOD_MS ) ) == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void cbs_4_run( void )
{
    stdio_init_all();
    vTraceTaskPinsInit();

    pxCBS4Server = NULL;
    xCBS4PeriodicHandle = NULL;
    xCBS4WorkerHandle = NULL;
    xCBS4SourceHandle = NULL;
    ulCBS4PeriodicReleases = 0u;
    ulCBS4ServerJobsCompleted = 0u;
    ulCBS4ServerSubmitFailures = 0u;

    xCBS4AnchorTick = xTaskGetTickCount();

    pxCBS4Server = xCBSServerCreate( pdMS_TO_TICKS( CBS4_SERVER_BUDGET_MS ),
                                     pdMS_TO_TICKS( CBS4_SERVER_PERIOD_MS ),
                                     "CBS4" );
    configASSERT( pxCBS4Server != NULL );

    configASSERT( xTaskCreate( vCBS4PeriodicOverrunTask,
                               "CBS4_PER",
                               CBS4_STACK_DEPTH,
                               NULL,
                               &xCBS4PeriodicHandle,
                               CBS4_PERIODIC_PERIOD_MS,
                               CBS4_PERIODIC_WCET_MS,
                               CBS4_PERIODIC_DEADLINE_MS ) == pdPASS );

    configASSERT( xTaskCreateCBSWorker( vCBS4WorkerTask,
                                        "CBS4_APER",
                                        CBS4_STACK_DEPTH,
                                        NULL,
                                        &xCBS4WorkerHandle,
                                        pxCBS4Server ) == pdPASS );

    configASSERT( xTaskCreate( vCBS4SourceTask,
                               "CBS4_SRC",
                               CBS4_STACK_DEPTH,
                               NULL,
                               &xCBS4SourceHandle,
                               CBS4_SERVER_JOB_PERIOD_MS,
                               50u,
                               CBS4_SOURCE_DEADLINE_MS ) == pdPASS );

    vTaskSetApplicationTaskTag( xCBS4PeriodicHandle, ( TaskHookFunction_t ) CBS4_TRACE_PERIODIC );
    vTaskSetApplicationTaskTag( xCBS4WorkerHandle, ( TaskHookFunction_t ) CBS4_TRACE_SERVER );
    vTaskSetApplicationTaskTag( xCBS4SourceHandle, ( TaskHookFunction_t ) CBS4_TRACE_SOURCE );

    vTraceUsbPrint( "[CBS4] periodic task should overrun WCET and miss its deadline; CBS worker should keep completing short jobs\r\n" );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#else

void cbs_4_run( void )
{
}

#endif
