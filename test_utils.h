#pragma once

#include <stdint.h>

#include "schedulingConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

void spin_ms(uint32_t target_ms);

#ifdef __cplusplus
}
#endif
