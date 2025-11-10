/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include <stdio.h>

#include "drv8452.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "sdkconfig.h"

void app_main(void)
{
    drv8452_config_t drv_cfg = {
        .pwm_speed_mode      = LEDC_LOW_SPEED_MODE,
        .pwm_timer           = LEDC_TIMER_0,
        .pwm_channel         = LEDC_CHANNEL_0,
        .pwm_duty_resolution = LEDC_TIMER_8_BIT,
        .pwm_frequency_hz    = 6000,
        .initial_pwm_duty    = 128,
        .pwm_gpio_num        = 9,
        .spi_host            = SPI2_HOST,
        .mosi_io_num         = 22,
        .miso_io_num         = 21,
        .sclk_io_num         = 23,
        .cs_io_num           = 15,
        .spi_clock_speed_hz  = 1 * 1000 * 1000,
        .spi_mode            = 1,
        .enable_gpio_num     = 13,
        .enable_pulse_ms     = 500,
    };

    drv8452_handle_t drv = NULL;
    ESP_ERROR_CHECK(drv8452_init(&drv_cfg, &drv));
    printf("DRV8452 driver initialized and PWM configured on GPIO%d\n", drv_cfg.pwm_gpio_num);

    uint8_t fault = 0;
    if (drv8452_read_reg(drv, DRV8452_REG_FAULT, &fault) == ESP_OK)
    {
        printf("DRV8452 fault register: 0x%02X\n", fault);
    }
    else
    {
        printf("Unable to read DRV8452 fault register\n");
    }

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t        flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ", CONFIG_IDF_TARGET, chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "", (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK)
    {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t) (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    ESP_ERROR_CHECK(drv8452_write_reg(drv, DRV8452_REG_MICROSTEP, DRV8452_MSTEP_32));
    ESP_ERROR_CHECK(drv8452_write_reg(drv, DRV8452_REG_TORQUE, 0x40));
    ESP_ERROR_CHECK(drv8452_write_reg(drv, DRV8452_REG_CTRL1, CTRL1_EN_OUT | CTRL1_CLR_FLT));
}
