#include "cbs_tests/test_1.h"

#include "FreeRTOS.h"

#if ( (configUSE_UP == 1) && ( configUSE_EDF == 1 ) && ( configUSE_CBS == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "task.h"
#include "cbs.h"

#include "test_utils.h"
#include "task_trace.h"

#define CBS_SERVER_PERIOD_MS    2000u
#define CBS_SERVER_BUDGET_MS     400u
#define PERIODIC_TASK_PERIOD_MS  1500u
#define PERIODIC_TASK_WORK_MS     300u
#define APERIODIC_RELEASE_MS      100u
#define CBS_TEST_STACK_WORDS      512u

static volatile uint32_t ulPeriodicBeats;
static volatile uint32_t ulAperiodicWorkIters;
static volatile uint32_t ulAperiodicSubmitFailures;
static TaskHandle_t xAperiodicWorkerHandle;
static CBS_Server_t * pxAperiodicServer;

static TickType_t xCbs1SharedAnchorTick = 0u;

static void vPeriodicTask( void * pvParameters )
{
    TickType_t xLastWake;

    ( void ) pvParameters;
    xLastWake = xCbs1SharedAnchorTick;

    for( ; ; )
    {
        spin_ms( PERIODIC_TASK_WORK_MS );
        ulPeriodicBeats++;
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( PERIODIC_TASK_PERIOD_MS ) );
    }
}

static void vCBSAperiodicTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ; ; )
    {
        if( xCBSWaitForJob( portMAX_DELAY ) == pdTRUE )
        {
            spin_ms( 250u );
            ulAperiodicWorkIters++;
            ( void ) xCBSCompleteJob();
        }
    }
}

static void vAperiodicArrivalTask( void * pvParameters )
{
    TickType_t xLastWake;

    ( void ) pvParameters;
    xLastWake = xCbs1SharedAnchorTick;

    for( ; ; )
    {
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( APERIODIC_RELEASE_MS ) );

        if( ( xAperiodicWorkerHandle != NULL ) && ( pxAperiodicServer != NULL ) )
        {
            if( xCBSSubmitJob( pxAperiodicServer, xAperiodicWorkerHandle ) == pdFAIL )
            {
                ulAperiodicSubmitFailures++;
            }
        }
    }
}

void cbs_1_run( void )
{
    TaskHandle_t xPeriodic = NULL;
    TaskHandle_t xAperiodic = NULL;
    TaskHandle_t xArrival = NULL;
    CBS_Server_t * pxServer;

    stdio_init_all();
    vTraceTaskPinsInit();

    xCbs1SharedAnchorTick = xTaskGetTickCount();

    pxServer = xCBSServerCreate( pdMS_TO_TICKS( CBS_SERVER_BUDGET_MS ),
                                 pdMS_TO_TICKS( CBS_SERVER_PERIOD_MS ),
                                 "CBS0" );

    if( pxServer == NULL )
    {
        for( ; ; )
        {
        }
    }

    pxAperiodicServer = pxServer;

    ( void ) xTaskCreate( vPeriodicTask,
                          "PERIODIC",
                          CBS_TEST_STACK_WORDS,
                          NULL,
                          &xPeriodic,
                          PERIODIC_TASK_PERIOD_MS,
                          PERIODIC_TASK_WORK_MS,
                          PERIODIC_TASK_PERIOD_MS );

    ( void ) xTaskCreateCBSWorker( vCBSAperiodicTask,
                                   "CBS_APER",
                                   CBS_TEST_STACK_WORDS,
                                   NULL,
                                   &xAperiodic,
                                   pxServer );

    xAperiodicWorkerHandle = xAperiodic;

    ( void ) xTaskCreate( vAperiodicArrivalTask,
                          "APER_ARRIVE",
                          CBS_TEST_STACK_WORDS,
                          NULL,
                          &xArrival,
                          APERIODIC_RELEASE_MS,
                          20u,
                          APERIODIC_RELEASE_MS );

    if( xPeriodic != NULL )
    {
        vTaskSetApplicationTaskTag( xPeriodic, ( TaskHookFunction_t ) 1 );
    }

    if( xAperiodic != NULL )
    {
        vTaskSetApplicationTaskTag( xAperiodic, ( TaskHookFunction_t ) 2 );
    }


    vTaskStartScheduler();

    for( ; ; )
    {
    }
}

#else

void cbs_1_run( void )
{
}

#endif
