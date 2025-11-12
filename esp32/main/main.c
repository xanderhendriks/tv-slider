/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include <stdio.h>

#include "console.h"
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
    uint8_t          value          = 0;
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

    if (drv8452_register_read(drv8452_handle, DRV8452_REG_FAULT, &value) == ESP_OK)
    {
        ESP_LOGI(TAG, "DRV8452 fault register: 0x%02X", value);
    }
    else
    {
        ESP_LOGE(TAG, "Unable to read DRV8452 fault register");
    }

    ESP_ERROR_CHECK(drv8452_register_write(drv8452_handle, DRV8452_REG_CTRL2, DRV8452_CTRL2_MICROSTEP_MODE_1_OVER_128));
    ESP_ERROR_CHECK(drv8452_register_read(drv8452_handle, DRV8452_REG_CTRL4, &value));
    ESP_LOGI(TAG, "DRV8452 CTRL4 register: 0x%02X", value);
    ESP_ERROR_CHECK(drv8452_register_write(drv8452_handle, DRV8452_REG_CTRL4, value | DRV8452_CTRL4_EN_STL_EN));
    ESP_ERROR_CHECK(drv8452_register_write(drv8452_handle, DRV8452_REG_CTRL11, DRV8452_CTRL11_TRQ_DAC_50_PCT));

    ESP_LOGI(TAG, "DRV8452 configured");

    ESP_LOGI(TAG, "Starting console...");
    ESP_ERROR_CHECK(console_start(drv8452_handle));

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
