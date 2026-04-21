#include "mp_tests/global_edf_tests/test_2.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( PARTITIONED_EDF_ENABLE == 0U ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * Global EDF preemption test.
 *
 * Task set:
 * - G1: T=8000 ms, C=3000 ms, D=T, unrestricted, initial delay 0 ms,   tag 1
 * - G2: T=9000 ms, C=3000 ms, D=T, unrestricted, initial delay 0 ms,   tag 2
 * - G3: T=2000 ms, C=500 ms,  D=T, unrestricted, initial delay 800 ms, tag 4
 *
 * Desired observations:
 * - At startup, tags 1 and 2 should occupy the two cores.
 * - When tag 4 is released after its initial delay, one core should switch
 *   immediately from a worse current job to tag 4.
 */

#define GLOB2_STACK_DEPTH     256u

typedef struct MPGlobPreemptTaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulWcetMs;
    uint32_t ulInitialDelayMs;
} MPGlobPreemptTaskConfig_t;

static TickType_t xMpGlob2SharedAnchorTick = 0u;

static void vMPGlobPreemptTask( void * pvParameters )
{
    const MPGlobPreemptTaskConfig_t * pxCfg = ( const MPGlobPreemptTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    configASSERT( pxCfg != NULL );

    if( pxCfg->ulInitialDelayMs > 0u )
    {
        vTaskDelay( pdMS_TO_TICKS( pxCfg->ulInitialDelayMs ) );
    }

    xLastWakeTime = xMpGlob2SharedAnchorTick + pdMS_TO_TICKS( pxCfg->ulInitialDelayMs );

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

void mp_global_edf_2_run( void )
{
    static const MPGlobPreemptTaskConfig_t xTaskCfgs[] =
    {
        { "G Preempt 1", 1u, 8000u, 3000u, 0u },
        { "G Preempt 2", 2u, 9000u, 3000u, 0u },
        { "G Preempt 3", 4u, 2000u, 500u, 800u }
    };
    UBaseType_t uxIndex;

    stdio_init_all();
    vTraceTaskPinsInit();
    xMpGlob2SharedAnchorTick = xTaskGetTickCount();

    for( uxIndex = 0u; uxIndex < ( UBaseType_t ) ( sizeof( xTaskCfgs ) / sizeof( xTaskCfgs[ 0 ] ) ); uxIndex++ )
    {
        TaskHandle_t xHandle = NULL;

        ( void ) xTaskCreate( vMPGlobPreemptTask,
                              xTaskCfgs[ uxIndex ].pcName,
                              GLOB2_STACK_DEPTH,
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

#endif /* global EDF preemption test */
