#pragma once

#include "schedulingConfig.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( PARTITIONED_EDF_ENABLE == 0U ) )
/**
 * Run global EDF MP test case 2 (preemption).
 *
 * Intended usage: call once from main(). This function does not return.
 */
void mp_global_edf_2_run( void );
#else
static inline void mp_global_edf_2_run( void ) {}
#endif
