#include "mp_tests/global_edf_tests/test_3.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( PARTITIONED_EDF_ENABLE == 0U ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * Global EDF migration test.
 *
 * Task set:
 * - Migrator M: unrestricted, T=3000 ms, C=700 ms,  D=T, initial delay 0 ms,    tag 1
 * - Blocker B0: pinned to core 0, T=5000 ms, C=2500 ms, D=T, initial delay 0 ms, tag 2
 * - Blocker B1: pinned to core 1, T=5000 ms, C=2500 ms, D=T, initial delay 2000 ms, tag 4
 *
 * Desired observations:
 * - M should run on one core during its first job.
 * - On a later release, because the opposite core is the only eligible free core,
 *   M should appear on the other core bank.
 * - This demonstrates job migration for an unrestricted task under global EDF.
 */

#define GLOB3_STACK_DEPTH     256u

typedef struct MPGlobMigrationTaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulWcetMs;
    uint32_t ulInitialDelayMs;
    UBaseType_t uxCoreAffinityMask;
} MPGlobMigrationTaskConfig_t;

static TickType_t xMpGlob3SharedAnchorTick = 0u;

static void vMPGlobMigrationTask( void * pvParameters )
{
    const MPGlobMigrationTaskConfig_t * pxCfg = ( const MPGlobMigrationTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    configASSERT( pxCfg != NULL );

    if( pxCfg->ulInitialDelayMs > 0u )
    {
        vTaskDelay( pdMS_TO_TICKS( pxCfg->ulInitialDelayMs ) );
    }

    xLastWakeTime = xMpGlob3SharedAnchorTick + pdMS_TO_TICKS( pxCfg->ulInitialDelayMs );

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

void mp_global_edf_3_run( void )
{
    static const MPGlobMigrationTaskConfig_t xTaskCfgs[] =
    {
        { "G Migrator", 1u, 3000u, 700u, 0u, tskNO_AFFINITY },
        { "G Blocker0", 2u, 5000u, 2500u, 0u, ( UBaseType_t ) 1u << 0u },
        { "G Blocker1", 4u, 5000u, 2500u, 2000u, ( UBaseType_t ) 1u << 1u }
    };
    UBaseType_t uxIndex;

    stdio_init_all();
    vTraceTaskPinsInit();
    xMpGlob3SharedAnchorTick = xTaskGetTickCount();

    for( uxIndex = 0u; uxIndex < ( UBaseType_t ) ( sizeof( xTaskCfgs ) / sizeof( xTaskCfgs[ 0 ] ) ); uxIndex++ )
    {
        TaskHandle_t xHandle = NULL;

        ( void ) xTaskCreate( vMPGlobMigrationTask,
                              xTaskCfgs[ uxIndex ].pcName,
                              GLOB3_STACK_DEPTH,
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

#endif /* global EDF migration test */
