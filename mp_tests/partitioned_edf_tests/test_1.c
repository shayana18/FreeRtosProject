#include "mp_tests/partitioned_edf_tests/test_1.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( PARTITIONED_EDF_ENABLE == 1U ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

#define  TASK1_WORK 900
#define  TASK2_WORK 800
#define  TASK3_WORK 800



/*
 * Partitioned EDF basic dispatch test.
 *
 * Task set:
 * - P0A: pinned to core 0, T=4000 ms, C=900 ms, D=T, tag 1
 * - P0B: pinned to core 0, T=6000 ms, C=800 ms,  D=T, tag 2
 * - P1A: pinned to core 1, T=3000 ms, C=800 ms, D=T, tag 4
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

static void Task1Task( void * pvParameters )
{
    const MPPartBasicTaskConfig_t * pxCfg = ( const MPPartBasicTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    configASSERT( pxCfg != NULL );

    xLastWakeTime = xMpPart1SharedAnchorTick;

    for( ;; )
    {
        spin_ms(TASK1_WORK);

        xDelayResult = xTaskDelayUntil( &xLastWakeTime,
                                        pdMS_TO_TICKS( pxCfg->ulPeriodMs ) );

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void Task2Task( void * pvParameters )
{
    const MPPartBasicTaskConfig_t * pxCfg = ( const MPPartBasicTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    configASSERT( pxCfg != NULL );

    xLastWakeTime = xMpPart1SharedAnchorTick;

    for( ;; )
    {
        spin_ms(TASK2_WORK);

        xDelayResult = xTaskDelayUntil( &xLastWakeTime,
                                        pdMS_TO_TICKS( pxCfg->ulPeriodMs ) );

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}


static void Task3Task( void * pvParameters )
{
    const MPPartBasicTaskConfig_t * pxCfg = ( const MPPartBasicTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    configASSERT( pxCfg != NULL );

    xLastWakeTime = xMpPart1SharedAnchorTick;

    for( ;; )
    {
        spin_ms(TASK3_WORK);

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
    static const MPPartBasicTaskConfig_t xTask1Cfg =
    {
        .pcName = "P0A",
        .ulTag = 1u,
        .ulPeriodMs = 4000u,
        .ulWcetMs = TASK1_WORK,
        .uxCoreAffinityMask = ( UBaseType_t ) 1u << 0u
    };
    static const MPPartBasicTaskConfig_t xTask2Cfg =
    {
        .pcName = "P0B",
        .ulTag = 2u,
        .ulPeriodMs = 6000u,
        .ulWcetMs = TASK2_WORK,
        .uxCoreAffinityMask = ( UBaseType_t ) 1u << 0u
    };
    static const MPPartBasicTaskConfig_t xTask3Cfg =
    {
        .pcName = "P1A",
        .ulTag = 4u,
        .ulPeriodMs = 3000u,
        .ulWcetMs = TASK3_WORK,
        .uxCoreAffinityMask = ( UBaseType_t ) 1u << 1u
    };
    TaskHandle_t xTask1Handle = NULL;
    TaskHandle_t xTask2Handle = NULL;
    TaskHandle_t xTask3Handle = NULL;

    stdio_init_all();
    vTraceTaskPinsInit();
    xMpPart1SharedAnchorTick = xTaskGetTickCount();

    ( void ) xTaskCreate( Task1Task,
                          xTask1Cfg.pcName,
                          PART1_STACK_DEPTH,
                          ( void * ) &xTask1Cfg,
                          &xTask1Handle,
                          xTask1Cfg.ulPeriodMs,
                          xTask1Cfg.ulWcetMs,
                          xTask1Cfg.ulPeriodMs,
                          xTask1Cfg.uxCoreAffinityMask );

    ( void ) xTaskCreate( Task2Task,
                          xTask2Cfg.pcName,
                          PART1_STACK_DEPTH,
                          ( void * ) &xTask2Cfg,
                          &xTask2Handle,
                          xTask2Cfg.ulPeriodMs,
                          xTask2Cfg.ulWcetMs,
                          xTask2Cfg.ulPeriodMs,
                          xTask2Cfg.uxCoreAffinityMask );

    ( void ) xTaskCreate( Task3Task,
                          xTask3Cfg.pcName,
                          PART1_STACK_DEPTH,
                          ( void * ) &xTask3Cfg,
                          &xTask3Handle,
                          xTask3Cfg.ulPeriodMs,
                          xTask3Cfg.ulWcetMs,
                          xTask3Cfg.ulPeriodMs,
                          xTask3Cfg.uxCoreAffinityMask );

    if( xTask1Handle != NULL )
    {
        vTaskSetApplicationTaskTag( xTask1Handle,
                                    ( TaskHookFunction_t ) ( uintptr_t ) xTask1Cfg.ulTag );
    }

    if( xTask2Handle != NULL )
    {
        vTaskSetApplicationTaskTag( xTask2Handle,
                                    ( TaskHookFunction_t ) ( uintptr_t ) xTask2Cfg.ulTag );
    }

    if( xTask3Handle != NULL )
    {
        vTaskSetApplicationTaskTag( xTask3Handle,
                                    ( TaskHookFunction_t ) ( uintptr_t ) xTask3Cfg.ulTag );
    }

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#endif /* partitioned EDF basic dispatch test */
