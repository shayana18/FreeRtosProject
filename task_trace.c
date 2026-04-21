#include "FreeRTOS.h"
#include "task_trace.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define TRACE_DEADLINE_MISS_HOLD_TICKS    ( ( configTICK_RATE_HZ > 0u ) ? configTICK_RATE_HZ : 1u )

static volatile uint32_t ulDeadlineMissHoldTicks = 0u;

void vTraceUsbSerialInit( uint32_t ulWaitForHostMs )
{
    stdio_init_all();

    if( ulWaitForHostMs > 0u )
    {
        sleep_ms( ulWaitForHostMs );
    }
}

void vTraceUsbPrint( const char * pcFormat,
                     ... )
{
    va_list xArgs;

    va_start( xArgs, pcFormat );
    ( void ) vprintf( pcFormat, xArgs );
    va_end( xArgs );

    ( void ) fflush( stdout );
}

void vTraceTaskPinsInit( void )
{
#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
    gpio_init( TRACE_CORE0_TASK_PIN0 );
    gpio_init( TRACE_CORE0_TASK_PIN1 );
    gpio_init( TRACE_CORE0_TASK_PIN2 );
    gpio_init( TRACE_CORE0_TASK_SWITCH_PIN );
    gpio_init( TRACE_CORE1_TASK_PIN0 );
    gpio_init( TRACE_CORE1_TASK_PIN1 );
    gpio_init( TRACE_CORE1_TASK_PIN2 );
    gpio_init( TRACE_CORE1_TASK_SWITCH_PIN );
#else
    gpio_init( TRACE_TASK_PIN0 );
    gpio_init( TRACE_TASK_PIN1 );
    gpio_init( TRACE_TASK_PIN2 );
    gpio_init( TRACE_TASK_PIN3 );
    gpio_init( TRACE_TASK_PIN4 );
    gpio_init( TRACE_TASK_PIN5 );
    gpio_init( TRACE_TASK_PIN6 );
    gpio_init( TRACE_TASK_SWITCH_PIN );
#endif
    gpio_init( TRACE_DEADLINE_MISS_PIN );

#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
    gpio_set_dir( TRACE_CORE0_TASK_PIN0, GPIO_OUT );
    gpio_set_dir( TRACE_CORE0_TASK_PIN1, GPIO_OUT );
    gpio_set_dir( TRACE_CORE0_TASK_PIN2, GPIO_OUT );
    gpio_set_dir( TRACE_CORE0_TASK_SWITCH_PIN, GPIO_OUT );
    gpio_set_dir( TRACE_CORE1_TASK_PIN0, GPIO_OUT );
    gpio_set_dir( TRACE_CORE1_TASK_PIN1, GPIO_OUT );
    gpio_set_dir( TRACE_CORE1_TASK_PIN2, GPIO_OUT );
    gpio_set_dir( TRACE_CORE1_TASK_SWITCH_PIN, GPIO_OUT );
#else
    gpio_set_dir( TRACE_TASK_PIN0, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_PIN1, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_PIN2, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_PIN3, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_PIN4, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_PIN5, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_PIN6, GPIO_OUT );
    gpio_set_dir( TRACE_TASK_SWITCH_PIN, GPIO_OUT );
#endif
    gpio_set_dir( TRACE_DEADLINE_MISS_PIN, GPIO_OUT );

#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
    gpio_put( TRACE_CORE0_TASK_PIN0, 0 );
    gpio_put( TRACE_CORE0_TASK_PIN1, 0 );
    gpio_put( TRACE_CORE0_TASK_PIN2, 0 );
    gpio_put( TRACE_CORE0_TASK_SWITCH_PIN, 0 );
    gpio_put( TRACE_CORE1_TASK_PIN0, 0 );
    gpio_put( TRACE_CORE1_TASK_PIN1, 0 );
    gpio_put( TRACE_CORE1_TASK_PIN2, 0 );
    gpio_put( TRACE_CORE1_TASK_SWITCH_PIN, 0 );
#else
    gpio_put( TRACE_TASK_PIN0, 0 );
    gpio_put( TRACE_TASK_PIN1, 0 );
    gpio_put( TRACE_TASK_PIN2, 0 );
    gpio_put( TRACE_TASK_PIN3, 0 );
    gpio_put( TRACE_TASK_PIN4, 0 );
    gpio_put( TRACE_TASK_PIN5, 0 );
    gpio_put( TRACE_TASK_PIN6, 0 );
    gpio_put( TRACE_TASK_SWITCH_PIN, 0 );
#endif
    gpio_put( TRACE_DEADLINE_MISS_PIN, 0 );
}

void vTraceWriteTaskCode( uint32_t ulTaskCode )
{
#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
    const uint32_t ulCode = ulTaskCode & 0x7u;

    if( portGET_CORE_ID() == 0 )
    {
        gpio_put( TRACE_CORE0_TASK_PIN0, ( ulCode & 0x1u ) != 0u );
        gpio_put( TRACE_CORE0_TASK_PIN1, ( ulCode & 0x2u ) != 0u );
        gpio_put( TRACE_CORE0_TASK_PIN2, ( ulCode & 0x4u ) != 0u );
    }
    else
    {
        gpio_put( TRACE_CORE1_TASK_PIN0, ( ulCode & 0x1u ) != 0u );
        gpio_put( TRACE_CORE1_TASK_PIN1, ( ulCode & 0x2u ) != 0u );
        gpio_put( TRACE_CORE1_TASK_PIN2, ( ulCode & 0x4u ) != 0u );
    }
#else
    gpio_put( TRACE_TASK_PIN0, ( ulTaskCode & 0x1u ) != 0u );
    gpio_put( TRACE_TASK_PIN1, ( ulTaskCode & 0x2u ) != 0u );
    gpio_put( TRACE_TASK_PIN2, ( ulTaskCode & 0x4u ) != 0u );
    gpio_put( TRACE_TASK_PIN3, ( ulTaskCode & 0x8u ) != 0u );
    gpio_put( TRACE_TASK_PIN4, ( ulTaskCode & 0x10u ) != 0u );
    gpio_put( TRACE_TASK_PIN5, ( ulTaskCode & 0x20u ) != 0u );
    gpio_put( TRACE_TASK_PIN6, ( ulTaskCode & 0x40u ) != 0u );
#endif
}

void vTraceTaskSwitchedIn( uint32_t ulTaskCode )
{
    vTraceWriteTaskCode( ulTaskCode );
    vTraceSetTaskSwitchSignal();
}

void vTraceSetTaskSwitchSignal( void )
{
#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
    if( portGET_CORE_ID() == 0 )
    {
        gpio_put( TRACE_CORE0_TASK_SWITCH_PIN, 1 );
    }
    else
    {
        gpio_put( TRACE_CORE1_TASK_SWITCH_PIN, 1 );
    }
#else
    gpio_put( TRACE_TASK_SWITCH_PIN, 1 );
#endif
}

void vTraceClearTaskSwitchSignal( void )
{
#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
    if( portGET_CORE_ID() == 0 )
    {
        gpio_put( TRACE_CORE0_TASK_SWITCH_PIN, 0 );
    }
    else
    {
        gpio_put( TRACE_CORE1_TASK_SWITCH_PIN, 0 );
    }
#else
    gpio_put( TRACE_TASK_SWITCH_PIN, 0 );
#endif
}

void vTraceSignalDeadlineMiss( void )
{
    ulDeadlineMissHoldTicks = TRACE_DEADLINE_MISS_HOLD_TICKS;
    gpio_put( TRACE_DEADLINE_MISS_PIN, 1 );
}

void vTraceClearDeadlineMissSignal( void )
{
    ulDeadlineMissHoldTicks = 0u;
    gpio_put( TRACE_DEADLINE_MISS_PIN, 0 );
}

void vTraceDeadlineMissTick( void )
{
    if( ulDeadlineMissHoldTicks > 0u )
    {
        ulDeadlineMissHoldTicks--;

        if( ulDeadlineMissHoldTicks == 0u )
        {
            gpio_put( TRACE_DEADLINE_MISS_PIN, 0 );
        }
    }
}
