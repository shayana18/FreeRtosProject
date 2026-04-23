#include "srp_tests/test_4.h"

#if ( ( configUSE_UP == 1 ) && ( configUSE_EDF == 1 ) && ( configUSE_SRP == 1 ) && ( configUSE_SRP_SHARED_STACKS == 1 ) )

#include <stddef.h>
#include <stdint.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

/*
 * SRP shared-stack accounting study.
 *
 * The test creates several groups of tasks with the same relative deadline.
 * Tasks in the same deadline group have the same SRP preemption level, so the
 * shared-stack backend should reserve only the largest stack from each group.
 */

#define SRP4_TASK_WCET_MS           250u
#define SRP4_TASK_WORK_MS           100u
#define SRP4_PERIOD_MS            20000u

#define SRP4_STACK_A0               320u
#define SRP4_STACK_A1               280u
#define SRP4_STACK_A2               260u
#define SRP4_STACK_A3               240u
#define SRP4_STACK_B0               360u
#define SRP4_STACK_B1               320u
#define SRP4_STACK_B2               260u
#define SRP4_STACK_B3               220u
#define SRP4_STACK_C0               320u
#define SRP4_STACK_C1               280u
#define SRP4_STACK_C2               240u
#define SRP4_STACK_C3               200u
#define SRP4_STACK_REPORTER         256u

#define SRP4_EXPECTED_SHARED_WORDS      ( SRP4_STACK_A0 + SRP4_STACK_B0 + SRP4_STACK_C0 + SRP4_STACK_REPORTER )
#define SRP4_EXPECTED_NONSHARED_WORDS   ( SRP4_STACK_A0 + SRP4_STACK_A1 + SRP4_STACK_A2 + SRP4_STACK_A3 + \
                                          SRP4_STACK_B0 + SRP4_STACK_B1 + SRP4_STACK_B2 + SRP4_STACK_B3 + \
                                          SRP4_STACK_C0 + SRP4_STACK_C1 + SRP4_STACK_C2 + SRP4_STACK_C3 + \
                                          SRP4_STACK_REPORTER )

#define SRP4_TRACE_REPORTER         7u

typedef struct SRP4TaskConfig
{
    const char * pcName;
    uint32_t ulTraceTag;
    uint32_t ulRelativeDeadlineMs;
    configSTACK_DEPTH_TYPE uxStackDepthWords;
} SRP4TaskConfig_t;

static TickType_t xSRP4AnchorTick = 0u;
static volatile BaseType_t xSRP4TheoreticalMatches = pdFALSE;

static void vSRP4WorkerTask( void * pvParameters )
{
    const SRP4TaskConfig_t * pxCfg = ( const SRP4TaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime = xSRP4AnchorTick;

    configASSERT( pxCfg != NULL );

    for( ;; )
    {
        spin_ms( SRP4_TASK_WORK_MS );
        vSRPReportStackUsageIfDue();

        if( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( SRP4_PERIOD_MS ) ) == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void vSRP4ReporterTask( void * pvParameters )
{
    TickType_t xLastWakeTime = xSRP4AnchorTick;

    ( void ) pvParameters;

    for( ;; )
    {
        size_t uxCurrentSharedBytes = 0u;
        size_t uxCurrentNonSharedBytes = 0u;
        size_t uxMaxSharedBytes = 0u;
        size_t uxMaxNonSharedBytes = 0u;
        size_t uxTheoreticalSharedBytes = 0u;
        size_t uxTheoreticalNonSharedBytes = 0u;
        const size_t uxExpectedSharedBytes = ( size_t ) SRP4_EXPECTED_SHARED_WORDS * sizeof( StackType_t );
        const size_t uxExpectedNonSharedBytes = ( size_t ) SRP4_EXPECTED_NONSHARED_WORDS * sizeof( StackType_t );

        vTaskDelay( pdMS_TO_TICKS( 1000u ) );

        vTaskGetSRPStackUsageRuntimeStats( &uxCurrentSharedBytes,
                                           &uxCurrentNonSharedBytes,
                                           &uxMaxSharedBytes,
                                           &uxMaxNonSharedBytes,
                                           &uxTheoreticalSharedBytes,
                                           &uxTheoreticalNonSharedBytes );

        xSRP4TheoreticalMatches =
            ( ( uxTheoreticalSharedBytes == uxExpectedSharedBytes ) &&
              ( uxTheoreticalNonSharedBytes == uxExpectedNonSharedBytes ) ) ? pdTRUE : pdFALSE;

        vTraceUsbPrint( "[SRP4] stack bytes: theoretical shared=%lu expected=%lu nonshared=%lu expected=%lu saved=%ld\r\n",
                        ( unsigned long ) uxTheoreticalSharedBytes,
                        ( unsigned long ) uxExpectedSharedBytes,
                        ( unsigned long ) uxTheoreticalNonSharedBytes,
                        ( unsigned long ) uxExpectedNonSharedBytes,
                        ( long ) ( uxTheoreticalNonSharedBytes - uxTheoreticalSharedBytes ) );

        configASSERT( xSRP4TheoreticalMatches == pdTRUE );

        if( xTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( SRP4_PERIOD_MS ) ) == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void srp_4_run( void )
{
    static const SRP4TaskConfig_t xTaskCfgs[] =
    {
        { "SRP4 A0", 1u, 20000u, SRP4_STACK_A0 },
        { "SRP4 A1", 2u, 20000u, SRP4_STACK_A1 },
        { "SRP4 A2", 3u, 20000u, SRP4_STACK_A2 },
        { "SRP4 A3", 4u, 20000u, SRP4_STACK_A3 },
        { "SRP4 B0", 1u, 10000u, SRP4_STACK_B0 },
        { "SRP4 B1", 2u, 10000u, SRP4_STACK_B1 },
        { "SRP4 B2", 3u, 10000u, SRP4_STACK_B2 },
        { "SRP4 B3", 4u, 10000u, SRP4_STACK_B3 },
        { "SRP4 C0", 1u,  5000u, SRP4_STACK_C0 },
        { "SRP4 C1", 2u,  5000u, SRP4_STACK_C1 },
        { "SRP4 C2", 3u,  5000u, SRP4_STACK_C2 },
        { "SRP4 C3", 4u,  5000u, SRP4_STACK_C3 }
    };
    UBaseType_t uxIndex;
    TaskHandle_t xReporterHandle = NULL;

    stdio_init_all();
    vTraceTaskPinsInit();

    xSRP4AnchorTick = xTaskGetTickCount();

    for( uxIndex = 0u; uxIndex < ( UBaseType_t ) ( sizeof( xTaskCfgs ) / sizeof( xTaskCfgs[ 0 ] ) ); uxIndex++ )
    {
        TaskHandle_t xHandle = NULL;

        configASSERT( xTaskCreate( vSRP4WorkerTask,
                                   xTaskCfgs[ uxIndex ].pcName,
                                   xTaskCfgs[ uxIndex ].uxStackDepthWords,
                                   ( void * ) &xTaskCfgs[ uxIndex ],
                                   &xHandle,
                                   SRP4_PERIOD_MS,
                                   SRP4_TASK_WCET_MS,
                                   xTaskCfgs[ uxIndex ].ulRelativeDeadlineMs,
                                   NULL,
                                   0u ) == pdPASS );

        vTaskSetApplicationTaskTag( xHandle,
                                    ( TaskHookFunction_t ) ( uintptr_t ) xTaskCfgs[ uxIndex ].ulTraceTag );
    }

    configASSERT( xTaskCreate( vSRP4ReporterTask,
                               "SRP4 REPORT",
                               SRP4_STACK_REPORTER,
                               NULL,
                               &xReporterHandle,
                               SRP4_PERIOD_MS,
                               SRP4_TASK_WCET_MS,
                               3000u,
                               NULL,
                               0u ) == pdPASS );
    vTaskSetApplicationTaskTag( xReporterHandle, ( TaskHookFunction_t ) SRP4_TRACE_REPORTER );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}

#else

void srp_4_run( void )
{
}

#endif
