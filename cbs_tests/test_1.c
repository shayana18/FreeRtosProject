#include "cbs_tests/test_1.h"

#include "FreeRTOS.h"

#if ( (configUSE_UP == 1) && ( configUSE_EDF == 1 ) && ( configUSE_CBS == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "task.h"
#include "cbs.h"

#include "test_utils.h"
#include "task_trace.h"

#define P1_PERIOD_MS           3000u
#define P1_WORK_MS              250u
#define P2_PERIOD_MS           5000u
#define P2_WORK_MS              350u

#define A1_SERVER_PERIOD_MS    4000u
#define A1_SERVER_BUDGET_MS     1000u
#define A1_WORK_MS              550u
#define A1_ARRIVAL_MS           1000u

#define A2_SERVER_PERIOD_MS    6000u
#define A2_SERVER_BUDGET_MS     3000u
#define A2_WORK_MS              400u
#define A2_ARRIVAL_MS           1500u
#define CBS_PERIODIC_STACK_WORDS   256u
#define CBS_WORKER_STACK_WORDS      256u
#define CBS_ARRIVAL_STACK_WORDS     192u

static volatile uint32_t ulP1Beats;
static volatile uint32_t ulP2Beats;
static volatile uint32_t ulA1Bursts;
static volatile uint32_t ulA2Bursts;
static volatile uint32_t ulA1SubmitFailures;
static volatile uint32_t ulA2SubmitFailures;
static TaskHandle_t xA1WorkerHandle;
static TaskHandle_t xA2WorkerHandle;
static CBS_Server_t * pxA1ServerRef;
static CBS_Server_t * pxA2ServerRef;
static TaskHandle_t xP1TaskHandle;
static TaskHandle_t xP2TaskHandle;
static TaskHandle_t xA1ArrivalTaskHandle;
static TaskHandle_t xA2ArrivalTaskHandle;

static TickType_t xCBS1SharedAnchorTick = 0u;

static void vPeriodicTask1( void * pvParameters )
{
    TickType_t xLastWake;
    BaseType_t xDelayResult;

    ( void ) pvParameters;
    xLastWake = xCBS1SharedAnchorTick;

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
    xLastWake = xCBS1SharedAnchorTick;

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
        if( xCBSWaitForJob( portMAX_DELAY ) == pdTRUE )
        {
            spin_ms( A1_WORK_MS );
            ulA1Bursts++;
            ( void ) xCBSCompleteJob();
        }
    }
}

static void vAperiodicTask2( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        if( xCBSWaitForJob( portMAX_DELAY ) == pdTRUE )
        {
            spin_ms( A2_WORK_MS );
            ulA2Bursts++;
            ( void ) xCBSCompleteJob();
        }
    }
}

static void vA1ArrivalTask( void * pvParameters )
{
    TickType_t xLastWake;

    ( void ) pvParameters;
    xLastWake = xCBS1SharedAnchorTick;

    if( ( pxA1ServerRef != NULL ) && ( xA1WorkerHandle != NULL ) )
    {
        if( xCBSSubmitJob( pxA1ServerRef, xA1WorkerHandle ) == pdFAIL )
        {
            ulA1SubmitFailures++;
        }
    }

    for( ;; )
    {
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( A1_ARRIVAL_MS ) );

        if( ( pxA1ServerRef != NULL ) && ( xA1WorkerHandle != NULL ) )
        {
            if( xCBSSubmitJob( pxA1ServerRef, xA1WorkerHandle ) == pdFAIL )
            {
                ulA1SubmitFailures++;
            }
        }
    }
}

static void vA2ArrivalTask( void * pvParameters )
{
    TickType_t xLastWake;

    ( void ) pvParameters;
    xLastWake = xCBS1SharedAnchorTick;

    if( ( pxA2ServerRef != NULL ) && ( xA2WorkerHandle != NULL ) )
    {
        if( xCBSSubmitJob( pxA2ServerRef, xA2WorkerHandle ) == pdFAIL )
        {
            ulA2SubmitFailures++;
        }
    }

    for( ;; )
    {
        ( void ) xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( A2_ARRIVAL_MS ) );

        if( ( pxA2ServerRef != NULL ) && ( xA2WorkerHandle != NULL ) )
        {
            if( xCBSSubmitJob( pxA2ServerRef, xA2WorkerHandle ) == pdFAIL )
            {
                ulA2SubmitFailures++;
            }
        }
    }
}

void cbs_1_run( void )
{
    BaseType_t xCreateResult;

    xP1TaskHandle = NULL;
    xP2TaskHandle = NULL;
    xA1WorkerHandle = NULL;
    xA2WorkerHandle = NULL;
    xA1ArrivalTaskHandle = NULL;
    xA2ArrivalTaskHandle = NULL;
    pxA1ServerRef = NULL;
    pxA2ServerRef = NULL;

    stdio_init_all();
    vTraceTaskPinsInit();

    xCBS1SharedAnchorTick = xTaskGetTickCount();

    pxA1ServerRef = xCBSServerCreate( pdMS_TO_TICKS( A1_SERVER_BUDGET_MS ),
                                      pdMS_TO_TICKS( A1_SERVER_PERIOD_MS ),
                                      "CBS_A1" );
    pxA2ServerRef = xCBSServerCreate( pdMS_TO_TICKS( A2_SERVER_BUDGET_MS ),
                                      pdMS_TO_TICKS( A2_SERVER_PERIOD_MS ),
                                      "CBS_A2" );

    configASSERT( pxA1ServerRef != NULL );
    configASSERT( pxA2ServerRef != NULL );
    configASSERT( pxA1ServerRef->xPeriodTicks == pdMS_TO_TICKS( A1_SERVER_PERIOD_MS ) );
    configASSERT( pxA1ServerRef->xCapacityTicks == pdMS_TO_TICKS( A1_SERVER_BUDGET_MS ) );
    configASSERT( pxA2ServerRef->xPeriodTicks == pdMS_TO_TICKS( A2_SERVER_PERIOD_MS ) );
    configASSERT( pxA2ServerRef->xCapacityTicks == pdMS_TO_TICKS( A2_SERVER_BUDGET_MS ) );

    xCreateResult = xTaskCreate( vPeriodicTask1,
                                 "PERIODIC_1",
                                 CBS_PERIODIC_STACK_WORDS,
                                 NULL,
                                 &xP1TaskHandle,
                                 P1_PERIOD_MS,
                                 P1_WORK_MS,
                                 P1_PERIOD_MS );
    configASSERT( xCreateResult == pdPASS );
    configASSERT( xP1TaskHandle != NULL );

    xCreateResult = xTaskCreate( vPeriodicTask2,
                                 "PERIODIC_2",
                                 CBS_PERIODIC_STACK_WORDS,
                                 NULL,
                                 &xP2TaskHandle,
                                 P2_PERIOD_MS,
                                 P2_WORK_MS,
                                 P2_PERIOD_MS );
    configASSERT( xCreateResult == pdPASS );
    configASSERT( xP2TaskHandle != NULL );

    xCreateResult = xTaskCreateCBSWorker( vAperiodicTask1,
                                          "CBS_APER_1",
                                          CBS_WORKER_STACK_WORDS,
                                          NULL,
                                          &xA1WorkerHandle,
                                          pxA1ServerRef );
    configASSERT( xCreateResult == pdPASS );
    configASSERT( xA1WorkerHandle != NULL );

    xCreateResult = xTaskCreateCBSWorker( vAperiodicTask2,
                                          "CBS_APER_2",
                                          CBS_WORKER_STACK_WORDS,
                                          NULL,
                                          &xA2WorkerHandle,
                                          pxA2ServerRef );
    configASSERT( xCreateResult == pdPASS );
    configASSERT( xA2WorkerHandle != NULL );

    xCreateResult = xTaskCreate( vA1ArrivalTask,
                                 "APER_A1_SRC",
                                 CBS_ARRIVAL_STACK_WORDS,
                                 NULL,
                                 &xA1ArrivalTaskHandle,
                                 A1_ARRIVAL_MS,
                                 20u,
                                 A1_ARRIVAL_MS );
    configASSERT( xCreateResult == pdPASS );
    configASSERT( xA1ArrivalTaskHandle != NULL );

    xCreateResult = xTaskCreate( vA2ArrivalTask,
                                 "APER_A2_SRC",
                                 CBS_ARRIVAL_STACK_WORDS,
                                 NULL,
                                 &xA2ArrivalTaskHandle,
                                 A2_ARRIVAL_MS,
                                 20u,
                                 A2_ARRIVAL_MS );
    configASSERT( xCreateResult == pdPASS );
    configASSERT( xA2ArrivalTaskHandle != NULL );

    if( xP1TaskHandle != NULL )
    {
        vTaskSetApplicationTaskTag( xP1TaskHandle, ( TaskHookFunction_t ) 1 );
    }

    if( xP2TaskHandle != NULL )
    {
        vTaskSetApplicationTaskTag( xP2TaskHandle, ( TaskHookFunction_t ) 2 );
    }

    if( xA1WorkerHandle != NULL )
    {
        vTaskSetApplicationTaskTag( xA1WorkerHandle, ( TaskHookFunction_t ) 4 );
    }

    if( xA2WorkerHandle != NULL )
    {
        vTaskSetApplicationTaskTag( xA2WorkerHandle, ( TaskHookFunction_t ) 8 );
    }

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#else

void cbs_1_run( void )
{
}

#endif