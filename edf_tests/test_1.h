#pragma once

#include "schedulingConfig.h"

#if ( ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )
/**
 * Run EDF test case 1.
 *
 * Intended usage: call once from main(). This function does not return.
 */
void edf_1_run(void);
#else
/* EDF disabled: keep a callable stub so main.c can compile unchanged. */
static inline void edf_1_run(void) {}
#endif
