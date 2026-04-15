#include "cbs_tests/test_1.h"

#include "FreeRTOS.h"

#if ( ( configUSE_EDF == 1 ) && ( configUSE_CBS == 1 ) && ( configUSE_SRP == 0 ) )

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

static volatile uint32_t ulPeriodicBeats;
static volatile uint32_t ulAperiodicWorkIters;

static void vPeriodicTask( void * pvParameters )
{
    TickType_t xLastWake;

    ( void ) pvParameters;
    xLastWake = xTaskGetTickCount();

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
        spin_ms( 250u );
        ulAperiodicWorkIters++;
        ( void ) vTaskDelay( pdMS_TO_TICKS( 1000u ) );
    }
}

void cbs_1_run( void )
{
    TaskHandle_t xPeriodic = NULL;
    TaskHandle_t xAperiodic = NULL;
    CBS_Server_t * pxServer;

    stdio_init_all();
    vTraceTaskPinsInit();

    pxServer = xCBSServerCreate( pdMS_TO_TICKS( CBS_SERVER_BUDGET_MS ),
                                 pdMS_TO_TICKS( CBS_SERVER_PERIOD_MS ),
                                 "CBS0" );

    if( pxServer == NULL )
    {
        for( ; ; )
        {
        }
    }

    ( void ) xTaskCreate( vPeriodicTask,
                          "PERIODIC",
                          256,
                          NULL,
                          &xPeriodic,
                          PERIODIC_TASK_PERIOD_MS,
                          PERIODIC_TASK_WORK_MS,
                          PERIODIC_TASK_PERIOD_MS );

    ( void ) xTaskCreateCBS( vCBSAperiodicTask,
                             "CBS_APER",
                             256,
                             NULL,
                             &xAperiodic,
                             CBS_SERVER_PERIOD_MS,
                             CBS_SERVER_BUDGET_MS,
                             CBS_SERVER_PERIOD_MS,
                             pxServer );

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
