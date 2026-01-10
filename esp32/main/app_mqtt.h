#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*mqtt_switch_cb_t)(bool on, void *user_ctx);

void mqtt_client_init(mqtt_switch_cb_t cb, void *user_ctx);
void mqtt_client_start(void);
void mqtt_publish_state(bool on);
void mqtt_publish_position(int32_t position);
