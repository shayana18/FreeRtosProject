#include "cbs_tests/test_4.h"

#include "FreeRTOS.h"

#if ( ( configUSE_EDF == 1 ) && ( configUSE_CBS == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "task.h"
#include "cbs.h"

#include "test_utils.h"
#include "task_trace.h"

/* Larger scales for easier tracing. */
#define CBS4_SERVER_PERIOD_MS            7000u
#define CBS4_SERVER_BUDGET_MS            2200u
#define CBS4_PERIODIC_PERIOD_MS          7000u
#define CBS4_PERIODIC_WORK_MS            1200u
#define CBS4_APERIODIC_WORK_MS            900u
#define CBS4_TIE_TASK_REL_DEADLINE_MS     100u

/* Observation window after tie-trigger submission. */
#define CBS4_TIE_OBSERVE_MS               120u

/* Background arrivals (same server/worker, low rate). */
#define CBS4_BG1_OFFSET_MS               1400u
#define CBS4_BG2_OFFSET_MS               2600u
#define CBS4_BG3_OFFSET_MS               3900u
#define CBS4_BG1_PERIOD_MS               4000u
#define CBS4_BG2_PERIOD_MS               6000u
#define CBS4_BG3_PERIOD_MS               9000u

#define CBS4_PERIODIC_STACK_WORDS         256u
#define CBS4_WORKER_STACK_WORDS           256u
#define CBS4_ARRIVAL_STACK_WORDS          192u

#define CBS4_WINNER_NONE                    0u
#define CBS4_WINNER_PERIODIC                1u
#define CBS4_WINNER_CBS                     2u

static volatile uint32_t ulCBS4PeriodicBeats;
static volatile uint32_t ulCBS4AperiodicJobsDone;
static volatile uint32_t ulCBS4SubmitFailures;

static volatile TickType_t xCBS4PeriodicAnchorTick;
static volatile BaseType_t xCBS4TieWindowOpen;
static volatile uint32_t ulCBS4TieWinner;
static volatile uint32_t ulCBS4TieAttempts;
static volatile uint32_t ulCBS4TieCBSWins;

static TaskHandle_t xCBS4PeriodicHandle;
static TaskHandle_t xCBS4WorkerHandle;
static TaskHandle_t xCBS4TieArrivalHandle;
static TaskHandle_t xCBS4BgArrival1Handle;
static TaskHandle_t xCBS4BgArrival2Handle;
static TaskHandle_t xCBS4BgArrival3Handle;
static CBS_Server_t * pxCBS4Server;

static BaseType_t prvCBS4TrySubmit( void )
{
    if( ( pxCBS4Server != NULL ) && ( xCBS4WorkerHandle != NULL ) )
    {
        if( xCBSSubmitJob( pxCBS4Server, xCBS4WorkerHandle ) == pdPASS )
        {
            return pdPASS;
        }

        ulCBS4SubmitFailures++;
    }

    return pdFAIL;
}

static void vCBS4PeriodicTask( void * pvParameters )
{
    TickType_t xLastWake;

    ( void ) pvParameters;
    xLastWake = xTaskGetTickCount();

    if( xCBS4PeriodicAnchorTick == ( TickType_t ) 0U )
    {
        xCBS4PeriodicAnchorTick = xLastWake;
    }

    for( ;; )
    {
        if( ( xCBS4TieWindowOpen != pdFALSE ) && ( ulCBS4TieWinner == CBS4_WINNER_NONE ) )
        {
            ulCBS4TieWinner = CBS4_WINNER_PERIODIC;
        }

        spin_ms( CBS4_PERIODIC_WORK_MS );
        ulCBS4PeriodicBeats++;
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( CBS4_PERIODIC_PERIOD_MS ) );
    }
}

static void vCBS4WorkerTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        if( xCBSWaitForJob( portMAX_DELAY ) == pdTRUE )
        {
            if( ( xCBS4TieWindowOpen != pdFALSE ) && ( ulCBS4TieWinner == CBS4_WINNER_NONE ) )
            {
                ulCBS4TieWinner = CBS4_WINNER_CBS;
            }

            spin_ms( CBS4_APERIODIC_WORK_MS );
            ulCBS4AperiodicJobsDone++;
            ( void ) xCBSCompleteJob();
        }
    }
}

static void vCBS4BackgroundArrivalTask1( void * pvParameters )
{
    TickType_t xLastWake;

    ( void ) pvParameters;
    vTaskDelay( pdMS_TO_TICKS( CBS4_BG1_OFFSET_MS ) );
    xLastWake = xTaskGetTickCount();

    for( ;; )
    {
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( CBS4_BG1_PERIOD_MS ) );
        ( void ) prvCBS4TrySubmit();
    }
}

static void vCBS4BackgroundArrivalTask2( void * pvParameters )
{
    TickType_t xLastWake;

    ( void ) pvParameters;
    vTaskDelay( pdMS_TO_TICKS( CBS4_BG2_OFFSET_MS ) );
    xLastWake = xTaskGetTickCount();

    for( ;; )
    {
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( CBS4_BG2_PERIOD_MS ) );
        ( void ) prvCBS4TrySubmit();
    }
}

static void vCBS4BackgroundArrivalTask3( void * pvParameters )
{
    TickType_t xLastWake;

    ( void ) pvParameters;
    vTaskDelay( pdMS_TO_TICKS( CBS4_BG3_OFFSET_MS ) );
    xLastWake = xTaskGetTickCount();

    for( ;; )
    {
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( CBS4_BG3_PERIOD_MS ) );
        ( void ) prvCBS4TrySubmit();
    }
}

static void vCBS4TieArrivalTask( void * pvParameters )
{
    TickType_t xLastWake;

    ( void ) pvParameters;

    while( xCBS4PeriodicAnchorTick == ( TickType_t ) 0U )
    {
        vTaskDelay( 1u );
    }

    /* Align this submitter to the same period anchor as the periodic task so
     * submit and periodic release happen on the same tick. */
    xLastWake = xCBS4PeriodicAnchorTick;

    for( ;; )
    {
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( CBS4_SERVER_PERIOD_MS ) );

        ulCBS4TieAttempts++;
        ulCBS4TieWinner = CBS4_WINNER_NONE;
        xCBS4TieWindowOpen = pdTRUE;

        configASSERT( prvCBS4TrySubmit() == pdPASS );

        vTaskDelay( pdMS_TO_TICKS( CBS4_TIE_OBSERVE_MS ) );

        /* At equal deadlines, CBS must win tie-break over periodic EDF tasks. */
        configASSERT( ulCBS4TieWinner == CBS4_WINNER_CBS );
        ulCBS4TieCBSWins++;

        xCBS4TieWindowOpen = pdFALSE;
    }
}

void cbs_4_run( void )
{
    BaseType_t xCreateResult;

    xCBS4PeriodicHandle = NULL;
    xCBS4WorkerHandle = NULL;
    xCBS4TieArrivalHandle = NULL;
    xCBS4BgArrival1Handle = NULL;
    xCBS4BgArrival2Handle = NULL;
    xCBS4BgArrival3Handle = NULL;
    pxCBS4Server = NULL;

    ulCBS4PeriodicBeats = 0u;
    ulCBS4AperiodicJobsDone = 0u;
    ulCBS4SubmitFailures = 0u;
    xCBS4PeriodicAnchorTick = 0u;
    xCBS4TieWindowOpen = pdFALSE;
    ulCBS4TieWinner = CBS4_WINNER_NONE;
    ulCBS4TieAttempts = 0u;
    ulCBS4TieCBSWins = 0u;

    stdio_init_all();
    vTraceTaskPinsInit();

    pxCBS4Server = xCBSServerCreate( pdMS_TO_TICKS( CBS4_SERVER_BUDGET_MS ),
                                     pdMS_TO_TICKS( CBS4_SERVER_PERIOD_MS ),
                                     "CBS4" );
    configASSERT( pxCBS4Server != NULL );

    xCreateResult = xTaskCreate( vCBS4PeriodicTask,
                                 "CBS4_PER",
                                 CBS4_PERIODIC_STACK_WORDS,
                                 NULL,
                                 &xCBS4PeriodicHandle,
                                 CBS4_PERIODIC_PERIOD_MS,
                                 CBS4_PERIODIC_WORK_MS,
                                 CBS4_PERIODIC_PERIOD_MS );
    configASSERT( xCreateResult == pdPASS );

    xCreateResult = xTaskCreateCBSWorker( vCBS4WorkerTask,
                                          "CBS4_APER",
                                          CBS4_WORKER_STACK_WORDS,
                                          NULL,
                                          &xCBS4WorkerHandle,
                                          pxCBS4Server );
    configASSERT( xCreateResult == pdPASS );

    xCreateResult = xTaskCreate( vCBS4TieArrivalTask,
                                 "CBS4_TIE",
                                 CBS4_ARRIVAL_STACK_WORDS,
                                 NULL,
                                 &xCBS4TieArrivalHandle,
                                 CBS4_SERVER_PERIOD_MS,
                                 20u,
                                 CBS4_TIE_TASK_REL_DEADLINE_MS );
    configASSERT( xCreateResult == pdPASS );

    xCreateResult = xTaskCreate( vCBS4BackgroundArrivalTask1,
                                 "CBS4_BG1",
                                 CBS4_ARRIVAL_STACK_WORDS,
                                 NULL,
                                 &xCBS4BgArrival1Handle,
                                 CBS4_BG1_PERIOD_MS,
                                 20u,
                                 CBS4_BG1_PERIOD_MS );
    configASSERT( xCreateResult == pdPASS );

    xCreateResult = xTaskCreate( vCBS4BackgroundArrivalTask2,
                                 "CBS4_BG2",
                                 CBS4_ARRIVAL_STACK_WORDS,
                                 NULL,
                                 &xCBS4BgArrival2Handle,
                                 CBS4_BG2_PERIOD_MS,
                                 20u,
                                 CBS4_BG2_PERIOD_MS );
    configASSERT( xCreateResult == pdPASS );

    xCreateResult = xTaskCreate( vCBS4BackgroundArrivalTask3,
                                 "CBS4_BG3",
                                 CBS4_ARRIVAL_STACK_WORDS,
                                 NULL,
                                 &xCBS4BgArrival3Handle,
                                 CBS4_BG3_PERIOD_MS,
                                 20u,
                                 CBS4_BG3_PERIOD_MS );
    configASSERT( xCreateResult == pdPASS );

    if( xCBS4PeriodicHandle != NULL )
    {
        vTaskSetApplicationTaskTag( xCBS4PeriodicHandle, ( TaskHookFunction_t ) 1 );
    }

    if( xCBS4WorkerHandle != NULL )
    {
        vTaskSetApplicationTaskTag( xCBS4WorkerHandle, ( TaskHookFunction_t ) 2 );
    }

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