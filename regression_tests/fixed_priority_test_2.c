#include "regression_tests/fixed_priority_test_2.h"

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
 * Fixed-priority delayed-unblock regression test.
 * Low priority runs first, then medium wakes, then high wakes. Each higher
 * priority task asserts that the lower priority level already made progress.
 */

#define FP2_STACK_DEPTH          256u
#define FP2_HIGH_PRIORITY       ( tskIDLE_PRIORITY + 3u )
#define FP2_MED_PRIORITY        ( tskIDLE_PRIORITY + 2u )
#define FP2_LOW_PRIORITY        ( tskIDLE_PRIORITY + 1u )

static volatile uint32_t ulFP2LowRuns = 0u;
static volatile uint32_t ulFP2MediumRuns = 0u;
static volatile uint32_t ulFP2HighRuns = 0u;

static void vFP2HighTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 1000u ) );
        configASSERT( ulFP2MediumRuns > 0u );
        ulFP2HighRuns++;
        vTraceUsbPrint( "[FP2] high wake count=%lu medium=%lu low=%lu\r\n",
                        ( unsigned long ) ulFP2HighRuns,
                        ( unsigned long ) ulFP2MediumRuns,
                        ( unsigned long ) ulFP2LowRuns );
        spin_ms( 40u );
    }
}

static void vFP2MediumTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 500u ) );
        configASSERT( ulFP2LowRuns > 0u );
        ulFP2MediumRuns++;
        spin_ms( 40u );
    }
}

static void vFP2LowTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        ulFP2LowRuns++;
        spin_ms( 40u );
        vTaskDelay( pdMS_TO_TICKS( 100u ) );
    }
}

void fixed_priority_test_2_run( void )
{
    TaskHandle_t xHighHandle = NULL;
    TaskHandle_t xMediumHandle = NULL;
    TaskHandle_t xLowHandle = NULL;

    stdio_init_all();
    vTraceTaskPinsInit();

    configASSERT( xTaskCreate( vFP2LowTask,
                               "FP2 LOW",
                               FP2_STACK_DEPTH,
                               NULL,
                               FP2_LOW_PRIORITY,
                               &xLowHandle ) == pdPASS );
    configASSERT( xTaskCreate( vFP2MediumTask,
                               "FP2 MED",
                               FP2_STACK_DEPTH,
                               NULL,
                               FP2_MED_PRIORITY,
                               &xMediumHandle ) == pdPASS );
    configASSERT( xTaskCreate( vFP2HighTask,
                               "FP2 HIGH",
                               FP2_STACK_DEPTH,
                               NULL,
                               FP2_HIGH_PRIORITY,
                               &xHighHandle ) == pdPASS );

    vTaskSetApplicationTaskTag( xLowHandle, ( TaskHookFunction_t ) 1 );
    vTaskSetApplicationTaskTag( xMediumHandle, ( TaskHookFunction_t ) 2 );
    vTaskSetApplicationTaskTag( xHighHandle, ( TaskHookFunction_t ) 4 );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#else

void fixed_priority_test_2_run( void )
{
}

#endif
