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

    typedef struct
    {
        ledc_mode_t      pwm_speed_mode;
        ledc_timer_t     pwm_timer;
        ledc_channel_t   pwm_channel;
        ledc_timer_bit_t pwm_duty_resolution;
        uint32_t         pwm_frequency_hz;
        uint32_t         initial_pwm_duty;
        int              pwm_gpio_num;

        spi_host_device_t spi_host;
        int               mosi_io_num;
        int               miso_io_num;
        int               sclk_io_num;
        int               cs_io_num;
        int               spi_clock_speed_hz;
        uint8_t           spi_mode;

        int      enable_gpio_num;
        uint32_t enable_pulse_ms;
    } drv8452_config_t;

    esp_err_t drv8452_init(const drv8452_config_t *config, drv8452_handle_t *out_handle);
    esp_err_t drv8452_read_reg(drv8452_handle_t handle, uint8_t addr, uint8_t *val);
    esp_err_t drv8452_write_reg(drv8452_handle_t handle, uint8_t addr, uint8_t val);
    esp_err_t drv8452_set_pwm_duty(drv8452_handle_t handle, uint32_t duty);
    esp_err_t drv8452_enable_output(drv8452_handle_t handle, bool enable);

#ifdef __cplusplus
}
#endif
