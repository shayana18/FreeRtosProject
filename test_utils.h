#pragma once

#include <stdint.h>

#include "schedulingConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

void spin_ms(uint32_t target_ms);

#if ( ( configUSE_UP == 1U ) && ( configUSE_EDF == 1U ) && ( configUSE_SRP == 1U ) && ( configUSE_SRP_SHARED_STACKS == 1U ) && ( configENABLE_TEST_SRP_STACK_REPORT == 1U ) )
void vSRPReportStackUsageIfDue(void);
#else
static inline void vSRPReportStackUsageIfDue(void) {}
#endif

#ifdef __cplusplus
}
#endif
