#ifndef SLIDER_TYPES_H
#define SLIDER_TYPES_H

#define SLIDER_RAMP_UP_INTERVAL_MS   60   // time between speed increments during ramp up
#define SLIDER_RAMP_DOWN_INTERVAL_MS 200  // time between speed decrements during ramp down
#define SLIDER_SPEED_STEP            2    // speed increment/decrement step
#define SLIDER_RAMP_UP_MIN_SPEED     0    // minimum speed when starting ramp up
#define SLIDER_RAMP_DOWN_MIN_SPEED   10   // minimum speed when ramping down
#define SLIDER_MAX_SPEED             99   // maximum speed

typedef enum
{
    SLIDER_DIRECTION_IN  = 0,
    SLIDER_DIRECTION_OUT = 1
} slider_direction_t;

#endif  // SLIDER_TYPES_H