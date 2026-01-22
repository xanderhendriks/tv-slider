/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include <stdio.h>

#include "app_mqtt.h"
#include "config.h"
#include "console.h"
#include "drv8452.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "hall_sensors.h"
#include "led.h"
#include "nvs_flash.h"
#include "provisioning.h"
#include "shaft_encoder.h"
#include "slider.h"
#include "system.h"
#include "webserver.h"

static const char *TAG = "main";

typedef struct
{
    hall_sensors_handle_t hall_handle;
} apply_ctx_t;

static void drv8452_fault_handler(drv8452_handle_t handle);
static void hall_sensor_handler(hall_sensor_t sensor, void *user_ctx);
static void mqtt_switch_handler(bool on, void *user_ctx);
static void apply_config_cb(const config_data_t *cfg, void *user_ctx);

void app_main(void)
{
    drv8452_handle_t       drv8452_handle = NULL;
    shaft_encoder_handle_t encoder_handle = NULL;
    hall_sensors_handle_t  hall_handle    = NULL;
    led_handle_t           led_handle     = NULL;
    apply_ctx_t            apply_ctx      = {0};
    uint8_t                value          = 0;
    drv8452_config_t       drv_cfg        = {
                     .step_pwm_timer     = LEDC_TIMER_0,
                     .step_pwm_channel   = LEDC_CHANNEL_0,
                     .step_pwm_gpio_num  = 0,
                     .spi_host           = SPI2_HOST,
                     .spi_mosi_io_num    = 10,
                     .spi_miso_io_num    = 11,
                     .spi_sclk_io_num    = 1,
                     .spi_cs_io_num      = 2,
                     .spi_clock_speed_hz = 1e6,
                     .direction_gpio_num = 7,
                     .fault_gpio_num     = 3,
                     .sleep_gpio_num     = 6,
                     .fault_callback     = drv8452_fault_handler,
    };
    shaft_encoder_config_t encoder_cfg = {
        .count_gpio_num     = 4,
        .direction_gpio_num = 5,
        .low_limit          = -32768,
        .high_limit         = 32767,
        .glitch_filter_ns   = 0,
        .invert_direction   = false,
    };
    hall_sensors_config_t hall_cfg = {
        .in_stop_gpio_num  = 15,
        .in_slow_gpio_num  = 23,
        .out_slow_gpio_num = 22,
        .out_stop_gpio_num = 21,
        .active_low        = true,
        .pull_up_enable    = true,
        .pull_down_enable  = false,
        .callback          = hall_sensor_handler,
        .user_ctx          = NULL,
    };

    system_init();
    mqtt_client_init(mqtt_switch_handler, NULL);

    // Initialize NVS flash for persistent storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    else
    {
        ESP_ERROR_CHECK(ret);
    }

    // Initialize configuration module
    ESP_ERROR_CHECK(config_init());

    // Register config apply callback before webserver may start
    webserver_set_config_apply_callback(apply_config_cb, &apply_ctx);

    ble_provisioning_start();

    ESP_ERROR_CHECK(led_init(&led_handle));
    // led_set_pixel(led_handle, 128, 0, 128);
    ESP_LOGI(TAG, "LED initialized");

    ESP_ERROR_CHECK(drv8452_init(&drv_cfg, &drv8452_handle));
    ESP_LOGI(TAG, "DRV8452 driver initialized");

    ESP_ERROR_CHECK(shaft_encoder_init(&encoder_cfg, &encoder_handle));
    ESP_LOGI(TAG, "Shaft encoder initialized");

    ESP_ERROR_CHECK(hall_sensors_init(&hall_cfg, &hall_handle));
    ESP_LOGI(TAG, "Hall sensors initialized");
    apply_ctx.hall_handle = hall_handle;
    // Apply initial config (hall inversion and MQTT restart)
    config_data_t cfg;
    if (config_get(&cfg) == ESP_OK)
    {
        apply_config_cb(&cfg, &apply_ctx);
    }

    ESP_ERROR_CHECK(drv8452_sleep(drv8452_handle, false));
    vTaskDelay(pdMS_TO_TICKS(10));

    if (drv8452_register_read(drv8452_handle, DRV8452_REG_FAULT, &value) == ESP_OK)
    {
        ESP_LOGI(TAG, "DRV8452 fault register: 0x%02X", value);

        ESP_ERROR_CHECK(
            drv8452_register_write(drv8452_handle, DRV8452_REG_CTRL2, DRV8452_CTRL2_MICROSTEP_MODE_1_OVER_128));
        ESP_ERROR_CHECK(drv8452_register_read(drv8452_handle, DRV8452_REG_CTRL4, &value));
        ESP_LOGI(TAG, "DRV8452 CTRL4 register: 0x%02X", value);
        ESP_ERROR_CHECK(drv8452_register_write(drv8452_handle, DRV8452_REG_CTRL4, value | DRV8452_CTRL4_EN_STL_EN));
        ESP_ERROR_CHECK(drv8452_register_write(drv8452_handle, DRV8452_REG_CTRL11, DRV8452_CTRL11_TRQ_DAC_37_5_PCT));
        ESP_ERROR_CHECK(drv8452_register_read(drv8452_handle, DRV8452_REG_CTRL13, &value));
        ESP_LOGI(TAG, "DRV8452 CTRL13 register: 0x%02X", value);
        ESP_ERROR_CHECK(
            drv8452_register_write(drv8452_handle, DRV8452_REG_CTRL13, value | DRV8452_CTRL13_VREF_INT_EN_EN));

        ESP_LOGI(TAG, "DRV8452 configured");
    }
    else
    {
        ESP_LOGE(TAG, "Unable to read DRV8452 fault register");
    }

    ESP_LOGI(TAG, "Starting console...");
    ESP_ERROR_CHECK(console_start(drv8452_handle, encoder_handle, hall_handle, led_handle));

    slider_init(hall_handle, drv8452_handle);

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static IRAM_ATTR void drv8452_fault_handler(drv8452_handle_t handle)
{
    (void) handle;
    esp_rom_printf("DRV8452 fault interrupt\n");
}

static void IRAM_ATTR hall_sensor_handler(hall_sensor_t sensor, void *user_ctx)
{
    (void) user_ctx;
    const char *name = "UNKNOWN";

    switch (sensor)
    {
        case HALL_SENSOR_IN_STOP:
            name = "IN_STOP";
            slider_post_event(slider_state_machine_EventId_SENSOR_IN_STOP);
            slider_position(0);
            break;
        case HALL_SENSOR_IN_SLOW:
            name = "IN_SLOW";
            slider_post_event(slider_state_machine_EventId_SENSOR_IN_SLOW);
            slider_position(25);
            break;
        case HALL_SENSOR_OUT_SLOW:
            name = "OUT_SLOW";
            slider_post_event(slider_state_machine_EventId_SENSOR_OUT_SLOW);
            slider_position(75);
            break;
        case HALL_SENSOR_OUT_STOP:
            name = "OUT_STOP";
            slider_post_event(slider_state_machine_EventId_SENSOR_OUT_STOP);
            slider_position(100);
            break;
        default:
            break;
    }

    esp_rom_printf("Hall sensor triggered: %s\n", name);
}

static void mqtt_switch_handler(bool on, void *user_ctx)
{
    (void) user_ctx;
    ESP_LOGI(TAG, "MQTT switch update: %s", on ? "on" : "off");
    slider_post_event(on ? slider_state_machine_EventId_CMD_MOVE_OUT : slider_state_machine_EventId_CMD_MOVE_IN);
    slider_status(on);
}

static void apply_config_cb(const config_data_t *cfg, void *user_ctx)
{
    if (!cfg)
    {
        return;
    }

    // Apply hall sensor inversion
    if (user_ctx)
    {
        apply_ctx_t *ctx = (apply_ctx_t *) user_ctx;
        if (ctx->hall_handle)
        {
            hall_sensors_set_invert(ctx->hall_handle, cfg->invert_inputs);
            ESP_LOGI(TAG, "Hall sensor invert_inputs=%d applied", cfg->invert_inputs);
        }
    }

    // Reconfigure MQTT client
    mqtt_client_apply_config(cfg);
}
