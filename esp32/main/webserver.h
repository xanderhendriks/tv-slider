#pragma once

#include "config.h"

typedef void (*config_apply_cb)(const config_data_t *cfg, void *user_ctx);

void webserver_set_config_apply_callback(config_apply_cb cb, void *user_ctx);
void webserver_start(void);
