#pragma once

#include "schedulingConfig.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( GLOBAL_EDF_ENABLE == 1U ) )
/**
 * Run MP demo test 1 for global EDF.
 *
 * Intended usage: call once from main(). This function does not return.
 */
void mp_demo_test_1_glob_run( void );
#else
static inline void mp_demo_test_1_glob_run( void ) {}
#endif
