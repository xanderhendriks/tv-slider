#pragma once

#include "drv8452.h"
#include "esp_err.h"

/**
 * Initialize the ESP-IDF console REPL and register application commands.
 *
 * @param drv_handle Initialized DRV8452 handle.
 * @return ESP_OK on success or an error from esp_console_* APIs.
 */
esp_err_t console_start(drv8452_handle_t drv_handle);
