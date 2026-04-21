#include "edf_tests/test_9.h"
#include "schedulingConfig.h"

#if ( (configUSE_UP == 1) && ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"
#include "test_utils.h"

#define TASK_B_WORK_MS   95u
#define TASK_PERIOD_MS   11000u
#define NUM_TASKS        100

static TickType_t xEdf9SharedAnchorTick = 0u;

static void GenericTask(void *pvParameters)
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    /* task id is passed through pvParameters */
    int task_id = (int)(intptr_t)pvParameters;

    xLastWakeTime = xEdf9SharedAnchorTick;

    for (;;)
    {
        spin_ms(TASK_B_WORK_MS);
        xDelayResult = xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK_PERIOD_MS));

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void edf_9_run(void)
{
    stdio_init_all();
    vTraceTaskPinsInit();
    xEdf9SharedAnchorTick = xTaskGetTickCount();

    for (int i = 1; i <= NUM_TASKS; ++i)
    {
        char name[20];
        snprintf(name, sizeof(name), "Task %d", i);

        TaskHandle_t xTaskHandle = NULL;
        TickType_t deadline_ms = i * 100u;
        
        /* According to FreeRTOS EDF implementation here, xTaskCreate takes:
         * TaskFunction_t, const char*, configSTACK_DEPTH_TYPE, void*, TaskHandle_t*, Period, ComputationTime, Deadline
         */
        if (xTaskCreate(
            GenericTask,
            name,
            256,
            (void *)(intptr_t)i,
            &xTaskHandle,
            TASK_PERIOD_MS,
            TASK_B_WORK_MS,
            deadline_ms) == pdPASS)

        {
            vTaskSetApplicationTaskTag(xTaskHandle, (TaskHookFunction_t)i);
        }
        else 
        {
            vTraceUsbPrint("Failed to create task %d\r\n", i);
        }

    }

    vTaskStartScheduler();

    for (;;)
    {
    }
}

#endif /* configUSE_EDF */
