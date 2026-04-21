#pragma once

#include "schedulingConfig.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) )
/**
 * Run MP EDF run-time task-creation test 1.
 *
 * Intended usage: call once from main(). This function does not return.
 */
void mp_edf_runtime_create_1_run( void );
#else
static inline void mp_edf_runtime_create_1_run( void ) {}
#endif
