#ifndef TASK_TRACE_H
#define TASK_TRACE_H

#include <stdint.h>
#include <stddef.h>

#include "schedulingConfig.h"
#include "FreeRTOSConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
    #define TRACE_CORE0_TASK_PIN0         2u
    #define TRACE_CORE0_TASK_PIN1         3u
    #define TRACE_CORE0_TASK_PIN2         4u
    #define TRACE_CORE0_TASK_SWITCH_PIN   5u
    #define TRACE_CORE1_TASK_PIN0         6u
    #define TRACE_CORE1_TASK_PIN1         7u
    #define TRACE_CORE1_TASK_PIN2         8u
    #define TRACE_CORE1_TASK_SWITCH_PIN   9u
#else
    #define TRACE_TASK_PIN0               2u
    #define TRACE_TASK_PIN1               3u
    #define TRACE_TASK_PIN2               4u
    #define TRACE_TASK_PIN3               5u
    #define TRACE_TASK_PIN4               6u
    #define TRACE_TASK_PIN5               7u
    #define TRACE_TASK_PIN6               8u
    #define TRACE_TASK_SWITCH_PIN         9u
#endif

#define TRACE_DEADLINE_MISS_PIN    15u

void vTraceTaskPinsInit( void );
void vTraceUsbSerialInit( uint32_t ulWaitForHostMs );
void vTraceUsbPrint( const char * pcFormat, ... );
void vTraceWriteTaskCode( uint32_t ulTaskCode );
void vTraceTaskSwitchedIn( uint32_t ulTaskCode );
void vTraceSetTaskSwitchSignal( void );
void vTraceClearTaskSwitchSignal( void );
void vTraceSignalDeadlineMiss( void );
void vTraceClearDeadlineMissSignal( void );
void vTraceDeadlineMissTick( void );
void vApplicationDeadlineMissHook( uint32_t ulTaskId );

#if ( configUSE_MP == 1 ) && ( configNUMBER_OF_CORES > 1 )
void vTraceRecordMPOverrunEvent( const char * pcPolicy,
                                 const char * pcReason,
                                 const char * pcTaskName,
                                 uint32_t ulTaskCode,
                                 uint32_t ulCoreID,
                                 uint32_t ulNowTick,
                                 uint32_t ulReleaseTick,
                                 uint32_t ulDeadlineTick,
                                 uint32_t ulExecTicks,
                                 uint32_t ulWcetTicks,
                                 uint32_t ulNextReleaseTick );
void vTraceFlushMPOverrunEvents( void );
#else
static inline void vTraceFlushMPOverrunEvents( void )
{
}
#endif

struct tskTaskControlBlock;
void vApplicationStackOverflowHook( struct tskTaskControlBlock * xTask, char * pcTaskName );

#define traceTASK_SWITCHED_IN()    vTraceTaskSwitchedIn( ( uint32_t ) ( uintptr_t ) pxCurrentTCB->pxTaskTag )

#define traceTASK_SWITCHED_OUT()    vTraceClearTaskSwitchSignal()
#define traceTASK_DEADLINE_MISSED()            \
    do                                         \
    {                                          \
        vTraceSignalDeadlineMiss();            \
        vApplicationDeadlineMissHook( ( uint32_t ) ( uintptr_t ) pxCurrentTCB->pxTaskTag ); \
    } while( 0 )
#define traceTASK_INCREMENT_TICK( xTickCount )    vTraceDeadlineMissTick()

#ifdef __cplusplus
}
#endif

#endif /* TASK_TRACE_H */
