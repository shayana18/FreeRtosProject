#include "srp_tests/test_2.h"

#if ( (configUSE_UP == 1) && ( configUSE_EDF == 1 ) && ( configUSE_SRP == 1 ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "test_utils.h"
#include "task_trace.h"

#define STACK_DEPTH             512u
#define WCET_SLACK              500u

#define SAME_LEVEL_TASK_COUNT    6u
#define COMMON_PERIOD_MS      10000u
#define COMMON_DEADLINE_MS    10000u

#define T1_R1_CS_MS             300u
#define T1_NON_CRIT_MS          700u
#define T1_WCET_MS             ( T1_R1_CS_MS + T1_NON_CRIT_MS + WCET_SLACK )

#define T2_R2_CS_MS             250u
#define T2_NON_CRIT_MS          800u
#define T2_WCET_MS             ( T2_R2_CS_MS + T2_NON_CRIT_MS + WCET_SLACK )

#define T3_R1_CS_MS             200u
#define T3_NON_CRIT_MS          900u
#define T3_WCET_MS             ( T3_R1_CS_MS + T3_NON_CRIT_MS + WCET_SLACK )

#define T4_R2_CS_MS             300u
#define T4_NON_CRIT_MS          700u
#define T4_WCET_MS             ( T4_R2_CS_MS + T4_NON_CRIT_MS + WCET_SLACK )

#define T5_R1_CS_MS             250u
#define T5_NON_CRIT_MS          800u
#define T5_PREMPTED_NON_CRIT_MS 200u
#define T5_WCET_MS             ( T5_R1_CS_MS + T5_NON_CRIT_MS + WCET_SLACK + T5_PREMPTED_NON_CRIT_MS )

#define T6_DEADLINE_MS         4000u
#define T6_R1_CS_MS             200u
#define T6_NON_CRIT_MS          900u
#define T6_WCET_MS             ( T6_R1_CS_MS + T6_NON_CRIT_MS + WCET_SLACK )
#define T6_RELEASE_OFFSET_MS   ( T1_NON_CRIT_MS + T1_R1_CS_MS + \
                                 T2_NON_CRIT_MS + T2_R2_CS_MS + \
                                 T3_NON_CRIT_MS + T3_R1_CS_MS + \
                                 T4_NON_CRIT_MS + T4_R2_CS_MS + \
                                 T5_NON_CRIT_MS )

#define SRP_RESOURCE_R1           0u
#define SRP_RESOURCE_R2           1u

#define TRACE_TASK_T1             1u
#define TRACE_TASK_T2             2u
#define TRACE_TASK_T3             4u
#define TRACE_TASK_T4             8u
#define TRACE_TASK_T5            16u
#define TRACE_TASK_T6            32u

#define ARRAY_LENGTH( x )    ( sizeof( x ) / sizeof( ( x )[ 0 ] ) )

static SemaphoreHandle_t xR1Semaphore = NULL;
static SemaphoreHandle_t xR2Semaphore = NULL;
static TickType_t xSynchronousReleaseTick = 0U;

static const SRPResourceClaim_t xTask1Claims[] =
{
    { SRP_RESOURCE_R1, pdMS_TO_TICKS( T1_R1_CS_MS ) }
};

static const SRPResourceClaim_t xTask2Claims[] =
{
    { SRP_RESOURCE_R2, pdMS_TO_TICKS( T2_R2_CS_MS ) }
};

static const SRPResourceClaim_t xTask3Claims[] =
{
    { SRP_RESOURCE_R1, pdMS_TO_TICKS( T3_R1_CS_MS ) }
};

static const SRPResourceClaim_t xTask4Claims[] =
{
    { SRP_RESOURCE_R2, pdMS_TO_TICKS( T4_R2_CS_MS ) }
};

static const SRPResourceClaim_t xTask5Claims[] =
{
    { SRP_RESOURCE_R1, pdMS_TO_TICKS( T5_R1_CS_MS ) }
};

static const SRPResourceClaim_t xTask6Claims[] =
{
    { SRP_RESOURCE_R1, pdMS_TO_TICKS( T6_R1_CS_MS ) }
};

static void vSRP2Task1( void * pvParameters )
{
    TickType_t xLastWakeTime;

    ( void ) pvParameters;
    xLastWakeTime = xSynchronousReleaseTick;

    for( ;; )
    {
        vSRPReportStackUsageIfDue();

        spin_ms( T1_NON_CRIT_MS );

        configASSERT( xSemaphoreTakeSRP( xR1Semaphore, 0U, SRP_RESOURCE_R1 ) == pdPASS );
        spin_ms( T1_R1_CS_MS );
        configASSERT( xSemaphoreGiveSRP( xR1Semaphore, SRP_RESOURCE_R1 ) == pdPASS );

        configASSERT( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( COMMON_PERIOD_MS ) ) != pdFALSE );
    }
}

static void vSRP2Task2( void * pvParameters )
{
    TickType_t xLastWakeTime;

    ( void ) pvParameters;
    xLastWakeTime = xSynchronousReleaseTick;

    for( ;; )
    {
        vSRPReportStackUsageIfDue();

        spin_ms( T2_NON_CRIT_MS );

        configASSERT( xSemaphoreTakeSRP( xR2Semaphore, 0U, SRP_RESOURCE_R2 ) == pdPASS );
        spin_ms( T2_R2_CS_MS );
        configASSERT( xSemaphoreGiveSRP( xR2Semaphore, SRP_RESOURCE_R2 ) == pdPASS );

        configASSERT( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( COMMON_PERIOD_MS ) ) != pdFALSE );
    }
}

static void vSRP2Task3( void * pvParameters )
{
    TickType_t xLastWakeTime;

    ( void ) pvParameters;
    xLastWakeTime = xSynchronousReleaseTick;

    for( ;; )
    {
        vSRPReportStackUsageIfDue();

        spin_ms( T3_NON_CRIT_MS );

        configASSERT( xSemaphoreTakeSRP( xR1Semaphore, 0U, SRP_RESOURCE_R1 ) == pdPASS );
        spin_ms( T3_R1_CS_MS );
        configASSERT( xSemaphoreGiveSRP( xR1Semaphore, SRP_RESOURCE_R1 ) == pdPASS );

        configASSERT( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( COMMON_PERIOD_MS ) ) != pdFALSE );
    }
}

static void vSRP2Task4( void * pvParameters )
{
    TickType_t xLastWakeTime;

    ( void ) pvParameters;
    xLastWakeTime = xSynchronousReleaseTick;

    for( ;; )
    {
        vSRPReportStackUsageIfDue();

        spin_ms( T4_NON_CRIT_MS );

        configASSERT( xSemaphoreTakeSRP( xR2Semaphore, 0U, SRP_RESOURCE_R2 ) == pdPASS );
        spin_ms( T4_R2_CS_MS );
        configASSERT( xSemaphoreGiveSRP( xR2Semaphore, SRP_RESOURCE_R2 ) == pdPASS );

        configASSERT( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( COMMON_PERIOD_MS ) ) != pdFALSE );
    }
}

static void vSRP2Task5( void * pvParameters )
{
    TickType_t xLastWakeTime;

    ( void ) pvParameters;
    xLastWakeTime = xSynchronousReleaseTick;

    for( ;; )
    {
        vSRPReportStackUsageIfDue();

        spin_ms( T5_NON_CRIT_MS );

        configASSERT( xSemaphoreTakeSRP( xR1Semaphore, 0U, SRP_RESOURCE_R1 ) == pdPASS );
        spin_ms( T5_R1_CS_MS );
        configASSERT( xSemaphoreGiveSRP( xR1Semaphore, SRP_RESOURCE_R1 ) == pdPASS );
        
        spin_ms(T5_PREMPTED_NON_CRIT_MS); // Add a non-critical section after resource release to allow Task 6 to preempt

        configASSERT( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( COMMON_PERIOD_MS ) ) != pdFALSE );
    }
}

static void vSRP2Task6( void * pvParameters )
{
    TickType_t xLastWakeTime;

    ( void ) pvParameters;
    xLastWakeTime = xSynchronousReleaseTick;

    /* Phase Task 6 so its release coincides with Task 5 reaching R1. */
    configASSERT( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( T6_RELEASE_OFFSET_MS ) ) != pdFALSE );

    for( ;; )
    {
        vSRPReportStackUsageIfDue();

        spin_ms( T6_NON_CRIT_MS );

        configASSERT( xSemaphoreTakeSRP( xR1Semaphore, 0U, SRP_RESOURCE_R1 ) == pdPASS );
        spin_ms( T6_R1_CS_MS );
        configASSERT( xSemaphoreGiveSRP( xR1Semaphore, SRP_RESOURCE_R1 ) == pdPASS );

        configASSERT( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( COMMON_PERIOD_MS ) ) != pdFALSE );
    }
}

void srp_2_run( void )
{
    TaskHandle_t xTaskHandles[ SAME_LEVEL_TASK_COUNT ] = { NULL };

    stdio_init_all();
    vTraceTaskPinsInit();

    xR1Semaphore = xSemaphoreCreateBinary();
    xR2Semaphore = xSemaphoreCreateBinary();

    configASSERT( xR1Semaphore != NULL );
    configASSERT( xR2Semaphore != NULL );

    /* FreeRTOS binary semaphores start empty when created with xSemaphoreCreateBinary(). */
    configASSERT( xSemaphoreGive( xR1Semaphore ) == pdTRUE );
    configASSERT( xSemaphoreGive( xR2Semaphore ) == pdTRUE );

    /* Keep the task-local delay-until epochs aligned with the kernel's initial
     * synchronous job releases for this same-deadline stress test. */
    xSynchronousReleaseTick = xTaskGetTickCount();

    /* Tasks 1-5 remain the same-deadline shared-stack stress case. Task 6 keeps
     * the same period, but uses a shorter constrained deadline and a phased
     * release so it becomes ready when Task 5 enters R1. */
    configASSERT( xTaskCreate( vSRP2Task1,
                               "SRP2 T1",
                               STACK_DEPTH,
                               NULL,
                               &xTaskHandles[ 0 ],
                               COMMON_PERIOD_MS,
                               T1_WCET_MS,
                               COMMON_DEADLINE_MS,
                               xTask1Claims,
                               ARRAY_LENGTH( xTask1Claims ) ) == pdPASS );

    configASSERT( xTaskCreate( vSRP2Task2,
                               "SRP2 T2",
                               STACK_DEPTH,
                               NULL,
                               &xTaskHandles[ 1 ],
                               COMMON_PERIOD_MS,
                               T2_WCET_MS,
                               COMMON_DEADLINE_MS,
                               xTask2Claims,
                               ARRAY_LENGTH( xTask2Claims ) ) == pdPASS );

    configASSERT( xTaskCreate( vSRP2Task3,
                               "SRP2 T3",
                               STACK_DEPTH,
                               NULL,
                               &xTaskHandles[ 2 ],
                               COMMON_PERIOD_MS,
                               T3_WCET_MS,
                               COMMON_DEADLINE_MS,
                               xTask3Claims,
                               ARRAY_LENGTH( xTask3Claims ) ) == pdPASS );

    configASSERT( xTaskCreate( vSRP2Task4,
                               "SRP2 T4",
                               STACK_DEPTH,
                               NULL,
                               &xTaskHandles[ 3 ],
                               COMMON_PERIOD_MS,
                               T4_WCET_MS,
                               COMMON_DEADLINE_MS,
                               xTask4Claims,
                               ARRAY_LENGTH( xTask4Claims ) ) == pdPASS );

    configASSERT( xTaskCreate( vSRP2Task5,
                               "SRP2 T5",
                               STACK_DEPTH,
                               NULL,
                               &xTaskHandles[ 4 ],
                               COMMON_PERIOD_MS,
                               T5_WCET_MS,
                               COMMON_DEADLINE_MS,
                               xTask5Claims,
                               ARRAY_LENGTH( xTask5Claims ) ) == pdPASS );

    configASSERT( xTaskCreate( vSRP2Task6,
                               "SRP2 T6",
                               STACK_DEPTH,
                               NULL,
                               &xTaskHandles[ 5 ],
                               COMMON_PERIOD_MS,
                               T6_WCET_MS,
                               T6_DEADLINE_MS,
                               xTask6Claims,
                               ARRAY_LENGTH( xTask6Claims ) ) == pdPASS );

    vTaskSetApplicationTaskTag( xTaskHandles[ 0 ], ( TaskHookFunction_t ) TRACE_TASK_T1 );
    vTaskSetApplicationTaskTag( xTaskHandles[ 1 ], ( TaskHookFunction_t ) TRACE_TASK_T2 );
    vTaskSetApplicationTaskTag( xTaskHandles[ 2 ], ( TaskHookFunction_t ) TRACE_TASK_T3 );
    vTaskSetApplicationTaskTag( xTaskHandles[ 3 ], ( TaskHookFunction_t ) TRACE_TASK_T4 );
    vTaskSetApplicationTaskTag( xTaskHandles[ 4 ], ( TaskHookFunction_t ) TRACE_TASK_T5 );
    vTaskSetApplicationTaskTag( xTaskHandles[ 5 ], ( TaskHookFunction_t ) TRACE_TASK_T6 );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#endif
