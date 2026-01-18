#ifndef SLIDER_H
#define SLIDER_H

#include <stdbool.h>
#include <stdint.h>

#include "drv8452.h"
#include "hall_sensors.h"
#include "slider_state_machine.h"
#include "slider_types.h"

void slider_init(hall_sensors_handle_t hall_sensors, drv8452_handle_t drv8452);
void slider_motor_enable(bool enable);
void slider_speed_set(uint16_t speed);
void slider_direction_set(slider_direction_t direction);
void slider_start_timer(uint32_t interval_ms);
bool slider_is_at_in_stop();
bool slider_is_at_out_stop();
void slider_fault_handler();

// Enqueue an event to be processed by the slider state machine task
bool slider_post_event(slider_state_machine_EventId event);

#endif  // SLIDER_H