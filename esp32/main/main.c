/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

// Register addresses (examples — replace with your part’s values)
#define DRV8452_REG_FAULT 0x00
#define DRV8452_REG_CTRL1 0x04
#define DRV8452_REG_CTRL2 0x05
#define DRV8452_REG_MICROSTEP 0x06
#define DRV8452_REG_TORQUE 0x07

// CTRL1 bits (examples — replace to match datasheet)
#define CTRL1_EN_OUT (1u << 0)  // enable H-bridges
#define CTRL1_CLR_FLT (1u << 7) // clear latched faults, if available

// MICROSTEP modes (examples)
#define MSTEP_1 0x00
#define MSTEP_8 0x03
#define MSTEP_16 0x04
#define MSTEP_32 0x05

// TORQUE/Ilim setting (example)
#define TORQUE_50PCT 0x40 // pick a safe starting current

void init_pwm(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 6000,
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = 9, // GPIO pin number
        .duty = 128,   // Set duty to 50%. duty cycle = duty/2^duty_resolution
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

bool drv8452_read_reg(spi_device_handle_t dev, uint8_t addr, uint8_t *val)
{
    // Build 16-bit command: B15=0 (std frame), B14=1 (read), B13..B8=addr
    uint16_t cmd = (0u << 15) | (1u << 14) | ((addr & 0x3Fu) << 8) | 0x00u;

    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length = 16, // bits
    };
    t.tx_data[0] = (cmd >> 8) & 0xFF; // command byte
    t.tx_data[1] = cmd & 0xFF;        // data/don't-care

    esp_err_t err = spi_device_transmit(dev, &t);
    if (err != ESP_OK)
        return false;

    uint8_t status = t.rx_data[0]; // 8-bit status (MSBs must be 11)
    uint8_t report = t.rx_data[1]; // register data

    bool msb11 = (status & 0xC0) == 0xC0; // top two bits set
    if (val)
        *val = report;
    return msb11;
}

bool drv8452_write_reg(spi_device_handle_t dev, uint8_t addr, uint8_t val)
{
    // Build 16-bit command: B15=0 (std frame), B14=0 (write), B13..B8=addr
    uint16_t cmd = (0u << 15) | (0u << 14) | ((addr & 0x3Fu) << 8) | 0x00u;

    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length = 16, // bits
    };
    t.tx_data[0] = (cmd >> 8) & 0xFF; // command byte
    t.tx_data[1] = val;               // data byte to write

    esp_err_t err = spi_device_transmit(dev, &t);
    if (err != ESP_OK)
        return false;

    uint8_t status = t.rx_data[0];        // 8-bit status (MSBs must be 11)
    bool msb11 = (status & 0xC0) == 0xC0; // top two bits set
    return msb11;
}

// Initialize SPI bus and add a single device using the requested pins
spi_device_handle_t init_spi(void)
{
    esp_err_t ret;

    spi_bus_config_t buscfg = {
        .mosi_io_num = 22,
        .miso_io_num = 21,
        .sclk_io_num = 23,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    // Initialize the SPI bus (use SPI2_HOST for HSPI)
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);

    // Configure a default SPI device on the bus (chip select on GPIO15)
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1 * 1000 * 1000, // 1 MHz
        .mode = 1,
        .spics_io_num = 15,
        .queue_size = 2,
        .cs_ena_pretrans = 2,
        .cs_ena_posttrans = 2,
    };

    spi_device_handle_t handle = NULL;
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &handle);
    ESP_ERROR_CHECK(ret);

    return handle;
}

// Configure GPIO13 as output, drive low at startup, then set high
void gpio13_toggle_startup(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << 13),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Drive low, wait briefly, then drive high
    gpio_set_level(13, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(13, 1);
}

void app_main(void)
{
    // Initialize PWM
    init_pwm();
    printf("PWM output initialized on GPIO4\n");

    // Initialize SPI
    spi_device_handle_t spi = init_spi();
    if (spi)
    {
        printf("SPI initialized (CS=GPIO15, SCLK=GPIO23, MOSI=GPIO22, MISO=GPIO21)\n");
    }

    // Toggle GPIO13: low at startup, then high
    gpio13_toggle_startup();

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK)
    {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    uint8_t fault = 0;
    bool ok = drv8452_read_reg(spi, 0x00, &fault); // FAULT reg
    printf("SPI %s, FAULT=0x%02X\n", ok ? "OK" : "bad", fault);

    uint8_t ctrl3 = 0;
    ok = drv8452_read_reg(spi, 0x06, &ctrl3); // CTRL3 reg
    printf("SPI %s, CTRL3=0x%02X\n", ok ? "OK" : "bad", ctrl3);

    ok = drv8452_write_reg(spi, DRV8452_REG_MICROSTEP, MSTEP_32);
    printf("SPI %s\n", ok ? "OK" : "bad");

    // Set torque / current limit to a safe value
    ok = drv8452_write_reg(spi, DRV8452_REG_TORQUE, TORQUE_50PCT);
    printf("SPI %s\n", ok ? "OK" : "bad");

    // Enable outputs
    ok = drv8452_write_reg(spi, DRV8452_REG_CTRL1, 0x8F);
    printf("SPI %s\n", ok ? "OK" : "bad");
}
