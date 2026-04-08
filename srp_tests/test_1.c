#include "srp_tests/test_1.h"

#if ( ( configUSE_EDF == 1 ) && ( configUSE_SRP == 1 ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "test_utils.h"
#include "task_trace.h"

#define STACK_DEPTH             512u
#define WCET_SLACK              500u

#define T1_PERIOD_MS           4000u
#define T1_DEADLINE_MS         4000u
#define T1_R1_CS_MS             500u
#define T1_NON_CRIT_MS          500u
#define T1_WCET_MS             ( T1_R1_CS_MS + T1_NON_CRIT_MS + WCET_SLACK )

#define T2_PERIOD_MS           6000u
#define T2_DEADLINE_MS         6000u
#define T2_R1_CS_MS             500u
#define T2_R2_CS_MS             500u
#define T2_WCET_MS             ( T2_R1_CS_MS + T2_R2_CS_MS + WCET_SLACK )

#define T3_PERIOD_MS          12000u
#define T3_DEADLINE_MS        12000u
#define T3_R2_CS_MS             500u
#define T3_NON_CRIT_MS         1500u
#define T3_WCET_MS             ( T3_NON_CRIT_MS + T3_R2_CS_MS + WCET_SLACK )

#define SRP_RESOURCE_R1           0u
#define SRP_RESOURCE_R2           1u

#define TRACE_TASK_T1             1u
#define TRACE_TASK_T2             2u
#define TRACE_TASK_T3             4u

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
    { SRP_RESOURCE_R1, pdMS_TO_TICKS( T2_R1_CS_MS ) },
    { SRP_RESOURCE_R2, pdMS_TO_TICKS( T2_R2_CS_MS ) }
};

static const SRPResourceClaim_t xTask3Claims[] =
{
    { SRP_RESOURCE_R2, pdMS_TO_TICKS( T3_R2_CS_MS ) }
};

static void vSRPTask1( void * pvParameters )
{
    TickType_t xLastWakeTime;

    ( void ) pvParameters;
    xLastWakeTime = xSynchronousReleaseTick;

    for( ;; )
    {
        configASSERT( xSemaphoreTakeSRP( xR1Semaphore, 0U, SRP_RESOURCE_R1 ) == pdPASS );
        spin_ms( T1_R1_CS_MS );
        configASSERT( xSemaphoreGiveSRP( xR1Semaphore, SRP_RESOURCE_R1 ) == pdPASS );

        spin_ms( T1_NON_CRIT_MS );
        configASSERT( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( T1_PERIOD_MS ) ) != pdFALSE );
    }
}

static void vSRPTask2( void * pvParameters )
{
    TickType_t xLastWakeTime;

    ( void ) pvParameters;
    xLastWakeTime = xSynchronousReleaseTick;

    for( ;; )
    {
        configASSERT( xSemaphoreTakeSRP( xR1Semaphore, 0U, SRP_RESOURCE_R1 ) == pdPASS );
        spin_ms( T2_R1_CS_MS );
        configASSERT( xSemaphoreGiveSRP( xR1Semaphore, SRP_RESOURCE_R1 ) == pdPASS );

        configASSERT( xSemaphoreTakeSRP( xR2Semaphore, 0U, SRP_RESOURCE_R2 ) == pdPASS );
        spin_ms( T2_R2_CS_MS );
        configASSERT( xSemaphoreGiveSRP( xR2Semaphore, SRP_RESOURCE_R2 ) == pdPASS );

        configASSERT( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( T2_PERIOD_MS ) ) != pdFALSE );
    }
}

static void vSRPTask3( void * pvParameters )
{
    TickType_t xLastWakeTime;

    ( void ) pvParameters;
    xLastWakeTime = xSynchronousReleaseTick;

    for( ;; )
    {
        spin_ms( T3_NON_CRIT_MS );

        configASSERT( xSemaphoreTakeSRP( xR2Semaphore, 0U, SRP_RESOURCE_R2 ) == pdPASS );
        spin_ms( T3_R2_CS_MS );
        configASSERT( xSemaphoreGiveSRP( xR2Semaphore, SRP_RESOURCE_R2 ) == pdPASS );

        configASSERT( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( T3_PERIOD_MS ) ) != pdFALSE );
    }
}

void srp_1_run( void )
{
    TaskHandle_t xTask1Handle = NULL;
    TaskHandle_t xTask2Handle = NULL;
    TaskHandle_t xTask3Handle = NULL;

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
     * synchronous job releases for this test case. */
    xSynchronousReleaseTick = xTaskGetTickCount();

    /* This is the exact implicit-deadline task set from the schedulability figure:
     *  tau1: C=1, T=D=4,  claims R1 for 0.5
     *  tau2: C=1, T=D=6,  claims R1 and R2 for 0.5 each
     *  tau3: C=2, T=D=12, claims R2 for 0.5
     *
     * With synchronous release at t=0, the expected job-level Gantt chart over
     * the hyperperiod [0, 12s) is:
     *   [0,1)   tau1
     *   [1,2)   tau2
     *   [2,4)   tau3
     *   [4,5)   tau1
     *   [5,6)   idle
     *   [6,7)   tau2
     *   [7,8)   idle
     *   [8,9)   tau1
     *   [9,12)  idle
     *
     * So the graph should not show any extra tau3 execution after t=4 for this
     * synchronous-release test case. */
    /* The current SRP shared-stack allocator appends regions in nondecreasing
     * preemption-level order, so create tasks from longest relative deadline to
     * shortest relative deadline before starting the scheduler. */
    configASSERT( xTaskCreate( vSRPTask3,
                               "SRP1 T3",
                               STACK_DEPTH,
                               NULL,
                               &xTask3Handle,
                               T3_PERIOD_MS,
                               T3_WCET_MS,
                               T3_DEADLINE_MS,
                               xTask3Claims,
                               ARRAY_LENGTH( xTask3Claims ) ) == pdPASS );

    configASSERT( xTaskCreate( vSRPTask2,
                               "SRP1 T2",
                               STACK_DEPTH,
                               NULL,
                               &xTask2Handle,
                               T2_PERIOD_MS,
                               T2_WCET_MS,
                               T2_DEADLINE_MS,
                               xTask2Claims,
                               ARRAY_LENGTH( xTask2Claims ) ) == pdPASS );

    configASSERT( xTaskCreate( vSRPTask1,
                               "SRP1 T1",
                               STACK_DEPTH,
                               NULL,
                               &xTask1Handle,
                               T1_PERIOD_MS,
                               T1_WCET_MS,
                               T1_DEADLINE_MS,
                               xTask1Claims,
                               ARRAY_LENGTH( xTask1Claims ) ) == pdPASS );

    vTaskSetApplicationTaskTag( xTask1Handle, ( TaskHookFunction_t ) TRACE_TASK_T1 );
    vTaskSetApplicationTaskTag( xTask2Handle, ( TaskHookFunction_t ) TRACE_TASK_T2 );
    vTaskSetApplicationTaskTag( xTask3Handle, ( TaskHookFunction_t ) TRACE_TASK_T3 );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#endif
