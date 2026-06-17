#pragma once

#include <stdint.h>

// Encoder count at fully-out (OUT_STOP) position, used to scale to 0-100%
#define POSITION_MAX_ENCODER_COUNT 300000

void    position_set(int32_t position);
int32_t position_get(void);
