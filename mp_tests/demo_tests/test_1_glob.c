#include "mp_tests/demo_tests/test_1_glob.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( GLOBAL_EDF_ENABLE == 1U ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * Demo test 1: global EDF placement and dispatch.
 *
 * Task set:
 * - GDemoA: T=4000 ms, C=1200 ms, D=T, no explicit affinity, tag 1
 * - GDemoB: T=5000 ms, C=1000 ms, D=T, no explicit affinity, tag 2
 * - GDemoC: T=8000 ms, C=800 ms,  D=T, no explicit affinity, tag 4
 *
 * Total utilization:
 * - 0.30 + 0.20 + 0.10 = 0.60 < 1.00
 *
 * Desired observations:
 * - Even though the total utilization is less than one core, global EDF keeps
 *   the tasks globally runnable rather than assigning them to one partition.
 * - At startup, the two earliest-deadline jobs (tags 1 and 2) should appear on
 *   the two core GPIO banks simultaneously.
 * - Tag 4 should appear only after one of the earlier-deadline jobs completes.
 */

#define DEMO_GLOB1_STACK_DEPTH    256u

typedef struct MPDemoGlobTaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulWcetMs;
    uint32_t ulWorkMs;
} MPDemoGlobTaskConfig_t;

static TickType_t xMpDemoGlob1SharedAnchorTick = 0u;

static void vMPDemoGlob1Task( void * pvParameters )
{
    const MPDemoGlobTaskConfig_t * pxCfg = ( const MPDemoGlobTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    configASSERT( pxCfg != NULL );

    xLastWakeTime = xMpDemoGlob1SharedAnchorTick;

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

void mp_demo_test_1_glob_run( void )
{
    static const MPDemoGlobTaskConfig_t xTaskCfgs[] =
    {
        { "GDemoA", 1u, 4000u, 1200u, 1050u },
        { "GDemoB", 2u, 5000u, 1000u, 850u },
        { "GDemoC", 4u, 8000u, 800u, 650u }
    };
    UBaseType_t uxIndex;

    stdio_init_all();
    vTraceTaskPinsInit();
    xMpDemoGlob1SharedAnchorTick = xTaskGetTickCount();

    for( uxIndex = 0u; uxIndex < ( UBaseType_t ) ( sizeof( xTaskCfgs ) / sizeof( xTaskCfgs[ 0 ] ) ); uxIndex++ )
    {
        TaskHandle_t xHandle = NULL;

        ( void ) xTaskCreate( vMPDemoGlob1Task,
                              xTaskCfgs[ uxIndex ].pcName,
                              DEMO_GLOB1_STACK_DEPTH,
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

#endif /* demo global EDF test 1 */
