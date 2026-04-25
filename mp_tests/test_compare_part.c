#include "mp_tests/test_compare_part.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( PARTITIONED_EDF_ENABLE == 1U ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * Demo test 1: partitioned EDF best-fit placement.
 *
 * Task set:
 * - PDemoA: T=4000 ms, C=1050 ms, D=T, no explicit affinity, tag 1
 * - PDemoB: T=5000 ms, C=850 ms, D=T, no explicit affinity, tag 2
 * - PDemoC: T=8000 ms, C=650 ms,  D=T, no explicit affinity, tag 4
 *
 * Total utilization:
 * - 0.30 + 0.20 + 0.10 = 0.60 < 1.00
 *
 * Desired observations:
 * - None of the tasks specify an affinity mask; partition assignment is chosen
 *   during `xTaskCreate()`.
 * - Because partitioned EDF uses best-fit online placement, all three tasks
 *   should be assigned to core 0:
 *   - after task A: core utilizations are 0.30 and 0.00
 *   - after task B: best-fit chooses core 0, giving 0.50 and 0.00
 *   - after task C: best-fit chooses core 0 again, giving 0.60 and 0.00
 * - Application task tags 1, 2, and 4 should therefore appear only on core 0's
 *   GPIO bank.
 */

#define DEMO_PART1_STACK_DEPTH    256u

typedef struct MPDemoPartTaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulWcetMs;
    uint32_t ulWorkMs;
} MPDemoPartTaskConfig_t;

static TickType_t xMpDemoPart1SharedAnchorTick = 0u;

static void vMPDemoPart1Task( void * pvParameters )
{
    const MPDemoPartTaskConfig_t * pxCfg = ( const MPDemoPartTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    configASSERT( pxCfg != NULL );

    xLastWakeTime = xMpDemoPart1SharedAnchorTick;

    for( ;; )
    {
        spin_ms( pxCfg->ulWorkMs );

        xDelayResult = xTaskDelayUntil( &xLastWakeTime,
                                        pdMS_TO_TICKS( pxCfg->ulPeriodMs ) );

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void mp_compare_part_run( void )
{
    static const MPDemoPartTaskConfig_t xTaskCfgs[] =
    {
        { "PDemoA", 1u, 4000u, 1200u, 1050u },
        { "PDemoB", 2u, 5000u, 1000u, 850u },
        { "PDemoC", 4u, 8000u, 800u, 650u }
    };
    UBaseType_t uxIndex;

    stdio_init_all();
    vTraceTaskPinsInit();
    xMpDemoPart1SharedAnchorTick = xTaskGetTickCount();

    for( uxIndex = 0u; uxIndex < ( UBaseType_t ) ( sizeof( xTaskCfgs ) / sizeof( xTaskCfgs[ 0 ] ) ); uxIndex++ )
    {
        TaskHandle_t xHandle = NULL;

        ( void ) xTaskCreate( vMPDemoPart1Task,
                              xTaskCfgs[ uxIndex ].pcName,
                              DEMO_PART1_STACK_DEPTH,
                              ( void * ) &xTaskCfgs[ uxIndex ],
                              &xHandle,
                              xTaskCfgs[ uxIndex ].ulPeriodMs,
                              xTaskCfgs[ uxIndex ].ulWcetMs,
                              xTaskCfgs[ uxIndex ].ulPeriodMs,
                              tskNO_AFFINITY );

        if( xHandle != NULL )
        {
            vTaskSetApplicationTaskTag( xHandle,
                                        ( TaskHookFunction_t ) ( uintptr_t ) xTaskCfgs[ uxIndex ].ulTag );
        }
    }

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#endif /* demo partitioned EDF test 1 */
