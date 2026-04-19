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
 * - Controller: pinned to core 0, after ~10 s calls `vTaskCoreAffinitySet( M, 1 << 1 )`, tag 7
 *
 * Desired observations:
 * - Before migration, tag 1 should appear only on core 0's bank.
 * - After the controller changes the affinity, later jobs of tag 1 should stop
 *   appearing on core 0 and should appear only on core 1.
 */

#define PART2_STACK_DEPTH           256u
#define PART2_CTRL_PERIOD_MS        30000u
#define PART2_CTRL_WCET_MS          10u

typedef struct MPPartMigrationTaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulWcetMs;
    UBaseType_t uxCoreAffinityMask;
} MPPartMigrationTaskConfig_t;

static TaskHandle_t xMigratingTaskHandle = NULL;

static void vMPPartMigrationTask( void * pvParameters )
{
    const MPPartMigrationTaskConfig_t * pxCfg = ( const MPPartMigrationTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    configASSERT( pxCfg != NULL );

    xLastWakeTime = xTaskGetTickCount();

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

static void vMPPartMigrationControllerTask( void * pvParameters )
{
    ( void ) pvParameters;

    vTaskDelay( pdMS_TO_TICKS( 10000u ) );

    if( xMigratingTaskHandle != NULL )
    {
        vTaskCoreAffinitySet( xMigratingTaskHandle, ( UBaseType_t ) 1u << 1u );
    }

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 1000u ) );
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
        .uxCoreAffinityMask = ( UBaseType_t ) 1u << 0u
    };
    static const MPPartMigrationTaskConfig_t xCore0Cfg =
    {
        .pcName = "P Core0",
        .ulTag = 2u,
        .ulPeriodMs = 5000u,
        .ulWcetMs = 1200u,
        .uxCoreAffinityMask = ( UBaseType_t ) 1u << 0u
    };
    static const MPPartMigrationTaskConfig_t xCore1Cfg =
    {
        .pcName = "P Core1",
        .ulTag = 4u,
        .ulPeriodMs = 5000u,
        .ulWcetMs = 1200u,
        .uxCoreAffinityMask = ( UBaseType_t ) 1u << 1u
    };
    TaskHandle_t xCore0Handle = NULL;
    TaskHandle_t xCore1Handle = NULL;
    TaskHandle_t xControllerHandle = NULL;

    stdio_init_all();
    vTraceTaskPinsInit();

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

    ( void ) xTaskCreate( vMPPartMigrationControllerTask,
                          "P Ctrl",
                          PART2_STACK_DEPTH,
                          NULL,
                          &xControllerHandle,
                          PART2_CTRL_PERIOD_MS,
                          PART2_CTRL_WCET_MS,
                          PART2_CTRL_PERIOD_MS,
                          ( UBaseType_t ) 1u << 0u );

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

    if( xControllerHandle != NULL )
    {
        vTaskSetApplicationTaskTag( xControllerHandle,
                                    ( TaskHookFunction_t ) ( uintptr_t ) 7u );
    }

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#endif /* partitioned EDF explicit migration test */
