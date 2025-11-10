#include "drv8452.h"

#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct drv8452_t
{
    spi_device_handle_t spi;
    ledc_mode_t         pwm_mode;
    ledc_channel_t      pwm_channel;
    ledc_timer_bit_t    duty_resolution;
    int                 enable_gpio;
} drv8452_ctx_t;

static const char *TAG = "drv8452";

static esp_err_t drv8452_init_pwm(const drv8452_config_t *config)
{
    if (config->pwm_gpio_num < 0)
    {
        ESP_LOGE(TAG, "PWM GPIO not configured");
        return ESP_ERR_INVALID_ARG;
    }

    ledc_timer_config_t ledc_timer = {
        .speed_mode      = config->pwm_speed_mode,
        .timer_num       = config->pwm_timer,
        .duty_resolution = config->pwm_duty_resolution,
        .freq_hz         = config->pwm_frequency_hz ? config->pwm_frequency_hz : 6000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };

    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure LEDC timer (%d)", err);
        return err;
    }

    uint32_t max_duty = (1u << config->pwm_duty_resolution) - 1u;
    uint32_t duty     = config->initial_pwm_duty;
    if (duty > max_duty)
    {
        duty = max_duty;
    }

    ledc_channel_config_t ledc_channel = {
        .speed_mode = config->pwm_speed_mode,
        .channel    = config->pwm_channel,
        .timer_sel  = config->pwm_timer,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = config->pwm_gpio_num,
        .duty       = duty,
        .hpoint     = 0,
    };

    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure LEDC channel (%d)", err);
    }

    return err;
}

static esp_err_t drv8452_init_spi(const drv8452_config_t *config, spi_device_handle_t *out_device)
{
    if (config->cs_io_num < 0)
    {
        ESP_LOGE(TAG, "SPI CS GPIO not configured");
        return ESP_ERR_INVALID_ARG;
    }

    spi_bus_config_t buscfg = {
        .mosi_io_num     = config->mosi_io_num,
        .miso_io_num     = config->miso_io_num,
        .sclk_io_num     = config->sclk_io_num,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4096,
    };

    esp_err_t err = spi_bus_initialize(config->spi_host, &buscfg, SPI_DMA_CH_AUTO);
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

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz   = config->spi_clock_speed_hz ? config->spi_clock_speed_hz : 1 * 1000 * 1000,
        .mode             = (config->spi_mode <= 3u) ? config->spi_mode : 1,
        .spics_io_num     = config->cs_io_num,
        .queue_size       = 2,
        .cs_ena_pretrans  = 2,
        .cs_ena_posttrans = 2,
    };

    err = spi_bus_add_device(config->spi_host, &devcfg, out_device);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add SPI device (%d)", err);
    }

    return err;
}

static esp_err_t drv8452_init_enable_gpio(const drv8452_config_t *config)
{
    if (config->enable_gpio_num < 0)
    {
        return ESP_OK;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->enable_gpio_num),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init enable GPIO (%d)", err);
        return err;
    }

    gpio_set_level(config->enable_gpio_num, 0);
    TickType_t delay_ticks = pdMS_TO_TICKS(config->enable_pulse_ms ? config->enable_pulse_ms : 10);
    vTaskDelay(delay_ticks);
    gpio_set_level(config->enable_gpio_num, 1);
    return ESP_OK;
}

esp_err_t drv8452_init(const drv8452_config_t *config, drv8452_handle_t *out_handle)
{
    if (!config || !out_handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *out_handle        = NULL;
    drv8452_ctx_t *ctx = calloc(1, sizeof(drv8452_ctx_t));
    if (!ctx)
    {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = drv8452_init_pwm(config);
    if (err != ESP_OK)
    {
        goto cleanup;
    }

    err = drv8452_init_spi(config, &ctx->spi);
    if (err != ESP_OK)
    {
        goto cleanup;
    }

    err = drv8452_init_enable_gpio(config);
    if (err != ESP_OK)
    {
        goto cleanup;
    }

    ctx->pwm_mode        = config->pwm_speed_mode;
    ctx->pwm_channel     = config->pwm_channel;
    ctx->duty_resolution = config->pwm_duty_resolution;
    ctx->enable_gpio     = config->enable_gpio_num;

    *out_handle = ctx;
    return ESP_OK;

cleanup:
    if (ctx->spi)
    {
        spi_bus_remove_device(ctx->spi);
    }
    free(ctx);
    return err;
}

esp_err_t drv8452_read_reg(drv8452_handle_t handle, uint8_t addr, uint8_t *val)
{
    if (!handle || !val)
    {
        return ESP_ERR_INVALID_ARG;
    }

    drv8452_ctx_t *ctx = handle;
    uint16_t       cmd = (0u << 15) | (1u << 14) | ((addr & 0x3Fu) << 8);

    spi_transaction_t t = {
        .flags  = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length = 16,
    };
    t.tx_data[0] = (cmd >> 8) & 0xFF;
    t.tx_data[1] = cmd & 0xFF;

    esp_err_t err = spi_device_transmit(ctx->spi, &t);
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

esp_err_t drv8452_write_reg(drv8452_handle_t handle, uint8_t addr, uint8_t val)
{
    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    drv8452_ctx_t *ctx = handle;
    uint16_t       cmd = (0u << 15) | (0u << 14) | ((addr & 0x3Fu) << 8);

    spi_transaction_t t = {
        .flags  = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length = 16,
    };
    t.tx_data[0] = (cmd >> 8) & 0xFF;
    t.tx_data[1] = val;

    esp_err_t err = spi_device_transmit(ctx->spi, &t);
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

esp_err_t drv8452_set_pwm_duty(drv8452_handle_t handle, uint32_t duty)
{
    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    drv8452_ctx_t *ctx      = handle;
    uint32_t       max_duty = (1u << ctx->duty_resolution) - 1u;
    if (duty > max_duty)
    {
        duty = max_duty;
    }

    esp_err_t err = ledc_set_duty(ctx->pwm_mode, ctx->pwm_channel, duty);
    if (err != ESP_OK)
    {
        return err;
    }
    return ledc_update_duty(ctx->pwm_mode, ctx->pwm_channel);
}

esp_err_t drv8452_enable_output(drv8452_handle_t handle, bool enable)
{
    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }
    drv8452_ctx_t *ctx = handle;
    if (ctx->enable_gpio < 0)
    {
        return ESP_OK;
    }
    return gpio_set_level(ctx->enable_gpio, enable ? 1 : 0);
}
