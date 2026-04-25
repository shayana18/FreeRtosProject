#include "cbs_tests/test_2.h"

#include "FreeRTOS.h"

#if ( ( configUSE_EDF == 1 ) && ( configUSE_CBS == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "task.h"
#include "cbs.h"

#include "test_utils.h"
#include "task_trace.h"

/* Large timing scales for easier timeline tracing in debugger/logic analyzer. */
#define CBS2_SERVER_PERIOD_MS            8000u
#define CBS2_SERVER_BUDGET_MS            1000u
#define CBS2_INITIAL_ARRIVAL_OFFSET_MS   3500u
#define CBS2_FREQUENT_ARRIVAL_MS          200u

#define CBS2_WORK_SPARSE_MS               250u
#define CBS2_WORK_FREQUENT_MS            1200u

#define CBS2_PERIODIC_PERIOD_MS          6000u
#define CBS2_PERIODIC_WORK_MS            2200u

#define CBS2_PERIODIC_STACK_WORDS         256u
#define CBS2_WORKER_STACK_WORDS           256u
#define CBS2_ARRIVAL_STACK_WORDS          192u

static volatile uint32_t ulCBS2PeriodicBeats;
static volatile uint32_t ulCBS2AperiodicJobsDone;
static volatile uint32_t ulCBS2SubmitFailures;

/* Debug/verification probes for deadline behavior. */
static volatile TickType_t xCBS2ServerCreateTick;
static volatile TickType_t xCBS2SparseArrivalTick;
static volatile TickType_t xCBS2DeadlineAfterSparseSubmit;
static volatile TickType_t xCBS2DeadlineAfterExhaustion;
static volatile BaseType_t xCBS2ObservedExhaustionPush;

static TaskHandle_t xCBS2PeriodicHandle;
static TaskHandle_t xCBS2WorkerHandle;
static TaskHandle_t xCBS2ArrivalHandle;
static CBS_Server_t * pxCBS2Server;

static void vCBS2PeriodicTask( void * pvParameters )
{
    TickType_t xLastWake;

    ( void ) pvParameters;
    xLastWake = xTaskGetTickCount();

    for( ;; )
    {
        spin_ms( CBS2_PERIODIC_WORK_MS );
        ulCBS2PeriodicBeats++;
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( CBS2_PERIODIC_PERIOD_MS ) );
    }
}

static void vCBS2WorkerTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        if( xCBSWaitForJob( portMAX_DELAY ) == pdTRUE )
        {
            if( ulCBS2AperiodicJobsDone == 0u )
            {
                /* First sparse job intentionally leaves most budget unused. */
                spin_ms( CBS2_WORK_SPARSE_MS );
            }
            else
            {
                /* Frequent phase intentionally overruns the remaining budget. */
                spin_ms( CBS2_WORK_FREQUENT_MS );
            }

            ulCBS2AperiodicJobsDone++;
            ( void ) xCBSCompleteJob();
        }
    }
}

static void vCBS2ArrivalTask( void * pvParameters )
{
    TickType_t xLastWake;
    TickType_t xPrevDeadline;
    const TickType_t xPeriodTicks = pdMS_TO_TICKS( CBS2_SERVER_PERIOD_MS );

    ( void ) pvParameters;

    /* Sparse start: trigger the idle-arrival CBS reset path (deadline = arrival + T). */
    vTaskDelay( pdMS_TO_TICKS( CBS2_INITIAL_ARRIVAL_OFFSET_MS ) );

    xCBS2SparseArrivalTick = xTaskGetTickCount();

    configASSERT( pxCBS2Server != NULL );
    configASSERT( xCBS2WorkerHandle != NULL );

    configASSERT( xCBSSubmitJob( pxCBS2Server, xCBS2WorkerHandle ) == pdPASS );

    xCBS2DeadlineAfterSparseSubmit = pxCBS2Server->xAbsDeadline;

    /* Must match the idle-arrival CBS reset rule exactly. */
    configASSERT( xCBS2DeadlineAfterSparseSubmit == ( xCBS2SparseArrivalTick + xPeriodTicks ) );

    /* The sparse arrival offset is chosen so this is not an aligned period multiple
     * from creation tick, making the reset effect obvious in traces. */
    configASSERT( ( ( xCBS2DeadlineAfterSparseSubmit - xCBS2ServerCreateTick ) % xPeriodTicks ) != 0u );

    xLastWake = xTaskGetTickCount();
    xPrevDeadline = xCBS2DeadlineAfterSparseSubmit;

    for( ;; )
    {
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( CBS2_FREQUENT_ARRIVAL_MS ) );

        if( xCBSSubmitJob( pxCBS2Server, xCBS2WorkerHandle ) == pdFAIL )
        {
            ulCBS2SubmitFailures++;
        }

        /* Detect the tick-driven budget exhaustion push: deadline = previous deadline + T. */
        if( ( xCBS2ObservedExhaustionPush == pdFALSE ) &&
            ( pxCBS2Server->xAbsDeadline != xPrevDeadline ) )
        {
            xCBS2DeadlineAfterExhaustion = pxCBS2Server->xAbsDeadline;
            configASSERT( xCBS2DeadlineAfterExhaustion == ( xPrevDeadline + xPeriodTicks ) );
            xCBS2ObservedExhaustionPush = pdTRUE;
        }
    }
}

void cbs_2_run( void )
{
    BaseType_t xCreateResult;

    xCBS2PeriodicHandle = NULL;
    xCBS2WorkerHandle = NULL;
    xCBS2ArrivalHandle = NULL;
    pxCBS2Server = NULL;

    ulCBS2PeriodicBeats = 0u;
    ulCBS2AperiodicJobsDone = 0u;
    ulCBS2SubmitFailures = 0u;
    xCBS2ServerCreateTick = 0u;
    xCBS2SparseArrivalTick = 0u;
    xCBS2DeadlineAfterSparseSubmit = 0u;
    xCBS2DeadlineAfterExhaustion = 0u;
    xCBS2ObservedExhaustionPush = pdFALSE;

    stdio_init_all();
    vTraceTaskPinsInit();

    pxCBS2Server = xCBSServerCreate( pdMS_TO_TICKS( CBS2_SERVER_BUDGET_MS ),
                                     pdMS_TO_TICKS( CBS2_SERVER_PERIOD_MS ),
                                     "CBS2" );
    configASSERT( pxCBS2Server != NULL );
    configASSERT( pxCBS2Server->xPeriodTicks == pdMS_TO_TICKS( CBS2_SERVER_PERIOD_MS ) );
    configASSERT( pxCBS2Server->xCapacityTicks == pdMS_TO_TICKS( CBS2_SERVER_BUDGET_MS ) );

    xCBS2ServerCreateTick = xTaskGetTickCount();

    xCreateResult = xTaskCreate( vCBS2PeriodicTask,
                                 "CBS2_PER",
                                 CBS2_PERIODIC_STACK_WORDS,
                                 NULL,
                                 &xCBS2PeriodicHandle,
                                 CBS2_PERIODIC_PERIOD_MS,
                                 CBS2_PERIODIC_WORK_MS,
                                 CBS2_PERIODIC_PERIOD_MS );
    configASSERT( xCreateResult == pdPASS );
    configASSERT( xCBS2PeriodicHandle != NULL );

    xCreateResult = xTaskCreateCBSWorker( vCBS2WorkerTask,
                                          "CBS2_APER",
                                          CBS2_WORKER_STACK_WORDS,
                                          NULL,
                                          &xCBS2WorkerHandle,
                                          pxCBS2Server );
    configASSERT( xCreateResult == pdPASS );
    configASSERT( xCBS2WorkerHandle != NULL );

    xCreateResult = xTaskCreate( vCBS2ArrivalTask,
                                 "CBS2_SRC",
                                 CBS2_ARRIVAL_STACK_WORDS,
                                 NULL,
                                 &xCBS2ArrivalHandle,
                                 CBS2_FREQUENT_ARRIVAL_MS,
                                 20u,
                                 CBS2_FREQUENT_ARRIVAL_MS );
    configASSERT( xCreateResult == pdPASS );
    configASSERT( xCBS2ArrivalHandle != NULL );

    vTaskSetApplicationTaskTag( xCBS2PeriodicHandle, ( TaskHookFunction_t ) 1 );
    vTaskSetApplicationTaskTag( xCBS2WorkerHandle, ( TaskHookFunction_t ) 2 );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#else

void cbs_2_run( void )
{
}

#endif