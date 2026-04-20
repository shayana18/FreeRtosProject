#include "mp_tests/test_2.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * MP EDF run-time task-creation test.
 *
 * Global EDF task set:
 * - Base 1: T=6000 ms, C=1800 ms, D=T, unrestricted, tag 1
 * - Base 2: T=7000 ms, C=2100 ms, D=T, unrestricted, tag 2
 * - Good runtime task: T=2000 ms, C=400 ms, D=T, unrestricted, expected accept, tag 4
 * - Bad runtime task:  T=3000 ms, C=2200 ms, D=T, unrestricted, expected reject
 *
 * Partitioned EDF task set:
 * - Base 1: T=4000 ms, C=2000 ms, D=T, no explicit affinity, tag 1
 * - Base 2: T=5000 ms, C=2500 ms, D=T, no explicit affinity, tag 2
 * - Good runtime task: T=4000 ms, C=1000 ms, D=T, no explicit affinity, expected accept, tag 4
 * - Bad runtime task:  T=4000 ms, C=2200 ms, D=T, no explicit affinity, expected reject
 *
 * Desired observations:
 * - After the scheduler has already started, the good runtime task should be admitted
 *   and begin appearing on the active core bank(s).
 * - Later, the bad runtime task should be rejected and should never appear on the GPIO banks.
 * - Volatile result flags record both create-time outcomes.
 */

#define MP_RUNTIME_STACK_DEPTH            256u
#define MP_RUNTIME_CTRL_PERIOD_MS         30000u
#define MP_RUNTIME_CTRL_WCET_MS           10u

#if ( PARTITIONED_EDF_ENABLE == 1U )
    #define BASE1_PERIOD_MS              4000u
    #define BASE1_WCET_MS                2000u
    #define BASE2_PERIOD_MS              5000u
    #define BASE2_WCET_MS                2500u
    #define GOOD_PERIOD_MS               4000u
    #define GOOD_WCET_MS                 1000u
    #define BAD_PERIOD_MS                4000u
    #define BAD_WCET_MS                  2200u
#else
    #define BASE1_PERIOD_MS              6000u
    #define BASE1_WCET_MS                1800u
    #define BASE2_PERIOD_MS              7000u
    #define BASE2_WCET_MS                2100u
    #define GOOD_PERIOD_MS               2000u
    #define GOOD_WCET_MS                 400u
    #define BAD_PERIOD_MS                3000u
    #define BAD_WCET_MS                  2200u
#endif

typedef struct MPRuntimeTaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulWcetMs;
    UBaseType_t uxCoreAffinityMask;
    uint32_t ulAnchorSource;
} MPRuntimeTaskConfig_t;

#define MP_RUNTIME_ANCHOR_SOURCE_STARTUP    0u
#define MP_RUNTIME_ANCHOR_SOURCE_BAD        1u
#define MP_RUNTIME_ANCHOR_SOURCE_GOOD       2u

static volatile BaseType_t xGoodRuntimeCreateResult = pdFAIL;
static volatile BaseType_t xBadRuntimeCreateResult = pdFAIL;
static TickType_t xMpRuntimeSharedAnchorTick = 0u;
static TickType_t xMpRuntimeBadReleaseAnchorTick = 0u;
static TickType_t xMpRuntimeGoodReleaseAnchorTick = 0u;

static void vMPRuntimePeriodicTask( void * pvParameters )
{
    const MPRuntimeTaskConfig_t * pxCfg = ( const MPRuntimeTaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    configASSERT( pxCfg != NULL );

    if( pxCfg->ulAnchorSource == MP_RUNTIME_ANCHOR_SOURCE_BAD )
    {
        xLastWakeTime = xMpRuntimeBadReleaseAnchorTick;
    }
    else if( pxCfg->ulAnchorSource == MP_RUNTIME_ANCHOR_SOURCE_GOOD )
    {
        xLastWakeTime = xMpRuntimeGoodReleaseAnchorTick;
    }
    else
    {
        xLastWakeTime = xMpRuntimeSharedAnchorTick;
    }

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

static void vMPRuntimeControllerTask( void * pvParameters )
{
    ( void ) pvParameters;

    vTaskDelay( pdMS_TO_TICKS( 6000u ) );

    {
        static const MPRuntimeTaskConfig_t xGoodTaskCfg =
        {
            .pcName = "MP Run Good",
            .ulTag = 4u,
            .ulPeriodMs = GOOD_PERIOD_MS,
            .ulWcetMs = GOOD_WCET_MS,
            .uxCoreAffinityMask = tskNO_AFFINITY,
            .ulAnchorSource = MP_RUNTIME_ANCHOR_SOURCE_GOOD
        };
        TaskHandle_t xGoodHandle = NULL;
        xMpRuntimeGoodReleaseAnchorTick = xTaskGetTickCount();

        xGoodRuntimeCreateResult =
            xTaskCreate( vMPRuntimePeriodicTask,
                         xGoodTaskCfg.pcName,
                         MP_RUNTIME_STACK_DEPTH,
                         ( void * ) &xGoodTaskCfg,
                         &xGoodHandle,
                         xGoodTaskCfg.ulPeriodMs,
                         xGoodTaskCfg.ulWcetMs,
                         xGoodTaskCfg.ulPeriodMs,
                         xGoodTaskCfg.uxCoreAffinityMask );

        if( ( xGoodRuntimeCreateResult == pdPASS ) && ( xGoodHandle != NULL ) )
        {
            vTaskSetApplicationTaskTag( xGoodHandle,
                                        ( TaskHookFunction_t ) ( uintptr_t ) xGoodTaskCfg.ulTag );
        }
    }

    vTaskDelay( pdMS_TO_TICKS( 6000u ) );

    {
        static const MPRuntimeTaskConfig_t xBadTaskCfg =
        {
            .pcName = "MP Run Bad",
            .ulTag = 6u,
            .ulPeriodMs = BAD_PERIOD_MS,
            .ulWcetMs = BAD_WCET_MS,
            .uxCoreAffinityMask = tskNO_AFFINITY,
            .ulAnchorSource = MP_RUNTIME_ANCHOR_SOURCE_BAD
        };
        TaskHandle_t xBadHandle = NULL;
        xMpRuntimeBadReleaseAnchorTick = xTaskGetTickCount();
        xBadRuntimeCreateResult =
            xTaskCreate( vMPRuntimePeriodicTask,
                         xBadTaskCfg.pcName,
                         MP_RUNTIME_STACK_DEPTH,
                         ( void * ) &xBadTaskCfg,
                         &xBadHandle,
                         xBadTaskCfg.ulPeriodMs,
                         xBadTaskCfg.ulWcetMs,
                         xBadTaskCfg.ulPeriodMs,
                         xBadTaskCfg.uxCoreAffinityMask );
    }

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 1000u ) );
    }
}

void mp_edf_runtime_create_1_run( void )
{
    static const MPRuntimeTaskConfig_t xBase1Cfg =
    {
        .pcName = "MP Run B1",
        .ulTag = 1u,
        .ulPeriodMs = BASE1_PERIOD_MS,
        .ulWcetMs = BASE1_WCET_MS,
        .uxCoreAffinityMask = tskNO_AFFINITY,
        .ulAnchorSource = MP_RUNTIME_ANCHOR_SOURCE_STARTUP
    };
    static const MPRuntimeTaskConfig_t xBase2Cfg =
    {
        .pcName = "MP Run B2",
        .ulTag = 2u,
        .ulPeriodMs = BASE2_PERIOD_MS,
        .ulWcetMs = BASE2_WCET_MS,
        .uxCoreAffinityMask = tskNO_AFFINITY,
        .ulAnchorSource = MP_RUNTIME_ANCHOR_SOURCE_STARTUP
    };
    TaskHandle_t xBase1Handle = NULL;
    TaskHandle_t xBase2Handle = NULL;
    TaskHandle_t xControllerHandle = NULL;

    stdio_init_all();
    vTraceTaskPinsInit();
    xMpRuntimeSharedAnchorTick = xTaskGetTickCount();

    ( void ) xTaskCreate( vMPRuntimePeriodicTask,
                          xBase1Cfg.pcName,
                          MP_RUNTIME_STACK_DEPTH,
                          ( void * ) &xBase1Cfg,
                          &xBase1Handle,
                          xBase1Cfg.ulPeriodMs,
                          xBase1Cfg.ulWcetMs,
                          xBase1Cfg.ulPeriodMs,
                          xBase1Cfg.uxCoreAffinityMask );

    ( void ) xTaskCreate( vMPRuntimePeriodicTask,
                          xBase2Cfg.pcName,
                          MP_RUNTIME_STACK_DEPTH,
                          ( void * ) &xBase2Cfg,
                          &xBase2Handle,
                          xBase2Cfg.ulPeriodMs,
                          xBase2Cfg.ulWcetMs,
                          xBase2Cfg.ulPeriodMs,
                          xBase2Cfg.uxCoreAffinityMask );

    ( void ) xTaskCreate( vMPRuntimeControllerTask,
                          "MP Run C",
                          MP_RUNTIME_STACK_DEPTH,
                          NULL,
                          &xControllerHandle,
                          MP_RUNTIME_CTRL_PERIOD_MS,
                          MP_RUNTIME_CTRL_WCET_MS,
                          MP_RUNTIME_CTRL_PERIOD_MS,
                          tskNO_AFFINITY );

    if( xBase1Handle != NULL )
    {
        vTaskSetApplicationTaskTag( xBase1Handle,
                                    ( TaskHookFunction_t ) ( uintptr_t ) xBase1Cfg.ulTag );
    }

    if( xBase2Handle != NULL )
    {
        vTaskSetApplicationTaskTag( xBase2Handle,
                                    ( TaskHookFunction_t ) ( uintptr_t ) xBase2Cfg.ulTag );
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

#endif /* ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) */
