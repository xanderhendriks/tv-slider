/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include <stdio.h>

#include "drv8452.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "system.h"

static const char *TAG = "main";

static void IRAM_ATTR drv8452_fault_handler(drv8452_handle_t handle);

void app_main(void)
{
    drv8452_handle_t drv8452_handle = NULL;
    uint8_t          fault          = 0;
    drv8452_config_t drv_cfg        = {
               .step_pwm_timer     = LEDC_TIMER_0,
               .step_pwm_channel   = LEDC_CHANNEL_0,
               .step_pwm_gpio_num  = 9,
               .spi_host           = SPI2_HOST,
               .spi_mosi_io_num    = 22,
               .spi_miso_io_num    = 21,
               .spi_sclk_io_num    = 23,
               .spi_cs_io_num      = 15,
               .spi_clock_speed_hz = 1e6,
               .enable_gpio_num    = 19,
               .direction_gpio_num = 18,
               .fault_gpio_num     = 7,
               .sleep_gpio_num     = 13,
               .fault_callback     = drv8452_fault_handler,
    };

    system_init();

    ESP_ERROR_CHECK(drv8452_init(&drv_cfg, &drv8452_handle));
    ESP_ERROR_CHECK(drv8452_sleep(drv8452_handle, false));
    ESP_LOGI(TAG, "DRV8452 driver initialized");

    if (drv8452_register_read(drv8452_handle, DRV8452_REG_FAULT, &fault) == ESP_OK)
    {
        ESP_LOGI(TAG, "DRV8452 fault register: 0x%02X", fault);
    }
    else
    {
        ESP_LOGE(TAG, "Unable to read DRV8452 fault register");
    }

    ESP_ERROR_CHECK(drv8452_register_write(drv8452_handle, DRV8452_REG_MICROSTEP, DRV8452_MSTEP_32));
    ESP_ERROR_CHECK(drv8452_register_write(drv8452_handle, DRV8452_REG_TORQUE, 0x40));
    ESP_ERROR_CHECK(drv8452_register_write(drv8452_handle, DRV8452_REG_CTRL1, CTRL1_EN_OUT | CTRL1_CLR_FLT));

    ESP_ERROR_CHECK(drv8452_step_frequency(drv8452_handle, 6000));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(drv8452_step_frequency(drv8452_handle, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(drv8452_enable(drv8452_handle, true));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(drv8452_enable(drv8452_handle, false));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(drv8452_sleep(drv8452_handle, true));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(drv8452_sleep(drv8452_handle, false));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(drv8452_direction(drv8452_handle, true));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(drv8452_direction(drv8452_handle, false));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(drv8452_step_frequency(drv8452_handle, 200));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(drv8452_step_frequency(drv8452_handle, 0));

    ESP_LOGI(TAG, "Done");

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void drv8452_fault_handler(drv8452_handle_t handle)
{
    (void) handle;
    esp_rom_printf("DRV8452 fault interrupt\n");
}
