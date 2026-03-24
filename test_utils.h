#pragma once

#include <stdint.h>

#include "schedulingConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#if ( ( configUSE_EDF == 1 ) && ( configUSE_SRP == 0 ) )
void spin_ms(uint32_t target_ms);
#else
static inline void spin_ms(uint32_t target_ms)
{
    (void) target_ms;
}
#endif

#ifdef __cplusplus
}
#endif
