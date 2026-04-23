#include "mp_tests/global_edf_tests/test_5.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( PARTITIONED_EDF_ENABLE == 0U ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * Global EDF affinity-enforcement test.
 *
 * Task set:
 * - G5 Free0:  T=4000 ms,  C=1000 ms, D=T, unrestricted,     tag 1
 * - G5 Core0:  T=4000 ms,  C=1200 ms, D=T, pinned to core 0, tag 2
 * - G5 Core1:  T=6000 ms,  C=1500 ms, D=T, pinned to core 1, tag 4
 * - G5 Free1:  T=12000 ms, C=800 ms,  D=T, unrestricted,     tag 8
 *
 * Hyperperiod: LCM(4000, 4000, 6000, 12000) = 12 000 ms.
 * Total utilisation: 0.25 + 0.30 + 0.25 + 0.067 ≈ 0.87 across 2 cores (schedulable).
 *
 * Desired observations:
 * - G5 Core0 (tag 2) must appear ONLY in the core-0 GPIO bank; never on core 1.
 * - G5 Core1 (tag 4) must appear ONLY in the core-1 GPIO bank; never on core 0.
 * - G5 Free0 (tag 1) and G5 Free1 (tag 8) may appear on either core depending on
 *   which core holds the earliest-deadline slot that admits an unrestricted task.
 * - This confirms that per-task affinity masks are honoured by the global EDF
 *   scheduler even when a less-constrained scheduling slot exists on the pinned core.
 */

#define GLOB5_STACK_DEPTH     256u

typedef struct MPGlobAffinityTaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulWcetMs;
    UBaseType_t uxCoreAffinityMask;
} MPGlobAffinityTaskConfig_t;

static TickType_t xGlob5SharedAnchorTick = 0u;

static void vMPGlobAffinityTask( void * pvParameters )
{
    const MPGlobAffinityTaskConfig_t * pxCfg = ( const MPGlobAffinityTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    configASSERT( pxCfg != NULL );

    xLastWakeTime = xGlob5SharedAnchorTick;

    for( ;; )
    {
        spin_ms( pxCfg->ulWcetMs );

        xDelayResult = xTaskDelayUntil( &xLastWakeTime,
                                        pdMS_TO_TICKS( pxCfg->ulPeriodMs ) );

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void mp_global_edf_5_run( void )
{
    static const MPGlobAffinityTaskConfig_t xTaskCfgs[] =
    {
        { "G5 Free0", 1u,  4000u, 1000u, tskNO_AFFINITY          },
        { "G5 Core0", 2u,  4000u, 1200u, ( UBaseType_t ) 1u << 0u },
        { "G5 Core1", 4u,  6000u, 1500u, ( UBaseType_t ) 1u << 1u },
        { "G5 Free1", 8u, 12000u,  800u, tskNO_AFFINITY          }
    };
    UBaseType_t uxIndex;

    stdio_init_all();
    vTraceTaskPinsInit();
    xGlob5SharedAnchorTick = xTaskGetTickCount();

    for( uxIndex = 0u; uxIndex < ( UBaseType_t ) ( sizeof( xTaskCfgs ) / sizeof( xTaskCfgs[ 0 ] ) ); uxIndex++ )
    {
        TaskHandle_t xHandle = NULL;

        ( void ) xTaskCreate( vMPGlobAffinityTask,
                              xTaskCfgs[ uxIndex ].pcName,
                              GLOB5_STACK_DEPTH,
                              ( void * ) &xTaskCfgs[ uxIndex ],
                              &xHandle,
                              xTaskCfgs[ uxIndex ].ulPeriodMs,
                              xTaskCfgs[ uxIndex ].ulWcetMs,
                              xTaskCfgs[ uxIndex ].ulPeriodMs,
                              xTaskCfgs[ uxIndex ].uxCoreAffinityMask );

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

#endif /* global EDF affinity-enforcement test */
