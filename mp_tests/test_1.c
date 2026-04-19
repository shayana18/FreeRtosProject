#include "mp_tests/test_1.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * MP EDF admission-control test.
 *
 * Global EDF task set:
 * - Base 1: T=4000 ms, C=1600 ms, D=T, unrestricted, tag 1
 * - Base 2: T=5000 ms, C=2000 ms, D=T, unrestricted, tag 2
 * - Bad   : T=3000 ms, C=1900 ms, D=T, unrestricted, expected reject
 * - Good  : T=8000 ms, C=800 ms,  D=T, unrestricted, expected accept, tag 4
 *
 * Partitioned EDF task set:
 * - Base 1: T=4000 ms, C=2400 ms, D=T, no explicit affinity, tag 1
 * - Base 2: T=5000 ms, C=3000 ms, D=T, no explicit affinity, tag 2
 * - Bad   : T=4000 ms, C=1400 ms, D=T, no explicit affinity, expected reject
 * - Good  : T=5000 ms, C=1000 ms, D=T, no explicit affinity, expected accept, tag 4
 *
 * Desired observations:
 * - The initial bad-task create should fail and set the corresponding volatile flag.
 * - After ~10 s, the repeated bad-task create should fail again.
 * - The later good-task create should succeed and the task should begin running.
 * - GPIO is used only to observe accepted tasks; admission results are tracked
 *   through volatile pass/fail variables in the same style as edf_tests/test_3.c.
 */

#define MP_ADMISSION_STACK_DEPTH           256u
#define MP_ADMISSION_CTRL_PERIOD_MS        30000u
#define MP_ADMISSION_CTRL_WCET_MS          10u

#if ( PARTITIONED_EDF_ENABLE == 1U )
    #define BASE1_PERIOD_MS                4000u
    #define BASE1_WCET_MS                  2400u
    #define BASE2_PERIOD_MS                5000u
    #define BASE2_WCET_MS                  3000u
    #define BAD_PERIOD_MS                  4000u
    #define BAD_WCET_MS                    1400u
    #define GOOD_PERIOD_MS                 5000u
    #define GOOD_WCET_MS                   1000u
#else
    #define BASE1_PERIOD_MS                4000u
    #define BASE1_WCET_MS                  1600u
    #define BASE2_PERIOD_MS                5000u
    #define BASE2_WCET_MS                  2000u
    #define BAD_PERIOD_MS                  3000u
    #define BAD_WCET_MS                    1900u
    #define GOOD_PERIOD_MS                 8000u
    #define GOOD_WCET_MS                   800u
#endif

typedef struct MPAdmissionTaskConfig
{
    const char * pcName;
    uint32_t ulTag;
    uint32_t ulPeriodMs;
    uint32_t ulWcetMs;
    UBaseType_t uxCoreAffinityMask;
} MPAdmissionTaskConfig_t;

static volatile BaseType_t xBadCreateResultInitial = pdFAIL;
static volatile BaseType_t xBadCreateResultAfter10s = pdFAIL;
static volatile BaseType_t xGoodCreateResultAfter10s = pdFAIL;

static void vMPAdmissionPeriodicTask( void * pvParameters )
{
    const MPAdmissionTaskConfig_t * pxCfg = ( const MPAdmissionTaskConfig_t * ) pvParameters;
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

static void vMPAdmissionControllerTask( void * pvParameters )
{
    ( void ) pvParameters;

    vTaskDelay( pdMS_TO_TICKS( 10000u ) );

    {
        static const MPAdmissionTaskConfig_t xBadTaskCfg =
        {
            .pcName = "MP Bad",
            .ulTag = 0u,
            .ulPeriodMs = BAD_PERIOD_MS,
            .ulWcetMs = BAD_WCET_MS,
            .uxCoreAffinityMask = tskNO_AFFINITY
        };
        TaskHandle_t xBadHandle = NULL;
        xBadCreateResultAfter10s =
            xTaskCreate( vMPAdmissionPeriodicTask,
                         xBadTaskCfg.pcName,
                         MP_ADMISSION_STACK_DEPTH,
                         ( void * ) &xBadTaskCfg,
                         &xBadHandle,
                         xBadTaskCfg.ulPeriodMs,
                         xBadTaskCfg.ulWcetMs,
                         xBadTaskCfg.ulPeriodMs,
                         xBadTaskCfg.uxCoreAffinityMask );
    }

    {
        static const MPAdmissionTaskConfig_t xGoodTaskCfg =
        {
            .pcName = "MP Good",
            .ulTag = 4u,
            .ulPeriodMs = GOOD_PERIOD_MS,
            .ulWcetMs = GOOD_WCET_MS,
            .uxCoreAffinityMask = tskNO_AFFINITY
        };
        TaskHandle_t xGoodHandle = NULL;

        xGoodCreateResultAfter10s =
            xTaskCreate( vMPAdmissionPeriodicTask,
                         xGoodTaskCfg.pcName,
                         MP_ADMISSION_STACK_DEPTH,
                         ( void * ) &xGoodTaskCfg,
                         &xGoodHandle,
                         xGoodTaskCfg.ulPeriodMs,
                         xGoodTaskCfg.ulWcetMs,
                         xGoodTaskCfg.ulPeriodMs,
                         xGoodTaskCfg.uxCoreAffinityMask );

        if( ( xGoodCreateResultAfter10s == pdPASS ) && ( xGoodHandle != NULL ) )
        {
            vTaskSetApplicationTaskTag( xGoodHandle,
                                        ( TaskHookFunction_t ) ( uintptr_t ) xGoodTaskCfg.ulTag );
        }
    }

    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 1000u ) );
    }
}

void mp_edf_admission_1_run( void )
{
    static const MPAdmissionTaskConfig_t xBase1Cfg =
    {
        .pcName = "MP Base 1",
        .ulTag = 1u,
        .ulPeriodMs = BASE1_PERIOD_MS,
        .ulWcetMs = BASE1_WCET_MS,
        .uxCoreAffinityMask = tskNO_AFFINITY
    };
    static const MPAdmissionTaskConfig_t xBase2Cfg =
    {
        .pcName = "MP Base 2",
        .ulTag = 2u,
        .ulPeriodMs = BASE2_PERIOD_MS,
        .ulWcetMs = BASE2_WCET_MS,
        .uxCoreAffinityMask = tskNO_AFFINITY
    };
    TaskHandle_t xBase1Handle = NULL;
    TaskHandle_t xBase2Handle = NULL;

    stdio_init_all();
    vTraceTaskPinsInit();

    ( void ) xTaskCreate( vMPAdmissionPeriodicTask,
                          xBase1Cfg.pcName,
                          MP_ADMISSION_STACK_DEPTH,
                          ( void * ) &xBase1Cfg,
                          &xBase1Handle,
                          xBase1Cfg.ulPeriodMs,
                          xBase1Cfg.ulWcetMs,
                          xBase1Cfg.ulPeriodMs,
                          xBase1Cfg.uxCoreAffinityMask );

    ( void ) xTaskCreate( vMPAdmissionPeriodicTask,
                          xBase2Cfg.pcName,
                          MP_ADMISSION_STACK_DEPTH,
                          ( void * ) &xBase2Cfg,
                          &xBase2Handle,
                          xBase2Cfg.ulPeriodMs,
                          xBase2Cfg.ulWcetMs,
                          xBase2Cfg.ulPeriodMs,
                          xBase2Cfg.uxCoreAffinityMask );

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

    {
        static const MPAdmissionTaskConfig_t xBadTaskCfg =
        {
            .pcName = "MP Bad",
            .ulTag = 0u,
            .ulPeriodMs = BAD_PERIOD_MS,
            .ulWcetMs = BAD_WCET_MS,
            .uxCoreAffinityMask = tskNO_AFFINITY
        };
        TaskHandle_t xBadHandle = NULL;

        xBadCreateResultInitial =
            xTaskCreate( vMPAdmissionPeriodicTask,
                         xBadTaskCfg.pcName,
                         MP_ADMISSION_STACK_DEPTH,
                         ( void * ) &xBadTaskCfg,
                         &xBadHandle,
                         xBadTaskCfg.ulPeriodMs,
                         xBadTaskCfg.ulWcetMs,
                         xBadTaskCfg.ulPeriodMs,
                         xBadTaskCfg.uxCoreAffinityMask );
    }

    {
        TaskHandle_t xControllerHandle = NULL;

        ( void ) xTaskCreate( vMPAdmissionControllerTask,
                              "MP Admit C",
                              MP_ADMISSION_STACK_DEPTH,
                              NULL,
                              &xControllerHandle,
                              MP_ADMISSION_CTRL_PERIOD_MS,
                              MP_ADMISSION_CTRL_WCET_MS,
                              MP_ADMISSION_CTRL_PERIOD_MS,
                              tskNO_AFFINITY );

        if( xControllerHandle != NULL )
        {
            vTaskSetApplicationTaskTag( xControllerHandle,
                                        ( TaskHookFunction_t ) ( uintptr_t ) 7u );
        }
    }

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#endif /* ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) */
