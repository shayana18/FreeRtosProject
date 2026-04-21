#include "mp_tests/global_edf_tests/test_1.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( GLOBAL_EDF_ENABLE == 1U ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * Global EDF basic dispatch test.
 *
 * Task set:
 * - G1: T=4000 ms, C=1200 ms, D=T, unrestricted, tag 1
 * - G2: T=5000 ms, C=1200 ms, D=T, unrestricted, tag 2
 * - G3: T=9000 ms, C=700 ms,  D=T, unrestricted, tag 4
 *
 * Desired observations:
 * - At startup, the two earliest-deadline jobs (tags 1 and 2) should occupy the
 *   two core GPIO banks simultaneously.
 * - Tag 4 should only appear once one of those earlier-deadline jobs completes.
 */

#define GLOB1_STACK_DEPTH     256u

static TickType_t xGlob1SharedAnchorTick = 0;

typedef struct MPGlobBasicTaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulWcetMs;
} MPGlobBasicTaskConfig_t;

static void vMPGlobBasicTask( void * pvParameters )
{
    const MPGlobBasicTaskConfig_t * pxCfg = ( const MPGlobBasicTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    configASSERT( pxCfg != NULL );

    xLastWakeTime = xGlob1SharedAnchorTick;

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

void mp_global_edf_1_run( void )
{
    static const MPGlobBasicTaskConfig_t xTaskCfgs[] =
    {
        { "G1", 1u, 4000u, 1200u },
        { "G2", 2u, 5000u, 1200u },
        { "G3", 4u, 9000u, 700u }
    };
    UBaseType_t uxIndex;

    stdio_init_all();
    vTraceTaskPinsInit();
    xGlob1SharedAnchorTick = xTaskGetTickCount();

    for( uxIndex = 0u; uxIndex < ( UBaseType_t ) ( sizeof( xTaskCfgs ) / sizeof( xTaskCfgs[ 0 ] ) ); uxIndex++ )
    {
        TaskHandle_t xHandle = NULL;

        ( void ) xTaskCreate( vMPGlobBasicTask,
                              xTaskCfgs[ uxIndex ].pcName,
                              GLOB1_STACK_DEPTH,
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

#endif /* global EDF basic dispatch test */
