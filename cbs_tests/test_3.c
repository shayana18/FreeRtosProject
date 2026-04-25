#include "cbs_tests/test_3.h"

#include "FreeRTOS.h"

#if ( ( configUSE_EDF == 1 ) && ( configUSE_CBS == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "task.h"
#include "cbs.h"

#include "test_utils.h"
#include "task_trace.h"

/* Larger scales for easier tracing. */
#define CBS3_SERVER_PERIOD_MS            7000u
#define CBS3_SERVER_BUDGET_MS            2200u
#define CBS3_PERIODIC_PERIOD_MS          7000u
#define CBS3_PERIODIC_WORK_MS            1200u
#define CBS3_APERIODIC_WORK_MS            900u
#define CBS3_TIE_TASK_REL_DEADLINE_MS     100u

/* Observation window after tie-trigger submission. */
#define CBS3_TIE_OBSERVE_MS               120u

#define CBS3_PERIODIC_STACK_WORDS         256u
#define CBS3_WORKER_STACK_WORDS           256u
#define CBS3_ARRIVAL_STACK_WORDS          220u

#define CBS3_WINNER_NONE                    0u
#define CBS3_WINNER_PERIODIC                1u
#define CBS3_WINNER_CBS                     2u

static volatile uint32_t ulCBS3PeriodicBeats;
static volatile uint32_t ulCBS3AperiodicJobsDone;
static volatile uint32_t ulCBS3SubmitFailures;

static volatile TickType_t xCBS3PeriodicAnchorTick;
static volatile BaseType_t xCBS3TieWindowOpen;
static volatile uint32_t ulCBS3TieWinner;
static volatile uint32_t ulCBS3TieAttempts;
static volatile uint32_t ulCBS3TieCBSWins;

static TaskHandle_t xCBS3PeriodicHandle;
static TaskHandle_t xCBS3WorkerHandle;
static TaskHandle_t xCBS3TieArrivalHandle;
static CBS_Server_t * pxCBS3Server;

static BaseType_t prvCBS3TrySubmit( void )
{
    if( ( pxCBS3Server != NULL ) && ( xCBS3WorkerHandle != NULL ) )
    {
        if( xCBSSubmitJob( pxCBS3Server, xCBS3WorkerHandle ) == pdPASS )
        {
            return pdPASS;
        }

        ulCBS3SubmitFailures++;
    }

    return pdFAIL;
}

static void vCBS3PeriodicTask( void * pvParameters )
{
    TickType_t xLastWake;
    TickType_t xPeriodicReleaseCount = ( TickType_t ) 0U;

    ( void ) pvParameters;
    xLastWake = xTaskGetTickCount();

    if( xCBS3PeriodicAnchorTick == ( TickType_t ) 0U )
    {
        xCBS3PeriodicAnchorTick = xLastWake;

        if( xCBS3TieArrivalHandle != NULL )
        {
            ( void ) xTaskNotifyGive( xCBS3TieArrivalHandle );
        }
    }

    for( ;; )
    {
        if( ( xCBS3TieWindowOpen != pdFALSE ) && ( ulCBS3TieWinner == CBS3_WINNER_NONE ) )
        {
            ulCBS3TieWinner = CBS3_WINNER_PERIODIC;
        }

        spin_ms( CBS3_PERIODIC_WORK_MS );
        ulCBS3PeriodicBeats++;
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( CBS3_PERIODIC_PERIOD_MS ) );
        xPeriodicReleaseCount++;

        vTraceUsbPrint( "CBS3_PER deadline update: release=%lu now=%lu next_deadline~=%lu\r\n",
                        ( unsigned long ) xPeriodicReleaseCount,
                        ( unsigned long ) xTaskGetTickCount(),
                        ( unsigned long ) ( xLastWake + pdMS_TO_TICKS( CBS3_PERIODIC_PERIOD_MS ) ) );
    }
}

static void vCBS3WorkerTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        if( xCBSWaitForJob( portMAX_DELAY ) == pdTRUE )
        {
            if( ( xCBS3TieWindowOpen != pdFALSE ) && ( ulCBS3TieWinner == CBS3_WINNER_NONE ) )
            {
                ulCBS3TieWinner = CBS3_WINNER_CBS;
            }

            spin_ms( CBS3_APERIODIC_WORK_MS );
            ulCBS3AperiodicJobsDone++;
            ( void ) xCBSCompleteJob();
        }
    }
}

static void vCBS3TieArrivalTask( void * pvParameters )
{
    TickType_t xLastWake;
    TickType_t xTieReleaseCount = ( TickType_t ) 0U;

    ( void ) pvParameters;

    /* Wait until the periodic task sets the common release anchor. */
    ( void ) ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

    /* Lock tie arrivals to the same period anchor as the periodic task so the
     * first arrival and subsequent arrivals share release/deadline phase. */
    xLastWake = xCBS3PeriodicAnchorTick;

    for( ;; )
    {
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( CBS3_SERVER_PERIOD_MS ) );
        ulCBS3TieAttempts++;

        ulCBS3TieWinner = CBS3_WINNER_NONE;
        xCBS3TieWindowOpen = pdTRUE;

        configASSERT( prvCBS3TrySubmit() == pdPASS );
        xTieReleaseCount++;
        vTraceUsbPrint( "CBS3_TIE deadline update: release=%lu now=%lu next_deadline~=%lu\r\n",
                ( unsigned long ) xTieReleaseCount,
                ( unsigned long ) xTaskGetTickCount(),
                ( unsigned long ) ( xLastWake + pdMS_TO_TICKS( CBS3_SERVER_PERIOD_MS ) ) );

        vTaskDelay( pdMS_TO_TICKS( CBS3_TIE_OBSERVE_MS ) );

        /* At equal deadlines, CBS must win tie-break over periodic EDF tasks. */
        configASSERT( ulCBS3TieWinner == CBS3_WINNER_CBS );
        ulCBS3TieCBSWins++;

        xCBS3TieWindowOpen = pdFALSE;
    }
}

void cbs_3_run( void )
{
    BaseType_t xCreateResult;

    xCBS3PeriodicHandle = NULL;
    xCBS3WorkerHandle = NULL;
    xCBS3TieArrivalHandle = NULL;
    pxCBS3Server = NULL;

    ulCBS3PeriodicBeats = 0u;
    ulCBS3AperiodicJobsDone = 0u;
    ulCBS3SubmitFailures = 0u;
    xCBS3PeriodicAnchorTick = 0u;
    xCBS3TieWindowOpen = pdFALSE;
    ulCBS3TieWinner = CBS3_WINNER_NONE;
    ulCBS3TieAttempts = 0u;
    ulCBS3TieCBSWins = 0u;

    stdio_init_all();
    vTraceTaskPinsInit();

    pxCBS3Server = xCBSServerCreate( pdMS_TO_TICKS( CBS3_SERVER_BUDGET_MS ),
                                     pdMS_TO_TICKS( CBS3_SERVER_PERIOD_MS ),
                                     "CBS3" );
    configASSERT( pxCBS3Server != NULL );

    xCreateResult = xTaskCreate( vCBS3PeriodicTask,
                                 "CBS3_PER",
                                 CBS3_PERIODIC_STACK_WORDS,
                                 NULL,
                                 &xCBS3PeriodicHandle,
                                 CBS3_PERIODIC_PERIOD_MS,
                                 CBS3_PERIODIC_WORK_MS,
                                 CBS3_PERIODIC_PERIOD_MS );
    configASSERT( xCreateResult == pdPASS );

    xCreateResult = xTaskCreateCBSWorker( vCBS3WorkerTask,
                                          "CBS3_APER",
                                          CBS3_WORKER_STACK_WORDS,
                                          NULL,
                                          &xCBS3WorkerHandle,
                                          pxCBS3Server );
    configASSERT( xCreateResult == pdPASS );

    xCreateResult = xTaskCreate( vCBS3TieArrivalTask,
                                 "CBS3_TIE",
                                 CBS3_ARRIVAL_STACK_WORDS,
                                 NULL,
                                 &xCBS3TieArrivalHandle,
                                 CBS3_SERVER_PERIOD_MS,
                                 20u,
                                 CBS3_TIE_TASK_REL_DEADLINE_MS );
    configASSERT( xCreateResult == pdPASS );

    if( xCBS3PeriodicHandle != NULL )
    {
        vTaskSetApplicationTaskTag( xCBS3PeriodicHandle, ( TaskHookFunction_t ) 1 );
    }

    if( xCBS3WorkerHandle != NULL )
    {
        vTaskSetApplicationTaskTag( xCBS3WorkerHandle, ( TaskHookFunction_t ) 2 );
    }

    if( xCBS3TieArrivalHandle != NULL )
    {
        vTaskSetApplicationTaskTag( xCBS3TieArrivalHandle, ( TaskHookFunction_t ) 4 );
    }

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