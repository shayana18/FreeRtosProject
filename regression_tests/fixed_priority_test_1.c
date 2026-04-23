#include "regression_tests/fixed_priority_test_1.h"

#if ( ( configUSE_EDF == 0U ) && ( configUSE_UP == 0U ) && ( configUSE_MP == 0U ) && \
      ( configUSE_SRP == 0U ) && ( configUSE_CBS == 0U ) && \
      ( GLOBAL_EDF_ENABLE == 0U ) && ( PARTITIONED_EDF_ENABLE == 0U ) && \
      ( configUSE_SRP_SHARED_STACKS == 0U ) )

#include <stdint.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * Fixed-priority regression test for the all-zero scheduler configuration.
 * The high-priority task starts blocked, the low-priority task runs, then the
 * high-priority task wakes and preempts it.
 */

#define FP1_STACK_DEPTH          256u
#define FP1_HIGH_PRIORITY       ( tskIDLE_PRIORITY + 3u )
#define FP1_LOW_PRIORITY        ( tskIDLE_PRIORITY + 1u )

static volatile uint32_t ulFP1LowRuns = 0u;
static volatile uint32_t ulFP1HighRuns = 0u;

static void vFP1HighTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 500u ) );
        configASSERT( ulFP1LowRuns > 0u );
        ulFP1HighRuns++;
        vTraceUsbPrint( "[FP1] high-priority task ran after low, high=%lu low=%lu\r\n",
                        ( unsigned long ) ulFP1HighRuns,
                        ( unsigned long ) ulFP1LowRuns );
        spin_ms( 50u );
    }
}

static void vFP1LowTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        ulFP1LowRuns++;
        spin_ms( 50u );
        vTaskDelay( pdMS_TO_TICKS( 100u ) );
    }
}

void fixed_priority_test_1_run( void )
{
    TaskHandle_t xHighHandle = NULL;
    TaskHandle_t xLowHandle = NULL;

    stdio_init_all();
    vTraceTaskPinsInit();

    configASSERT( xTaskCreate( vFP1LowTask,
                               "FP1 LOW",
                               FP1_STACK_DEPTH,
                               NULL,
                               FP1_LOW_PRIORITY,
                               &xLowHandle ) == pdPASS );
    configASSERT( xTaskCreate( vFP1HighTask,
                               "FP1 HIGH",
                               FP1_STACK_DEPTH,
                               NULL,
                               FP1_HIGH_PRIORITY,
                               &xHighHandle ) == pdPASS );

    vTaskSetApplicationTaskTag( xLowHandle, ( TaskHookFunction_t ) 1 );
    vTaskSetApplicationTaskTag( xHighHandle, ( TaskHookFunction_t ) 2 );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#else

void fixed_priority_test_1_run( void )
{
}

#endif
