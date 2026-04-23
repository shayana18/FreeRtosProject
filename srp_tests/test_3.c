#include "srp_tests/test_3.h"

#if ( ( configUSE_UP == 1 ) && ( configUSE_EDF == 1 ) && ( configUSE_SRP == 1 ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * SRP admission-control test with blocking terms.
 *
 * A long-deadline task claims R1 for 4.5s. A short-deadline candidate with
 * D=5s would pass a plain utilization check, but once the R1 blocking term is
 * included it must be rejected. A second candidate with D=8s should be accepted.
 */

#define SRP3_STACK_DEPTH                512u
#define SRP3_RESOURCE_R1                0u

#define SRP3_BASE_PERIOD_MS         10000u
#define SRP3_BASE_DEADLINE_MS       10000u
#define SRP3_BASE_R1_CLAIM_MS        4500u
#define SRP3_BASE_R1_WORK_MS         4300u
#define SRP3_BASE_NON_CRIT_MS         500u
#define SRP3_BASE_WCET_MS            5000u

#define SRP3_BAD_PERIOD_MS           5000u
#define SRP3_BAD_DEADLINE_MS         5000u
#define SRP3_BAD_R1_CLAIM_MS          100u
#define SRP3_BAD_WCET_MS             1000u

#define SRP3_GOOD_PERIOD_MS          8000u
#define SRP3_GOOD_DEADLINE_MS        8000u
#define SRP3_GOOD_R1_CLAIM_MS         400u
#define SRP3_GOOD_R1_WORK_MS          400u
#define SRP3_GOOD_NON_CRIT_MS         400u
#define SRP3_GOOD_WCET_MS            1000u

#define SRP3_TRACE_BASE                 1u
#define SRP3_TRACE_GOOD                 2u

#define SRP3_ARRAY_LENGTH( x )    ( sizeof( x ) / sizeof( ( x )[ 0 ] ) )

static SemaphoreHandle_t xSRP3R1Semaphore = NULL;
static TickType_t xSRP3AnchorTick = 0u;
static volatile BaseType_t xSRP3BadRejected = pdFALSE;
static volatile BaseType_t xSRP3GoodAccepted = pdFALSE;
static volatile uint32_t ulSRP3GoodJobs = 0u;

static const SRPResourceClaim_t xSRP3BaseClaims[] =
{
    { SRP3_RESOURCE_R1, pdMS_TO_TICKS( SRP3_BASE_R1_CLAIM_MS ) }
};

static const SRPResourceClaim_t xSRP3BadClaims[] =
{
    { SRP3_RESOURCE_R1, pdMS_TO_TICKS( SRP3_BAD_R1_CLAIM_MS ) }
};

static const SRPResourceClaim_t xSRP3GoodClaims[] =
{
    { SRP3_RESOURCE_R1, pdMS_TO_TICKS( SRP3_GOOD_R1_CLAIM_MS ) }
};

static void vSRP3BaseTask( void * pvParameters )
{
    TickType_t xLastWakeTime = xSRP3AnchorTick;

    ( void ) pvParameters;

    for( ;; )
    {
        configASSERT( xSemaphoreTakeSRP( xSRP3R1Semaphore, 0u, SRP3_RESOURCE_R1 ) == pdPASS );
        spin_ms( SRP3_BASE_R1_WORK_MS );
        configASSERT( xSemaphoreGiveSRP( xSRP3R1Semaphore, SRP3_RESOURCE_R1 ) == pdPASS );

        spin_ms( SRP3_BASE_NON_CRIT_MS );
        vSRPReportStackUsageIfDue();

        if( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( SRP3_BASE_PERIOD_MS ) ) == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void vSRP3GoodTask( void * pvParameters )
{
    TickType_t xLastWakeTime = xTaskGetTickCount();

    ( void ) pvParameters;

    for( ;; )
    {
        configASSERT( xSemaphoreTakeSRP( xSRP3R1Semaphore, 0u, SRP3_RESOURCE_R1 ) == pdPASS );
        spin_ms( SRP3_GOOD_R1_WORK_MS );
        configASSERT( xSemaphoreGiveSRP( xSRP3R1Semaphore, SRP3_RESOURCE_R1 ) == pdPASS );

        spin_ms( SRP3_GOOD_NON_CRIT_MS );
        ulSRP3GoodJobs++;
        vSRPReportStackUsageIfDue();

        if( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( SRP3_GOOD_PERIOD_MS ) ) == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void vSRP3BadTask( void * pvParameters )
{
    ( void ) pvParameters;
    configASSERT( pdFALSE );

    for( ;; )
    {
    }
}

void srp_3_run( void )
{
    TaskHandle_t xBaseHandle = NULL;
    TaskHandle_t xBadHandle = NULL;
    TaskHandle_t xGoodHandle = NULL;
    BaseType_t xCreateResult;

    stdio_init_all();
    vTraceTaskPinsInit();

    xSRP3R1Semaphore = xSemaphoreCreateBinary();
    configASSERT( xSRP3R1Semaphore != NULL );
    configASSERT( xSemaphoreGive( xSRP3R1Semaphore ) == pdTRUE );

    xSRP3AnchorTick = xTaskGetTickCount();

    configASSERT( xTaskCreate( vSRP3BaseTask,
                               "SRP3 BASE",
                               SRP3_STACK_DEPTH,
                               NULL,
                               &xBaseHandle,
                               SRP3_BASE_PERIOD_MS,
                               SRP3_BASE_WCET_MS,
                               SRP3_BASE_DEADLINE_MS,
                               xSRP3BaseClaims,
                               SRP3_ARRAY_LENGTH( xSRP3BaseClaims ) ) == pdPASS );

    xCreateResult = xTaskCreate( vSRP3BadTask,
                                 "SRP3 BAD",
                                 SRP3_STACK_DEPTH,
                                 NULL,
                                 &xBadHandle,
                                 SRP3_BAD_PERIOD_MS,
                                 SRP3_BAD_WCET_MS,
                                 SRP3_BAD_DEADLINE_MS,
                                 xSRP3BadClaims,
                                 SRP3_ARRAY_LENGTH( xSRP3BadClaims ) );
    xSRP3BadRejected = ( xCreateResult == pdFAIL ) ? pdTRUE : pdFALSE;
    configASSERT( xSRP3BadRejected == pdTRUE );
    configASSERT( xBadHandle == NULL );

    xCreateResult = xTaskCreate( vSRP3GoodTask,
                                 "SRP3 GOOD",
                                 SRP3_STACK_DEPTH,
                                 NULL,
                                 &xGoodHandle,
                                 SRP3_GOOD_PERIOD_MS,
                                 SRP3_GOOD_WCET_MS,
                                 SRP3_GOOD_DEADLINE_MS,
                                 xSRP3GoodClaims,
                                 SRP3_ARRAY_LENGTH( xSRP3GoodClaims ) );
    xSRP3GoodAccepted = ( xCreateResult == pdPASS ) ? pdTRUE : pdFALSE;
    configASSERT( xSRP3GoodAccepted == pdTRUE );
    configASSERT( xGoodHandle != NULL );

    vTaskSetApplicationTaskTag( xBaseHandle, ( TaskHookFunction_t ) SRP3_TRACE_BASE );
    vTaskSetApplicationTaskTag( xGoodHandle, ( TaskHookFunction_t ) SRP3_TRACE_GOOD );

    vTraceUsbPrint( "[SRP3] admission results: bad_rejected=%ld good_accepted=%ld\r\n",
                    ( long ) xSRP3BadRejected,
                    ( long ) xSRP3GoodAccepted );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#else

void srp_3_run( void )
{
}

#endif
