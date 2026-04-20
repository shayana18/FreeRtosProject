#include "mp_tests/partitioned_edf_tests/test_1.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( PARTITIONED_EDF_ENABLE == 1U ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * Partitioned EDF basic dispatch test.
 *
 * Task set:
 * - P0A: pinned to core 0, T=4000 ms, C=1000 ms, D=T, tag 1
 * - P0B: pinned to core 0, T=6000 ms, C=900 ms,  D=T, tag 2
 * - P1A: pinned to core 1, T=5000 ms, C=1000 ms, D=T, tag 4
 *
 * Desired observations:
 * - Tags 1 and 2 should only ever appear on core 0's GPIO bank.
 * - Tag 4 should only ever appear on core 1's GPIO bank.
 * - No task should spontaneously appear on the other core's bank.
 */

#define PART1_STACK_DEPTH     256u

typedef struct MPPartBasicTaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulWcetMs;
    UBaseType_t uxCoreAffinityMask;
} MPPartBasicTaskConfig_t;

static TickType_t xMpPart1SharedAnchorTick = 0u;

static void vMPPartBasicTask( void * pvParameters )
{
    const MPPartBasicTaskConfig_t * pxCfg = ( const MPPartBasicTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    configASSERT( pxCfg != NULL );

    xLastWakeTime = xMpPart1SharedAnchorTick;

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

void mp_partitioned_edf_1_run( void )
{
    static const MPPartBasicTaskConfig_t xTaskCfgs[] =
    {
        { "P0A", 1u, 4000u, 1000u, ( UBaseType_t ) 1u << 0u },
        { "P0B", 2u, 6000u, 900u, ( UBaseType_t ) 1u << 0u },
        { "P1A", 4u, 5000u, 1000u, ( UBaseType_t ) 1u << 1u }
    };
    UBaseType_t uxIndex;

    stdio_init_all();
    vTraceTaskPinsInit();
    xMpPart1SharedAnchorTick = xTaskGetTickCount();

    for( uxIndex = 0u; uxIndex < ( UBaseType_t ) ( sizeof( xTaskCfgs ) / sizeof( xTaskCfgs[ 0 ] ) ); uxIndex++ )
    {
        TaskHandle_t xHandle = NULL;

        ( void ) xTaskCreate( vMPPartBasicTask,
                              xTaskCfgs[ uxIndex ].pcName,
                              PART1_STACK_DEPTH,
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

#endif /* partitioned EDF basic dispatch test */
