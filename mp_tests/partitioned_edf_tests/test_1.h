#pragma once

#include "schedulingConfig.h"

#if ( ( configUSE_MP == 1 ) && ( configUSE_UP == 0 ) && ( configUSE_EDF == 1 ) && ( PARTITIONED_EDF_ENABLE == 1U ) )
/**
 * Run partitioned EDF MP test case 1 (basic dispatch).
 *
 * Intended usage: call once from main(). This function does not return.
 */
void mp_partitioned_edf_1_run( void );
#else
static inline void mp_partitioned_edf_1_run( void ) {}
#endif
