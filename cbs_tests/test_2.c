#include "cbs_tests/test_2.h"

#include "FreeRTOS.h"

#if ( ( configUSE_EDF == 1 ) && ( configUSE_CBS == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "task.h"
#include "cbs.h"

#include "test_utils.h"
#include "task_trace.h"

#define P1_PERIOD_MS           3000u
#define P1_WORK_MS              200u
#define P2_PERIOD_MS           5000u
#define P2_WORK_MS              350u

#define A1_SERVER_PERIOD_MS    4000u
#define A1_SERVER_BUDGET_MS     300u
#define A1_WORK_MS              220u
#define A1_SLEEP_MS            1200u

#define A2_SERVER_PERIOD_MS    7000u
#define A2_SERVER_BUDGET_MS     500u
#define A2_WORK_MS              260u
#define A2_SLEEP_MS            1700u

static volatile uint32_t ulP1Beats;
static volatile uint32_t ulP2Beats;
static volatile uint32_t ulA1Bursts;
static volatile uint32_t ulA2Bursts;

static void vPeriodicTask1( void * pvParameters )
{
    TickType_t xLastWake;
    BaseType_t xDelayResult;

    ( void ) pvParameters;
    xLastWake = xTaskGetTickCount();

    for( ;; )
    {
        spin_ms( P1_WORK_MS );
        ulP1Beats++;
        xDelayResult = xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( P1_PERIOD_MS ) );

        if( xDelayResult == pdFALSE )
        {
            xLastWake = xTaskGetTickCount();
        }
    }
}

static void vPeriodicTask2( void * pvParameters )
{
    TickType_t xLastWake;
    BaseType_t xDelayResult;

    ( void ) pvParameters;
    xLastWake = xTaskGetTickCount();

    for( ;; )
    {
        spin_ms( P2_WORK_MS );
        ulP2Beats++;
        xDelayResult = xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( P2_PERIOD_MS ) );

        if( xDelayResult == pdFALSE )
        {
            xLastWake = xTaskGetTickCount();
        }
    }
}

static void vAperiodicTask1( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        spin_ms( A1_WORK_MS );
        ulA1Bursts++;
        ( void ) vTaskDelay( pdMS_TO_TICKS( A1_SLEEP_MS ) );
    }
}

static void vAperiodicTask2( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        spin_ms( A2_WORK_MS );
        ulA2Bursts++;
        ( void ) vTaskDelay( pdMS_TO_TICKS( A2_SLEEP_MS ) );
    }
}

void cbs_2_run( void )
{
    TaskHandle_t xP1Handle = NULL;
    TaskHandle_t xP2Handle = NULL;
    TaskHandle_t xA1Handle = NULL;
    TaskHandle_t xA2Handle = NULL;
    CBS_Server_t * pxA1Server;
    CBS_Server_t * pxA2Server;

    stdio_init_all();
    vTraceTaskPinsInit();

    pxA1Server = xCBSServerCreate( pdMS_TO_TICKS( A1_SERVER_BUDGET_MS ),
                                   pdMS_TO_TICKS( A1_SERVER_PERIOD_MS ),
                                   "CBS_A1" );
    pxA2Server = xCBSServerCreate( pdMS_TO_TICKS( A2_SERVER_BUDGET_MS ),
                                   pdMS_TO_TICKS( A2_SERVER_PERIOD_MS ),
                                   "CBS_A2" );

    configASSERT( pxA1Server != NULL );
    configASSERT( pxA2Server != NULL );

    ( void ) xTaskCreate( vPeriodicTask1,
                          "PERIODIC_1",
                          256,
                          NULL,
                          &xP1Handle,
                          P1_PERIOD_MS,
                          P1_WORK_MS,
                          P1_PERIOD_MS );

    ( void ) xTaskCreate( vPeriodicTask2,
                          "PERIODIC_2",
                          256,
                          NULL,
                          &xP2Handle,
                          P2_PERIOD_MS,
                          P2_WORK_MS,
                          P2_PERIOD_MS );

    ( void ) xTaskCreateCBS( vAperiodicTask1,
                             "CBS_APER_1",
                             256,
                             NULL,
                             &xA1Handle,
                             A1_SERVER_PERIOD_MS,
                             A1_SERVER_BUDGET_MS,
                             A1_SERVER_PERIOD_MS,
                             pxA1Server );

    ( void ) xTaskCreateCBS( vAperiodicTask2,
                             "CBS_APER_2",
                             256,
                             NULL,
                             &xA2Handle,
                             A2_SERVER_PERIOD_MS,
                             A2_SERVER_BUDGET_MS,
                             A2_SERVER_PERIOD_MS,
                             pxA2Server );

    if( xP1Handle != NULL )
    {
        vTaskSetApplicationTaskTag( xP1Handle, ( TaskHookFunction_t ) 1 );
    }

    if( xP2Handle != NULL )
    {
        vTaskSetApplicationTaskTag( xP2Handle, ( TaskHookFunction_t ) 2 );
    }

    if( xA1Handle != NULL )
    {
        vTaskSetApplicationTaskTag( xA1Handle, ( TaskHookFunction_t ) 4 );
    }

    if( xA2Handle != NULL )
    {
        vTaskSetApplicationTaskTag( xA2Handle, ( TaskHookFunction_t ) 8 );
    }

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