#include "task_trace.h"

#include <stdint.h>

#include "hardware/gpio.h"

void vTraceTaskPinsInit( void )
{
    gpio_init( TRACE_TASK_PIN0 );
    gpio_init( TRACE_TASK_PIN1 );
    gpio_init( TRACE_TASK_PIN2 );

    gpio_set_dir( TRACE_TASK_PIN0, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_PIN1, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_PIN2, GPIO_OUT );

    gpio_put( TRACE_TASK_PIN0, 0 );
    gpio_put( TRACE_TASK_PIN1, 0 );
    gpio_put( TRACE_TASK_PIN2, 0 );
}

void vTraceWriteTaskCode( uint32_t ulTaskCode )
{
    gpio_put( TRACE_TASK_PIN0, ( ulTaskCode & 0x1u ) != 0u );
    gpio_put( TRACE_TASK_PIN1, ( ulTaskCode & 0x2u ) != 0u );
    gpio_put( TRACE_TASK_PIN2, ( ulTaskCode & 0x4u ) != 0u );
}
