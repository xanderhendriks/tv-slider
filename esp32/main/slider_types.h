#ifndef SLIDER_TYPES_H
#define SLIDER_TYPES_H

#define SLIDER_RAMP_INTERVAL_MS 60   // time between speed increments/decrements
#define SLIDER_SPEED_STEP       2    // speed increment/decrement step
#define SLIDER_MIN_SPEED        0    // minimum speed
#define SLIDER_MAX_SPEED        100  // maximum speed

typedef enum
{
    SLIDER_DIRECTION_IN  = 0,
    SLIDER_DIRECTION_OUT = 1
} slider_direction_t;

#endif  // SLIDER_TYPES_H