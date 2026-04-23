#include "srp_tests/test_5.h"

#if ( ( configUSE_UP == 1 ) && ( configUSE_EDF == 1 ) && ( configUSE_SRP == 1 ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * SRP WCET-overrun test inside a critical section.
 *
 * The overrun task holds R1 and intentionally executes longer than its WCET.
 * The kernel should stop that job and force-release the resource so another
 * task can acquire R1 instead of being blocked for the rest of the period.
 */

#define SRP5_STACK_DEPTH                 512u
#define SRP5_RESOURCE_R1                 0u

#define SRP5_OBSERVER_PERIOD_MS       10000u
#define SRP5_OBSERVER_DEADLINE_MS     10000u
#define SRP5_OBSERVER_WCET_MS           700u
#define SRP5_OBSERVER_R1_CLAIM_MS       300u
#define SRP5_OBSERVER_WORK_MS           500u

#define SRP5_OVERRUN_PERIOD_MS        8000u
#define SRP5_OVERRUN_DEADLINE_MS      8000u
#define SRP5_OVERRUN_WCET_MS          1000u
#define SRP5_OVERRUN_R1_CLAIM_MS      1800u
#define SRP5_OVERRUN_WORK_MS          1800u

#define SRP5_TRACE_OVERRUN               1u
#define SRP5_TRACE_OBSERVER              2u

#define SRP5_ARRAY_LENGTH( x )    ( sizeof( x ) / sizeof( ( x )[ 0 ] ) )

static SemaphoreHandle_t xSRP5R1Semaphore = NULL;
static TickType_t xSRP5AnchorTick = 0u;
static volatile uint32_t ulSRP5ObserverAcquires = 0u;
static volatile uint32_t ulSRP5ForcedReleaseObserved = 0u;

static const SRPResourceClaim_t xSRP5ObserverClaims[] =
{
    { SRP5_RESOURCE_R1, pdMS_TO_TICKS( SRP5_OBSERVER_R1_CLAIM_MS ) }
};

static const SRPResourceClaim_t xSRP5OverrunClaims[] =
{
    { SRP5_RESOURCE_R1, pdMS_TO_TICKS( SRP5_OVERRUN_R1_CLAIM_MS ) }
};

static void vSRP5ObserverTask( void * pvParameters )
{
    TickType_t xLastWakeTime = xSRP5AnchorTick;

    ( void ) pvParameters;

    for( ;; )
    {
        configASSERT( xSemaphoreTakeSRP( xSRP5R1Semaphore, 0u, SRP5_RESOURCE_R1 ) == pdPASS );
        ulSRP5ObserverAcquires++;
        vTraceUsbPrint( "[SRP5] observer acquired R1 after overrun cleanup, count=%lu\r\n",
                        ( unsigned long ) ulSRP5ObserverAcquires );
        spin_ms( SRP5_OBSERVER_WORK_MS );
        configASSERT( xSemaphoreGiveSRP( xSRP5R1Semaphore, SRP5_RESOURCE_R1 ) == pdPASS );

        vSRPReportStackUsageIfDue();

        if( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( SRP5_OBSERVER_PERIOD_MS ) ) == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void vSRP5OverrunTask( void * pvParameters )
{
    TickType_t xLastWakeTime = xSRP5AnchorTick;

    ( void ) pvParameters;

    for( ;; )
    {
        BaseType_t xGiveResult;

        configASSERT( xSemaphoreTakeSRP( xSRP5R1Semaphore, 0u, SRP5_RESOURCE_R1 ) == pdPASS );
        vTraceUsbPrint( "[SRP5] overrun task entered R1 critical section\r\n" );
        spin_ms( SRP5_OVERRUN_WORK_MS );

        xGiveResult = xSemaphoreGiveSRP( xSRP5R1Semaphore, SRP5_RESOURCE_R1 );

        if( xGiveResult == pdFAIL )
        {
            ulSRP5ForcedReleaseObserved++;
            vTraceUsbPrint( "[SRP5] forced release observed after WCET overrun, count=%lu\r\n",
                            ( unsigned long ) ulSRP5ForcedReleaseObserved );
        }

        vSRPReportStackUsageIfDue();

        if( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( SRP5_OVERRUN_PERIOD_MS ) ) == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void srp_5_run( void )
{
    TaskHandle_t xObserverHandle = NULL;
    TaskHandle_t xOverrunHandle = NULL;

    stdio_init_all();
    vTraceTaskPinsInit();

    xSRP5R1Semaphore = xSemaphoreCreateBinary();
    configASSERT( xSRP5R1Semaphore != NULL );
    configASSERT( xSemaphoreGive( xSRP5R1Semaphore ) == pdTRUE );

    xSRP5AnchorTick = xTaskGetTickCount();

    configASSERT( xTaskCreate( vSRP5ObserverTask,
                               "SRP5 OBS",
                               SRP5_STACK_DEPTH,
                               NULL,
                               &xObserverHandle,
                               SRP5_OBSERVER_PERIOD_MS,
                               SRP5_OBSERVER_WCET_MS,
                               SRP5_OBSERVER_DEADLINE_MS,
                               xSRP5ObserverClaims,
                               SRP5_ARRAY_LENGTH( xSRP5ObserverClaims ) ) == pdPASS );

    configASSERT( xTaskCreate( vSRP5OverrunTask,
                               "SRP5 OVER",
                               SRP5_STACK_DEPTH,
                               NULL,
                               &xOverrunHandle,
                               SRP5_OVERRUN_PERIOD_MS,
                               SRP5_OVERRUN_WCET_MS,
                               SRP5_OVERRUN_DEADLINE_MS,
                               xSRP5OverrunClaims,
                               SRP5_ARRAY_LENGTH( xSRP5OverrunClaims ) ) == pdPASS );

    vTaskSetApplicationTaskTag( xObserverHandle, ( TaskHookFunction_t ) SRP5_TRACE_OBSERVER );
    vTaskSetApplicationTaskTag( xOverrunHandle, ( TaskHookFunction_t ) SRP5_TRACE_OVERRUN );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#else

void srp_5_run( void )
{
}

#endif
