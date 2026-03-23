#pragma once

#include "schedulingConfig.h"

#if ( configUSE_EDF == 1 )
/**
 * Run EDF test case 8 (7 one-hot tasks + intentional single-job misses):
 * - Creates seven periodic EDF tasks with one-hot task tags 1,2,4,8,16,32,64.
 * - Most tasks only execute bounded looped work inside their WCET.
 * - Two tasks intentionally trigger one deadline miss each by delaying past deadline once.
 *
 * Intended usage: call once from main(). This function does not return.
 */
void edf_8_run( void );
#else
static inline void edf_8_run( void ) {}
#endif
