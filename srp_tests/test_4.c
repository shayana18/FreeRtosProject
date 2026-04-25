#include "srp_tests/test_4.h"

#if ( ( configUSE_UP == 1 ) && ( configUSE_EDF == 1 ) && ( configUSE_SRP == 1 ) && ( configUSE_SRP_SHARED_STACKS == 1 ) )

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

typedef struct SRP4TaskConfig
{
    const char * pcName;
    uint32_t ulTraceTag;
    uint32_t ulRelativeDeadlineMs;
    configSTACK_DEPTH_TYPE uxStackDepthWords;
} SRP4TaskConfig_t;

static TickType_t xSRP4AnchorTick = 0u;

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

void srp_4_run( void )
{
    static const SRP4TaskConfig_t xTaskCfgs[] =
    {
        { "SRP4 A0", 1u, 20000u, SRP4_STACK_A0 },
        { "SRP4 A1", 2u, 20000u, SRP4_STACK_A1 },
        { "SRP4 A2", 3u, 20000u, SRP4_STACK_A2 },
        { "SRP4 A3", 4u, 20000u, SRP4_STACK_A3 },
        { "SRP4 B0", 5u, 10000u, SRP4_STACK_B0 },
        { "SRP4 B1", 6u, 10000u, SRP4_STACK_B1 },
        { "SRP4 B2", 7u, 10000u, SRP4_STACK_B2 },
        { "SRP4 B3", 8u, 10000u, SRP4_STACK_B3 },
        { "SRP4 C0", 9u,  5000u, SRP4_STACK_C0 },
        { "SRP4 C1", 10u,  5000u, SRP4_STACK_C1 },
        { "SRP4 C2", 11u,  5000u, SRP4_STACK_C2 },
        { "SRP4 C3", 12u,  5000u, SRP4_STACK_C3 }
    };
    UBaseType_t uxIndex;

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
