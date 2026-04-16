#ifndef TASK_TRACE_H
#define TASK_TRACE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRACE_TASK_PIN0    2u
#define TRACE_TASK_PIN1    3u
#define TRACE_TASK_PIN2    4u
#define TRACE_TASK_PIN3    5u
#define TRACE_TASK_PIN4    6u
#define TRACE_TASK_PIN5    7u
#define TRACE_TASK_PIN6    8u
#define TRACE_TASK_SWITCH_PIN    9u
#define TRACE_DEADLINE_MISS_PIN    15u

void vTraceTaskPinsInit( void );
void vTraceWriteTaskCode( uint32_t ulTaskCode );
void vTraceTaskSwitchedIn( uint32_t ulTaskCode );
void vTraceSetTaskSwitchSignal( void );
void vTraceClearTaskSwitchSignal( void );
void vTraceSignalDeadlineMiss( void );
void vTraceClearDeadlineMissSignal( void );
void vTraceDeadlineMissTick( void );
void vApplicationDeadlineMissHook( void );
struct tskTaskControlBlock;
void vApplicationStackOverflowHook( struct tskTaskControlBlock * xTask, char * pcTaskName );

#define traceTASK_SWITCHED_IN()    vTraceTaskSwitchedIn( ( uint32_t ) ( uintptr_t ) pxCurrentTCB->pxTaskTag )

#define traceTASK_SWITCHED_OUT()    vTraceClearTaskSwitchSignal()
#define traceTASK_DEADLINE_MISSED()            \
    do                                         \
    {                                          \
        vTraceSignalDeadlineMiss();            \
        vApplicationDeadlineMissHook();        \
    } while( 0 )
#define traceTASK_INCREMENT_TICK( xTickCount )    vTraceDeadlineMissTick()

#ifdef __cplusplus
}
#endif

#endif /* TASK_TRACE_H */
