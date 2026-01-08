#pragma once

#include <stdint.h>

#include "esp_err.h"
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct led_t *led_handle_t;

esp_err_t led_init(led_handle_t *out_handle);
    esp_err_t led_deinit(led_handle_t handle);
    esp_err_t led_set_pixel(led_handle_t handle, uint8_t red, uint8_t green, uint8_t blue);
    esp_err_t led_clear(led_handle_t handle);

#ifdef __cplusplus
}
#endif
