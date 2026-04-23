#pragma once

#include "schedulingConfig.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( GLOBAL_EDF_ENABLE == 1U ) )
/**
 * Run MP demo test 2 showing a Dhall-style admission rejection under global EDF.
 *
 * Intended usage: call once from main(). This function does not return.
 */
void mp_test_dhall_run( void );
#else
static inline void mp_test_dhall_run( void ) {}
#endif
