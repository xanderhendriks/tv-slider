#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        HALL_SENSOR_IN_STOP,
        HALL_SENSOR_IN_SLOW,
        HALL_SENSOR_OUT_SLOW,
        HALL_SENSOR_OUT_STOP,
    } hall_sensor_t;

    typedef struct hall_sensors_t *hall_sensors_handle_t;

    typedef void (*hall_sensor_callback_t)(hall_sensor_t sensor, void *user_ctx);

    typedef struct
    {
        int                    in_stop_gpio_num;
        int                    in_slow_gpio_num;
        int                    out_slow_gpio_num;
        int                    out_stop_gpio_num;
        bool                   active_low;
        bool                   pull_up_enable;
        bool                   pull_down_enable;
        hall_sensor_callback_t callback;
        void                  *user_ctx;
    } hall_sensors_config_t;

    esp_err_t hall_sensors_init(const hall_sensors_config_t *config, hall_sensors_handle_t *out_handle);
    esp_err_t hall_sensors_deinit(hall_sensors_handle_t handle);
    esp_err_t hall_sensors_get_state(hall_sensors_handle_t handle, uint8_t *state_mask);
    esp_err_t hall_sensors_set_invert(hall_sensors_handle_t handle, bool invert);

#ifdef __cplusplus
}
#endif
