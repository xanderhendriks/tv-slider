#include "drv8452.h"

#include <stdbool.h>
#include <stdlib.h>

#include "esp_attr.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct drv8452_t
{
    spi_device_handle_t      spi;
    ledc_mode_t              step_pwm_mode;
    ledc_channel_t           step_pwm_channel;
    ledc_timer_t             step_pwm_timer;
    int                      enable_gpio_num;
    int                      sleep_gpio_num;
    int                      direction_gpio_num;
    int                      fault_gpio_num;
    bool                     fault_isr_registered;
    drv8452_fault_callback_t fault_callback;
} drv8452_ctx_t;

static const char *TAG = "drv8452";

static esp_err_t drv8452_init_gpio(const drv8452_config_t *config, drv8452_ctx_t *ctx);
static esp_err_t drv8452_init_pwm(const drv8452_config_t *config, ledc_mode_t mode);
static esp_err_t drv8452_init_spi(const drv8452_config_t *config, spi_device_handle_t *out_device);
static esp_err_t drv8452_install_gpio_isr_service(gpio_num_t gpio_num, gpio_isr_t isr_handler, drv8452_ctx_t *ctx);
static void      drv8452_remove_fault_isr(drv8452_ctx_t *ctx);
static void      drv8452_fault_isr(void *arg);

esp_err_t drv8452_init(const drv8452_config_t *config, drv8452_handle_t *out_handle)
{
    esp_err_t      err;
    drv8452_ctx_t *ctx;

    if (!config || !out_handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *out_handle = NULL;
    ctx         = calloc(1, sizeof(drv8452_ctx_t));
    if (!ctx)
    {
        return ESP_ERR_NO_MEM;
    }

    ctx->step_pwm_mode        = LEDC_LOW_SPEED_MODE;
    ctx->step_pwm_channel     = config->step_pwm_channel;
    ctx->step_pwm_timer       = config->step_pwm_timer;
    ctx->enable_gpio_num      = config->enable_gpio_num;
    ctx->sleep_gpio_num       = config->sleep_gpio_num;
    ctx->direction_gpio_num   = config->direction_gpio_num;
    ctx->fault_gpio_num       = config->fault_gpio_num;
    ctx->fault_callback       = config->fault_callback;
    ctx->fault_isr_registered = false;

    err = drv8452_init_gpio(config, ctx);
    if (err != ESP_OK)
    {
        drv8452_remove_fault_isr(ctx);
        free(ctx);
        return err;
    }

    err = drv8452_init_pwm(config, ctx->step_pwm_mode);
    if (err != ESP_OK)
    {
        drv8452_remove_fault_isr(ctx);
        free(ctx);
        return err;
    }

    err = drv8452_init_spi(config, &ctx->spi);
    if (err != ESP_OK)
    {
        drv8452_remove_fault_isr(ctx);
        free(ctx);
        return err;
    }

    *out_handle = ctx;
    return ESP_OK;
}

esp_err_t drv8452_register_read(drv8452_handle_t handle, uint8_t addr, uint8_t *val)
{
    esp_err_t         err;
    drv8452_ctx_t    *ctx = handle;
    uint16_t          cmd = (0u << 15) | (1u << 14) | ((addr & 0x3Fu) << 8);
    spi_transaction_t t   = {
          .flags  = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
          .length = 16,
    };

    if (!handle || !val)
    {
        return ESP_ERR_INVALID_ARG;
    }

    t.tx_data[0] = (cmd >> 8) & 0xFF;
    t.tx_data[1] = cmd & 0xFF;

    err = spi_device_transmit(ctx->spi, &t);
    if (err != ESP_OK)
    {
        return err;
    }

    if ((t.rx_data[0] & 0xC0) != 0xC0)
    {
        return ESP_FAIL;
    }

    *val = t.rx_data[1];
    return ESP_OK;
}

esp_err_t drv8452_register_write(drv8452_handle_t handle, uint8_t addr, uint8_t val)
{
    esp_err_t         err;
    drv8452_ctx_t    *ctx = handle;
    uint16_t          cmd = (0u << 15) | (0u << 14) | ((addr & 0x3Fu) << 8);
    spi_transaction_t t   = {
          .flags  = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
          .length = 16,
    };

    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    t.tx_data[0] = (cmd >> 8) & 0xFF;
    t.tx_data[1] = val;

    err = spi_device_transmit(ctx->spi, &t);
    if (err != ESP_OK)
    {
        return err;
    }

    if ((t.rx_data[0] & 0xC0) != 0xC0)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t drv8452_step_frequency(drv8452_handle_t handle, uint32_t frequency_hz)
{
    esp_err_t      err;
    drv8452_ctx_t *ctx = handle;

    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (frequency_hz == 0)
    {
        // Stop PWM
        err = ledc_stop(ctx->step_pwm_mode, ctx->step_pwm_channel, 0);
    }
    else
    {
        err = ledc_set_freq(ctx->step_pwm_mode, ctx->step_pwm_channel, frequency_hz);
        ledc_set_duty(ctx->step_pwm_mode, ctx->step_pwm_channel, 128);  // 50% with 8‑bit res
        ledc_update_duty(ctx->step_pwm_mode, ctx->step_pwm_channel);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set PWM frequency (%d)", err);
            return err;
        }
    }

    return ESP_OK;
}

esp_err_t drv8452_enable(drv8452_handle_t handle, bool enable)
{
    drv8452_ctx_t *ctx = handle;

    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (ctx->enable_gpio_num < 0)
    {
        return ESP_OK;
    }

    return gpio_set_level(ctx->enable_gpio_num, enable ? 1 : 0);
}

esp_err_t drv8452_sleep(drv8452_handle_t handle, bool enable)
{
    drv8452_ctx_t *ctx = handle;

    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (ctx->sleep_gpio_num < 0)
    {
        return ESP_OK;
    }

    return gpio_set_level(ctx->sleep_gpio_num, enable ? 0 : 1);
}

esp_err_t drv8452_direction(drv8452_handle_t handle, bool enable)
{
    drv8452_ctx_t *ctx = handle;

    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (ctx->direction_gpio_num < 0)
    {
        return ESP_OK;
    }
    return gpio_set_level(ctx->direction_gpio_num, enable ? 1 : 0);
}

static esp_err_t drv8452_init_gpio(const drv8452_config_t *config, drv8452_ctx_t *ctx)
{
    esp_err_t err;

    if (config->enable_gpio_num >= 0)
    {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << config->enable_gpio_num),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };

        err = gpio_config(&io_conf);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to init enable GPIO (%d)", err);
            return err;
        }

        gpio_set_level(config->enable_gpio_num, 0);
    }

    if (config->direction_gpio_num >= 0)
    {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << config->direction_gpio_num),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        err = gpio_config(&io_conf);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to init direction GPIO (%d)", err);
            return err;
        }

        gpio_set_level(config->direction_gpio_num, 0);
    }

    if (config->sleep_gpio_num >= 0)
    {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << config->sleep_gpio_num),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };

        err = gpio_config(&io_conf);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to init sleep GPIO (%d)", err);
            return err;
        }

        gpio_set_level(config->sleep_gpio_num, 0);
    }

    if (config->fault_gpio_num >= 0)
    {
        gpio_config_t fault_conf = {
            .pin_bit_mask = (1ULL << config->fault_gpio_num),
            .mode         = GPIO_MODE_INPUT,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .intr_type    = config->fault_callback ? GPIO_INTR_NEGEDGE : GPIO_INTR_DISABLE,
        };

        err = gpio_config(&fault_conf);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to init fault GPIO (%d)", err);
            return err;
        }

        if (config->fault_callback)
        {
            err = drv8452_install_gpio_isr_service(config->fault_gpio_num, drv8452_fault_isr, ctx);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to install GPIO ISR service (%d)", err);
                return err;
            }
        }
    }

    return ESP_OK;
}

static esp_err_t drv8452_init_pwm(const drv8452_config_t *config, ledc_mode_t mode)
{
    esp_err_t           err;
    ledc_timer_config_t ledc_timer = {
        .speed_mode      = mode,
        .timer_num       = config->step_pwm_timer,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz         = 1000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_channel_config_t ledc_channel = {
        .speed_mode = mode,
        .channel    = config->step_pwm_channel,
        .timer_sel  = config->step_pwm_timer,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = config->step_pwm_gpio_num,
        .duty       = 128,
        .hpoint     = 0,
    };

    if (config->step_pwm_gpio_num < 0)
    {
        ESP_LOGE(TAG, "PWM GPIO not configured");
        return ESP_ERR_INVALID_ARG;
    }

    err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure LEDC timer (%d)", err);
        return err;
    }

    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure LEDC channel (%d)", err);
        return err;
    }

    err = ledc_stop(LEDC_LOW_SPEED_MODE, config->step_pwm_channel, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop PWM channel (%d)", err);
        return err;
    }

    return ESP_OK;
}

static esp_err_t drv8452_init_spi(const drv8452_config_t *config, spi_device_handle_t *out_device)
{
    esp_err_t        err;
    spi_bus_config_t buscfg = {
        .mosi_io_num     = config->spi_mosi_io_num,
        .miso_io_num     = config->spi_miso_io_num,
        .sclk_io_num     = config->spi_sclk_io_num,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4096,
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz   = config->spi_clock_speed_hz,
        .mode             = 1,
        .spics_io_num     = config->spi_cs_io_num,
        .queue_size       = 2,
        .cs_ena_pretrans  = 2,
        .cs_ena_posttrans = 2,
    };

    if (config->spi_cs_io_num < 0)
    {
        ESP_LOGE(TAG, "SPI CS GPIO not configured");
        return ESP_ERR_INVALID_ARG;
    }

    err = spi_bus_initialize(config->spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (err == ESP_ERR_INVALID_STATE)
    {
        // Bus already initialized elsewhere; treat as success
        err = ESP_OK;
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SPI bus (%d)", err);
        return err;
    }

    err = spi_bus_add_device(config->spi_host, &devcfg, out_device);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add SPI device (%d)", err);
    }

    return err;
}

static esp_err_t drv8452_install_gpio_isr_service(gpio_num_t gpio_num, gpio_isr_t isr_handler, drv8452_ctx_t *ctx)
{
    esp_err_t err;

    err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err == ESP_ERR_INVALID_STATE)
    {
        return ESP_OK;
    }

    err = gpio_isr_handler_add(gpio_num, isr_handler, ctx);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add fault ISR handler (%d)", err);
        return err;
    }
    ctx->fault_isr_registered = true;

    err = gpio_intr_enable(gpio_num);
    if (err != ESP_OK)
    {
        drv8452_remove_fault_isr(ctx);
        ESP_LOGE(TAG, "Failed to enable fault GPIO interrupt (%d)", err);
        return err;
    }

    return err;
}

static void drv8452_remove_fault_isr(drv8452_ctx_t *ctx)
{
    if (!ctx || !ctx->fault_isr_registered || ctx->fault_gpio_num < 0)
    {
        return;
    }
    gpio_isr_handler_remove(ctx->fault_gpio_num);
    ctx->fault_isr_registered = false;
}

static void IRAM_ATTR drv8452_fault_isr(void *arg)
{
    drv8452_ctx_t *ctx = (drv8452_ctx_t *) arg;
    if (ctx && ctx->fault_callback)
    {
        ctx->fault_callback(ctx);
    }
}
