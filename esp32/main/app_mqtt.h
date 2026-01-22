#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

typedef void (*mqtt_switch_cb_t)(bool on, void *user_ctx);

void mqtt_client_init(mqtt_switch_cb_t cb, void *user_ctx);
void mqtt_client_start(void);
void mqtt_client_apply_config(const config_data_t *cfg);
void mqtt_publish_state(bool on);
void mqtt_publish_position(int32_t position);
void mqtt_publish_status(bool on);