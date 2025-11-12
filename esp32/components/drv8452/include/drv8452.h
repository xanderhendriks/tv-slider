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
#define DRV8452_REG_FAULT        0x00
#define DRV8452_REG_DIAG1        0x01
#define DRV8452_REG_DIAG2        0x02
#define DRV8452_REG_DIAG3        0x03
#define DRV8452_REG_CTRL1        0x04
#define DRV8452_REG_CTRL2        0x05
#define DRV8452_REG_CTRL3        0x06
#define DRV8452_REG_CTRL4        0x07
#define DRV8452_REG_CTRL5        0x08
#define DRV8452_REG_CTRL6        0x09
#define DRV8452_REG_CTRL7        0x0A
#define DRV8452_REG_CTRL8        0x0B
#define DRV8452_REG_CTRL9        0x0C
#define DRV8452_REG_CTRL10       0x0D
#define DRV8452_REG_CTRL11       0x0E
#define DRV8452_REG_CTRL12       0x0F
#define DRV8452_REG_CTRL13       0x10
#define DRV8452_REG_INDEX1       0x11
#define DRV8452_REG_INDEX2       0x12
#define DRV8452_REG_INDEX3       0x13
#define DRV8452_REG_INDEX4       0x14
#define DRV8452_REG_INDEX5       0x15
#define DRV8452_REG_CUSTOM_CTRL1 0x16
#define DRV8452_REG_CUSTOM_CTRL2 0x17
#define DRV8452_REG_CUSTOM_CTRL3 0x18
#define DRV8452_REG_CUSTOM_CTRL4 0x19
#define DRV8452_REG_CUSTOM_CTRL5 0x1A
#define DRV8452_REG_CUSTOM_CTRL6 0x1B
#define DRV8452_REG_CUSTOM_CTRL7 0x1C
#define DRV8452_REG_CUSTOM_CTRL8 0x1D
#define DRV8452_REG_CUSTOM_CTRL9 0x1E
#define DRV8452_REG_ATQ_CTRL1    0x1F
#define DRV8452_REG_ATQ_CTRL2    0x20
#define DRV8452_REG_ATQ_CTRL3    0x21
#define DRV8452_REG_ATQ_CTRL4    0x22
#define DRV8452_REG_ATQ_CTRL5    0x23
#define DRV8452_REG_ATQ_CTRL6    0x24
#define DRV8452_REG_ATQ_CTRL7    0x25
#define DRV8452_REG_ATQ_CTRL8    0x26
#define DRV8452_REG_ATQ_CTRL9    0x27
#define DRV8452_REG_ATQ_CTRL10   0x28
#define DRV8452_REG_ATQ_CTRL11   0x29
#define DRV8452_REG_ATQ_CTRL12   0x2A
#define DRV8452_REG_ATQ_CTRL13   0x2B
#define DRV8452_REG_ATQ_CTRL14   0x2C
#define DRV8452_REG_ATQ_CTRL15   0x2D
#define DRV8452_REG_ATQ_CTRL16   0x2E
#define DRV8452_REG_ATQ_CTRL17   0x2F
#define DRV8452_REG_ATQ_CTRL18   0x30
#define DRV8452_REG_SS_CTRL1     0x31
#define DRV8452_REG_SS_CTRL2     0x32
#define DRV8452_REG_SS_CTRL3     0x33
#define DRV8452_REG_SS_CTRL4     0x34
#define DRV8452_REG_SS_CTRL5     0x35
#define DRV8452_REG_CTRL14       0x3C

/* Fault register bit masks */
#define DRV8452_FAULT_FAULT     0x80
#define DRV8452_FAULT_SPI_ERROR 0x40
#define DRV8452_FAULT_UVLO      0x20
#define DRV8452_FAULT_CPUV      0x10
#define DRV8452_FAULT_OCP       0x08
#define DRV8452_FAULT_STL       0x04
#define DRV8452_FAULT_TF        0x02
#define DRV8452_FAULT_OL        0x01

/* Control 1 register bit masks */
#define DRV8452_CTRL1_EN_OUT_DISABLED     0x00
#define DRV8452_CTRL1_EN_OUT_ENABLED      0x80
#define DRV8452_CTRL1_EN_OUT_MASK         0x80
#define DRV8452_CTRL1_SR_140_NS           0x00
#define DRV8452_CTRL1_SR_70_NS            0x40
#define DRV8452_CTRL1_SR_MASK             0x40
#define DRV8452_CTRL1_IDX_RST_LOW         0x00
#define DRV8452_CTRL1_IDX_RST_HIGH        0x20
#define DRV8452_CTRL1_IDX_RST_MASK        0x20
#define DRV8452_CTRL1_TOFF_9_5_US         0x00
#define DRV8452_CTRL1_TOFF_19_US          0x08
#define DRV8452_CTRL1_TOFF_27_US          0x10
#define DRV8452_CTRL1_TOFF_35_US          0x18
#define DRV8452_CTRL1_TOFF_MASK           0x18
#define DRV8452_CTRL1_DECAY_SLOW          0x00
#define DRV8452_CTRL1_DECAY_MIXED_30_PCT  0x04
#define DRV8452_CTRL1_DECAY_MIXED_60_PCT  0x05
#define DRV8452_CTRL1_DECAY_SMART_DYNAMIC 0x06
#define DRV8452_CTRL1_DECAY_SMART_RIPPLE  0x07
#define DRV8452_CTRL1_DECAY_MASK          0x07

/* Control 2 register bit masks */
#define DRV8452_CTRL2_DIR_LOW                     0x00
#define DRV8452_CTRL2_DIR_HIGH                    0x80
#define DRV8452_CTRL2_DIR_MASK                    0x80
#define DRV8452_CTRL2_STEP_LOW                    0x00
#define DRV8452_CTRL2_STEP_HIGH                   0x40
#define DRV8452_CTRL2_STEP_MASK                   0x40
#define DRV8452_CTRL2_SPI_DIR_PIN                 0x00
#define DRV8452_CTRL2_SPI_DIR_BIT                 0x20
#define DRV8452_CTRL2_SPI_DIR_MASK                0x20
#define DRV8452_CTRL2_SPI_STEP_PIN                0x00
#define DRV8452_CTRL2_SPI_STEP_BIT                0x10
#define DRV8452_CTRL2_SPI_STEP_MASK               0x10
#define DRV8452_CTRL2_MICROSTEP_MODE_FULL_100     0x00
#define DRV8452_CTRL2_MICROSTEP_MODE_FULL_71      0x01
#define DRV8452_CTRL2_MICROSTEP_MODE_HALF_NO_CIRC 0x02
#define DRV8452_CTRL2_MICROSTEP_MODE_HALF         0x03
#define DRV8452_CTRL2_MICROSTEP_MODE_QUARTER      0x04
#define DRV8452_CTRL2_MICROSTEP_MODE_1_OVER_8     0x05
#define DRV8452_CTRL2_MICROSTEP_MODE_1_OVER_16    0x06
#define DRV8452_CTRL2_MICROSTEP_MODE_1_OVER_32    0x07
#define DRV8452_CTRL2_MICROSTEP_MODE_1_OVER_64    0x08
#define DRV8452_CTRL2_MICROSTEP_MODE_1_OVER_128   0x09
#define DRV8452_CTRL2_MICROSTEP_MODE_1_OVER_256   0x0A
#define DRV8452_CTRL2_MICROSTEP_MODE_MASK         0x0F

/* Control 3 register bit masks */
#define DRV8452_CTRL3_CLR_FLT_LOW          0x00
#define DRV8452_CTRL3_CLR_FLT_HIGH         0x80
#define DRV8452_CTRL3_CLR_FLT_MASK         0x80
#define DRV8452_CTRL3_LOCK_UNLOCK          0x30
#define DRV8452_CTRL3_LOCK_LOCK            0x60
#define DRV8452_CTRL3_LOCK_MASK            0x70
#define DRV8452_CTRL3_TOCP_1_2_US          0x00
#define DRV8452_CTRL3_TOCP_2_2_US          0x08
#define DRV8452_CTRL3_TOCP_MASK            0x08
#define DRV8452_CTRL3_OCP_MODE_LATCHED     0x00
#define DRV8452_CTRL3_OCP_MODE_AUTO_RETRY  0x04
#define DRV8452_CTRL3_OCP_MODE_MASK        0x04
#define DRV8452_CTRL3_OTSD_MODE_LATCHED    0x00
#define DRV8452_CTRL3_OTSD_MODE_AUTO_RETRY 0x02
#define DRV8452_CTRL3_OTSD_MODE_MASK       0x02
#define DRV8452_CTRL3_TW_REP_DISABLED      0x00
#define DRV8452_CTRL3_TW_REP_ENABLED       0x01
#define DRV8452_CTRL3_TW_REP_MASK          0x01

/* Control 4 register bit masks */
#define DRV8452_CTRL4_TBLANK_TIME_1_US     0x00
#define DRV8452_CTRL4_TBLANK_TIME_1_5_US   0x40
#define DRV8452_CTRL4_TBLANK_TIME_2_US     0x80
#define DRV8452_CTRL4_TBLANK_TIME_2_5_US   0xC0
#define DRV8452_CTRL4_TBLANK_TIME_MASK     0xC0
#define DRV8452_CTRL4_STL_LRN_DIS          0x00
#define DRV8452_CTRL4_STL_LRN_EN           0x20
#define DRV8452_CTRL4_STL_LRN_MASK         0x20
#define DRV8452_CTRL4_EN_STL_DIS           0x00
#define DRV8452_CTRL4_EN_STL_EN            0x10
#define DRV8452_CTRL4_EN_STL_MASK          0x10
#define DRV8452_CTRL4_STL_REP_DIS          0x00
#define DRV8452_CTRL4_STL_REP_EN           0x08
#define DRV8452_CTRL4_STL_REP_MASK         0x08
#define DRV8452_CTRL4_FRQ_CHG_FILTERED     0x00
#define DRV8452_CTRL4_FRQ_CHG_NOT_FILTERED 0x04
#define DRV8452_CTRL4_FRQ_CHG_MASK         0x04
#define DRV8452_CTRL4_STEP_FRQ_TOL_1_PCT   0x00
#define DRV8452_CTRL4_STEP_FRQ_TOL_2_PCT   0x01
#define DRV8452_CTRL4_STEP_FRQ_TOL_4_PCT   0x02
#define DRV8452_CTRL4_STEP_FRQ_TOL_6_PCT   0x03
#define DRV8452_CTRL4_STEP_FRQ_TOL_MASK    0x03

/* Control 11 register bit masks */
#define DRV8452_CTRL11_TRQ_DAC_100_PCT  0xFF
#define DRV8452_CTRL11_TRQ_DAC_87_5_PCT 0xDF
#define DRV8452_CTRL11_TRQ_DAC_75_PCT   0xBF
#define DRV8452_CTRL11_TRQ_DAC_62_5_PCT 0x9F
#define DRV8452_CTRL11_TRQ_DAC_50_PCT   0x7F
#define DRV8452_CTRL11_TRQ_DAC_37_5_PCT 0x5F
#define DRV8452_CTRL11_TRQ_DAC_25_PCT   0x3F
#define DRV8452_CTRL11_TRQ_DAC_12_5_PCT 0x1F

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
