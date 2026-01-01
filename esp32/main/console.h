#pragma once

#include "drv8452.h"
#include "esp_err.h"
#include "shaft_encoder.h"

/**
 * Initialize the ESP-IDF console REPL and register application commands.
 *
 * @param drv_handle Initialized DRV8452 handle.
 * @param encoder_handle Initialized shaft encoder handle.
 * @return ESP_OK on success or an error from esp_console_* APIs.
 */
esp_err_t console_start(drv8452_handle_t drv_handle, shaft_encoder_handle_t encoder_handle);
