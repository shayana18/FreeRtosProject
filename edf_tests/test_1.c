#include "edf_tests/test_1.h"

#if ( (configUSE_UP == 1) && ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )

#include <stdint.h>
#include <stdio.h>

#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "task_trace.h"

#include "test_utils.h"

#define TASK1_PERIOD_MS  5000u
#define TASK2_PERIOD_MS  7000u
#define TASK3_PERIOD_MS  8000u

#define TASK1_WORK_MS    1500u
#define TASK2_WORK_MS    1500u
#define TASK3_WORK_MS    2000u

static void Task1(void *pvParameters)
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    (void) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        spin_ms(TASK1_WORK_MS);
        xDelayResult = xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK1_PERIOD_MS));

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void Task2(void *pvParameters)
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    (void) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        spin_ms(TASK2_WORK_MS);
        xDelayResult = xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK2_PERIOD_MS));

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

static void Task3(void *pvParameters)
{
    TickType_t xLastWakeTime;
    BaseType_t xDelayResult;

    (void) pvParameters;
    xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        spin_ms(TASK3_WORK_MS);
        xDelayResult = xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TASK3_PERIOD_MS));

        if( xDelayResult == pdFALSE )
        {
            xLastWakeTime = xTaskGetTickCount();
        }
    }
}

void edf_1_run(void)
{
    stdio_init_all();
    vTraceTaskPinsInit();

    TaskHandle_t xTask1Handle = NULL;
    TaskHandle_t xTask2Handle = NULL;
    TaskHandle_t xTask3Handle = NULL;

    xTaskCreate(Task1, "Test Task 1", 256, NULL, &xTask1Handle, TASK1_PERIOD_MS, TASK1_WORK_MS, TASK1_PERIOD_MS);
    xTaskCreate(Task2, "Test Task 2", 256, NULL, &xTask2Handle, TASK2_PERIOD_MS, TASK2_WORK_MS, TASK2_PERIOD_MS);
    xTaskCreate(Task3, "Test Task 3", 256, NULL, &xTask3Handle, TASK3_PERIOD_MS, TASK3_WORK_MS, TASK3_PERIOD_MS);

    vTaskSetApplicationTaskTag(xTask1Handle, (TaskHookFunction_t) 1);
    vTaskSetApplicationTaskTag(xTask2Handle, (TaskHookFunction_t) 2);
    vTaskSetApplicationTaskTag(xTask3Handle, (TaskHookFunction_t) 4);

    vTaskStartScheduler();

    for (;;)
    {
    }
}

#endif /* configUSE_EDF */
