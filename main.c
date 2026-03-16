#include <stdio.h>

#include "pico/stdlib.h"
#include "schedulingConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "task_trace.h"

#define TASK1_WORK_MS    1500u
#define TASK2_WORK_MS    1500u
#define TASK3_WORK_MS    2000u

static void spin_ms(uint32_t target_ms)
{
    uint32_t executed_us = 0;
    uint32_t prev_us = time_us_32();

    while (executed_us < target_ms * 1000u)
    {
        uint32_t now_us = time_us_32();
        uint32_t delta_us = now_us - prev_us;

        if (delta_us < 100u)
        {
            executed_us += delta_us;
        }

        prev_us = now_us;
    }
}


static void Task1( void * pvParameters )
{
    ( void ) pvParameters;

    vTaskSetApplicationTaskTag( NULL, ( TaskHookFunction_t ) 1 );

    for( ;; )
    {
        spin_ms( TASK1_WORK_MS );
        volatile int x = 1U; 
    }
}

static void Task2( void * pvParameters )
{
    ( void ) pvParameters;

    vTaskSetApplicationTaskTag( NULL, ( TaskHookFunction_t ) 2 );

    for( ;; )
    {
        spin_ms(TASK2_WORK_MS);
    }
}

static void Task3( void * pvParameters )
{
    ( void ) pvParameters;

    vTaskSetApplicationTaskTag( NULL, ( TaskHookFunction_t ) 4 );

    for( ;; )
    {
        spin_ms(TASK3_WORK_MS);
    }
}

int main( void )
{
    stdio_init_all();
    vTraceTaskPinsInit();

    TaskHandle_t xTask1Handle = NULL;
    TaskHandle_t xTask2Handle = NULL;
    TaskHandle_t xTask3Handle = NULL;

    xTaskCreate( Task1, "Test Task 1", 256, NULL, &xTask1Handle, 5000, 1500, 5000 );
    xTaskCreate( Task2, "Test Task 2", 256, NULL, &xTask2Handle, 7000, 1500, 7000 );
    xTaskCreate( Task3, "Test Task 3", 256, NULL, &xTask3Handle, 8000, 2000, 8000 );

    vTaskSetApplicationTaskTag( xTask1Handle, ( TaskHookFunction_t ) 1 );
    vTaskSetApplicationTaskTag( xTask2Handle, ( TaskHookFunction_t ) 2 );
    vTaskSetApplicationTaskTag( xTask3Handle, ( TaskHookFunction_t ) 4 );

    vTaskStartScheduler();

    for( ;; )
    {
    }
}
