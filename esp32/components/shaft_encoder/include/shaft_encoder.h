#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct shaft_encoder_t *shaft_encoder_handle_t;

    typedef struct
    {
        int32_t  count_gpio_num;
        int32_t  direction_gpio_num;
        int32_t  low_limit;
        int32_t  high_limit;
        uint32_t glitch_filter_ns;
        bool     invert_direction;
    } shaft_encoder_config_t;

    esp_err_t shaft_encoder_init(const shaft_encoder_config_t *config, shaft_encoder_handle_t *out_handle);
    esp_err_t shaft_encoder_start(shaft_encoder_handle_t handle);
    esp_err_t shaft_encoder_stop(shaft_encoder_handle_t handle);
    esp_err_t shaft_encoder_clear(shaft_encoder_handle_t handle);
    esp_err_t shaft_encoder_get_count(shaft_encoder_handle_t handle, int32_t *count);
    esp_err_t shaft_encoder_deinit(shaft_encoder_handle_t handle);

#ifdef __cplusplus
}
#endif
