#include "cbs_tests/test_3.h"

#include "FreeRTOS.h"

#if ( ( configUSE_EDF == 1 ) && ( configUSE_CBS == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "task.h"
#include "cbs.h"

#include "test_utils.h"
#include "task_trace.h"

/* Large timing scales for easier timeline tracing in debugger/logic analyzer. */
#define CBS3_SERVER_PERIOD_MS            8000u
#define CBS3_SERVER_BUDGET_MS            1000u
#define CBS3_INITIAL_ARRIVAL_OFFSET_MS   3500u
#define CBS3_FREQUENT_ARRIVAL_MS          200u

#define CBS3_WORK_SPARSE_MS               250u
#define CBS3_WORK_FREQUENT_MS            1200u

#define CBS3_PERIODIC_PERIOD_MS          6000u
#define CBS3_PERIODIC_WORK_MS            2200u

#define CBS3_PERIODIC_STACK_WORDS         256u
#define CBS3_WORKER_STACK_WORDS           256u
#define CBS3_ARRIVAL_STACK_WORDS          192u

static volatile uint32_t ulCBS3PeriodicBeats;
static volatile uint32_t ulCBS3AperiodicJobsDone;
static volatile uint32_t ulCBS3SubmitFailures;

/* Debug/verification probes for deadline behavior. */
static volatile TickType_t xCBS3ServerCreateTick;
static volatile TickType_t xCBS3SparseArrivalTick;
static volatile TickType_t xCBS3DeadlineAfterSparseSubmit;
static volatile TickType_t xCBS3DeadlineAfterExhaustion;
static volatile BaseType_t xCBS3ObservedExhaustionPush;

static TaskHandle_t xCBS3PeriodicHandle;
static TaskHandle_t xCBS3WorkerHandle;
static TaskHandle_t xCBS3ArrivalHandle;
static CBS_Server_t * pxCBS3Server;

static void vCBS3PeriodicTask( void * pvParameters )
{
    TickType_t xLastWake;

    ( void ) pvParameters;
    xLastWake = xTaskGetTickCount();

    for( ;; )
    {
        spin_ms( CBS3_PERIODIC_WORK_MS );
        ulCBS3PeriodicBeats++;
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( CBS3_PERIODIC_PERIOD_MS ) );
    }
}

static void vCBS3WorkerTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        if( xCBSWaitForJob( portMAX_DELAY ) == pdTRUE )
        {
            if( ulCBS3AperiodicJobsDone == 0u )
            {
                /* First sparse job intentionally leaves most budget unused. */
                spin_ms( CBS3_WORK_SPARSE_MS );
            }
            else
            {
                /* Frequent phase intentionally overruns the remaining budget. */
                spin_ms( CBS3_WORK_FREQUENT_MS );
            }

            ulCBS3AperiodicJobsDone++;
            ( void ) xCBSCompleteJob();
        }
    }
}

static void vCBS3ArrivalTask( void * pvParameters )
{
    TickType_t xLastWake;
    TickType_t xPrevDeadline;
    const TickType_t xPeriodTicks = pdMS_TO_TICKS( CBS3_SERVER_PERIOD_MS );

    ( void ) pvParameters;

    /* Sparse start: trigger the idle-arrival CBS reset path (deadline = arrival + T). */
    vTaskDelay( pdMS_TO_TICKS( CBS3_INITIAL_ARRIVAL_OFFSET_MS ) );

    xCBS3SparseArrivalTick = xTaskGetTickCount();

    configASSERT( pxCBS3Server != NULL );
    configASSERT( xCBS3WorkerHandle != NULL );

    configASSERT( xCBSSubmitJob( pxCBS3Server, xCBS3WorkerHandle ) == pdPASS );

    xCBS3DeadlineAfterSparseSubmit = pxCBS3Server->xAbsDeadline;

    /* Must match the idle-arrival CBS reset rule exactly. */
    configASSERT( xCBS3DeadlineAfterSparseSubmit == ( xCBS3SparseArrivalTick + xPeriodTicks ) );

    /* The sparse arrival offset is chosen so this is not an aligned period multiple
     * from creation tick, making the reset effect obvious in traces. */
    configASSERT( ( ( xCBS3DeadlineAfterSparseSubmit - xCBS3ServerCreateTick ) % xPeriodTicks ) != 0u );

    xLastWake = xTaskGetTickCount();
    xPrevDeadline = xCBS3DeadlineAfterSparseSubmit;

    for( ;; )
    {
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( CBS3_FREQUENT_ARRIVAL_MS ) );

        if( xCBSSubmitJob( pxCBS3Server, xCBS3WorkerHandle ) == pdFAIL )
        {
            ulCBS3SubmitFailures++;
        }

        /* Detect the tick-driven budget exhaustion push: deadline = previous deadline + T. */
        if( ( xCBS3ObservedExhaustionPush == pdFALSE ) &&
            ( pxCBS3Server->xAbsDeadline != xPrevDeadline ) )
        {
            xCBS3DeadlineAfterExhaustion = pxCBS3Server->xAbsDeadline;
            configASSERT( xCBS3DeadlineAfterExhaustion == ( xPrevDeadline + xPeriodTicks ) );
            xCBS3ObservedExhaustionPush = pdTRUE;
        }
    }
}

void cbs_3_run( void )
{
    BaseType_t xCreateResult;

    xCBS3PeriodicHandle = NULL;
    xCBS3WorkerHandle = NULL;
    xCBS3ArrivalHandle = NULL;
    pxCBS3Server = NULL;

    ulCBS3PeriodicBeats = 0u;
    ulCBS3AperiodicJobsDone = 0u;
    ulCBS3SubmitFailures = 0u;
    xCBS3ServerCreateTick = 0u;
    xCBS3SparseArrivalTick = 0u;
    xCBS3DeadlineAfterSparseSubmit = 0u;
    xCBS3DeadlineAfterExhaustion = 0u;
    xCBS3ObservedExhaustionPush = pdFALSE;

    stdio_init_all();
    vTraceTaskPinsInit();

    pxCBS3Server = xCBSServerCreate( pdMS_TO_TICKS( CBS3_SERVER_BUDGET_MS ),
                                     pdMS_TO_TICKS( CBS3_SERVER_PERIOD_MS ),
                                     "CBS3" );
    configASSERT( pxCBS3Server != NULL );
    configASSERT( pxCBS3Server->xPeriodTicks == pdMS_TO_TICKS( CBS3_SERVER_PERIOD_MS ) );
    configASSERT( pxCBS3Server->xCapacityTicks == pdMS_TO_TICKS( CBS3_SERVER_BUDGET_MS ) );

    xCBS3ServerCreateTick = xTaskGetTickCount();

    xCreateResult = xTaskCreate( vCBS3PeriodicTask,
                                 "CBS3_PER",
                                 CBS3_PERIODIC_STACK_WORDS,
                                 NULL,
                                 &xCBS3PeriodicHandle,
                                 CBS3_PERIODIC_PERIOD_MS,
                                 CBS3_PERIODIC_WORK_MS,
                                 CBS3_PERIODIC_PERIOD_MS );
    configASSERT( xCreateResult == pdPASS );
    configASSERT( xCBS3PeriodicHandle != NULL );

    xCreateResult = xTaskCreateCBSWorker( vCBS3WorkerTask,
                                          "CBS3_APER",
                                          CBS3_WORKER_STACK_WORDS,
                                          NULL,
                                          &xCBS3WorkerHandle,
                                          pxCBS3Server );
    configASSERT( xCreateResult == pdPASS );
    configASSERT( xCBS3WorkerHandle != NULL );

    xCreateResult = xTaskCreate( vCBS3ArrivalTask,
                                 "CBS3_SRC",
                                 CBS3_ARRIVAL_STACK_WORDS,
                                 NULL,
                                 &xCBS3ArrivalHandle,
                                 CBS3_FREQUENT_ARRIVAL_MS,
                                 20u,
                                 CBS3_FREQUENT_ARRIVAL_MS );
    configASSERT( xCreateResult == pdPASS );
    configASSERT( xCBS3ArrivalHandle != NULL );

    vTaskSetApplicationTaskTag( xCBS3PeriodicHandle, ( TaskHookFunction_t ) 1 );
    vTaskSetApplicationTaskTag( xCBS3WorkerHandle, ( TaskHookFunction_t ) 2 );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#else

void cbs_3_run( void )
{
}

#endif