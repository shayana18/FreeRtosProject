#include "mp_tests/partitioned_edf_tests/test_2.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( PARTITIONED_EDF_ENABLE == 1U ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * Partitioned EDF explicit migration test.
 *
 * Task set:
 * - Migrating task M: initially pinned to core 0, T=4000 ms, C=900 ms,  D=T, tag 1
 * - Core-0 background: pinned to core 0, T=5000 ms, C=1200 ms, D=T, tag 2
 * - Core-1 background: pinned to core 1, T=5000 ms, C=1200 ms, D=T, tag 4
 *
 * Desired observations:
 * - Before migration, tag 1 should appear only on core 0's bank.
 * - After the migrating task completes five jobs, it changes its own affinity
 *   to core 1.
 * - After that change, later jobs of tag 1 should stop
 *   appearing on core 0 and should appear only on core 1.
 */

#define PART2_STACK_DEPTH           256u

typedef struct MPPartMigrationTaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulWcetMs;
    UBaseType_t uxCoreAffinityMask;
    uint32_t ulMigrateAfterJobs;
    UBaseType_t uxMigrationTargetMask;
} MPPartMigrationTaskConfig_t;

static TaskHandle_t xMigratingTaskHandle = NULL;
static TickType_t xMpPart2SharedAnchorTick = 0u;

static void vMPPartMigrationTask( void * pvParameters )
{
    const MPPartMigrationTaskConfig_t * pxCfg = ( const MPPartMigrationTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;
    uint32_t ulCompletedJobs = 0u;

    configASSERT( pxCfg != NULL );

    xLastWakeTime = xMpPart2SharedAnchorTick;

    for( ;; )
    {
        spin_ms( pxCfg->ulWcetMs );

        ulCompletedJobs++;

        if( ( pxCfg->ulMigrateAfterJobs != 0u ) &&
            ( ulCompletedJobs == pxCfg->ulMigrateAfterJobs ) )
        {
            vTaskCoreAffinitySet( xTaskGetCurrentTaskHandle(),
                                  pxCfg->uxMigrationTargetMask );
        }

        xDelayResult = xTaskDelayUntil( &xLastWakeTime,
                                        pdMS_TO_TICKS( pxCfg->ulPeriodMs ) );

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void mp_partitioned_edf_2_run( void )
{
    static const MPPartMigrationTaskConfig_t xMigratingCfg =
    {
        .pcName = "P Migrator",
        .ulTag = 1u,
        .ulPeriodMs = 4000u,
        .ulWcetMs = 900u,
        .uxCoreAffinityMask = ( UBaseType_t ) 1u << 0u,
        .ulMigrateAfterJobs = 5u,
        .uxMigrationTargetMask = ( UBaseType_t ) 1u << 1u
    };
    static const MPPartMigrationTaskConfig_t xCore0Cfg =
    {
        .pcName = "P Core0",
        .ulTag = 2u,
        .ulPeriodMs = 5000u,
        .ulWcetMs = 1200u,
        .uxCoreAffinityMask = ( UBaseType_t ) 1u << 0u,
        .ulMigrateAfterJobs = 0u,
        .uxMigrationTargetMask = 0u
    };
    static const MPPartMigrationTaskConfig_t xCore1Cfg =
    {
        .pcName = "P Core1",
        .ulTag = 4u,
        .ulPeriodMs = 5000u,
        .ulWcetMs = 1200u,
        .uxCoreAffinityMask = ( UBaseType_t ) 1u << 1u,
        .ulMigrateAfterJobs = 0u,
        .uxMigrationTargetMask = 0u
    };
    TaskHandle_t xCore0Handle = NULL;
    TaskHandle_t xCore1Handle = NULL;

    stdio_init_all();
    vTraceTaskPinsInit();
    xMpPart2SharedAnchorTick = xTaskGetTickCount();

    ( void ) xTaskCreate( vMPPartMigrationTask,
                          xMigratingCfg.pcName,
                          PART2_STACK_DEPTH,
                          ( void * ) &xMigratingCfg,
                          &xMigratingTaskHandle,
                          xMigratingCfg.ulPeriodMs,
                          xMigratingCfg.ulWcetMs,
                          xMigratingCfg.ulPeriodMs,
                          xMigratingCfg.uxCoreAffinityMask );

    ( void ) xTaskCreate( vMPPartMigrationTask,
                          xCore0Cfg.pcName,
                          PART2_STACK_DEPTH,
                          ( void * ) &xCore0Cfg,
                          &xCore0Handle,
                          xCore0Cfg.ulPeriodMs,
                          xCore0Cfg.ulWcetMs,
                          xCore0Cfg.ulPeriodMs,
                          xCore0Cfg.uxCoreAffinityMask );

    ( void ) xTaskCreate( vMPPartMigrationTask,
                          xCore1Cfg.pcName,
                          PART2_STACK_DEPTH,
                          ( void * ) &xCore1Cfg,
                          &xCore1Handle,
                          xCore1Cfg.ulPeriodMs,
                          xCore1Cfg.ulWcetMs,
                          xCore1Cfg.ulPeriodMs,
                          xCore1Cfg.uxCoreAffinityMask );

    if( xMigratingTaskHandle != NULL )
    {
        vTaskSetApplicationTaskTag( xMigratingTaskHandle,
                                    ( TaskHookFunction_t ) ( uintptr_t ) xMigratingCfg.ulTag );
    }

    if( xCore0Handle != NULL )
    {
        vTaskSetApplicationTaskTag( xCore0Handle,
                                    ( TaskHookFunction_t ) ( uintptr_t ) xCore0Cfg.ulTag );
    }

    if( xCore1Handle != NULL )
    {
        vTaskSetApplicationTaskTag( xCore1Handle,
                                    ( TaskHookFunction_t ) ( uintptr_t ) xCore1Cfg.ulTag );
    }

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#endif /* partitioned EDF explicit migration test */
