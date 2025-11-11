#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* Register addresses */
#define DRV8452_REG_FAULT     0x00
#define DRV8452_REG_CTRL1     0x04
#define DRV8452_REG_CTRL2     0x05
#define DRV8452_REG_MICROSTEP 0x06
#define DRV8452_REG_TORQUE    0x07

/* Useful bit fields */
#define CTRL1_EN_OUT  (1u << 0)
#define CTRL1_CLR_FLT (1u << 7)

/* Microstep presets */
#define DRV8452_MSTEP_1  0x00
#define DRV8452_MSTEP_8  0x03
#define DRV8452_MSTEP_16 0x04
#define DRV8452_MSTEP_32 0x05

    typedef struct drv8452_t *drv8452_handle_t;
    typedef void (*drv8452_fault_callback_t)(drv8452_handle_t handle);

    typedef struct
    {
        int enable_gpio_num;
        int direction_gpio_num;
        int fault_gpio_num;
        int sleep_gpio_num;

        ledc_timer_t   step_pwm_timer;
        ledc_channel_t step_pwm_channel;
        int            step_pwm_gpio_num;

        spi_host_device_t spi_host;
        int               spi_mosi_io_num;
        int               spi_miso_io_num;
        int               spi_sclk_io_num;
        int               spi_cs_io_num;
        int               spi_clock_speed_hz;
        uint8_t           spi_mode;

        drv8452_fault_callback_t fault_callback;
    } drv8452_config_t;

    esp_err_t drv8452_init(const drv8452_config_t *config, drv8452_handle_t *out_handle);
    esp_err_t drv8452_register_read(drv8452_handle_t handle, uint8_t addr, uint8_t *val);
    esp_err_t drv8452_register_write(drv8452_handle_t handle, uint8_t addr, uint8_t val);
    esp_err_t drv8452_step_frequency(drv8452_handle_t handle, uint32_t frequency_hz);
    esp_err_t drv8452_enable(drv8452_handle_t handle, bool enable);
    esp_err_t drv8452_sleep(drv8452_handle_t handle, bool enable);
    esp_err_t drv8452_direction(drv8452_handle_t handle, bool enable);

#ifdef __cplusplus
}
#endif
