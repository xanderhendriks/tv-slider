#include "led.h"

#include <stdlib.h>

#include "led_strip.h"

typedef struct led_t
{
    led_strip_handle_t strip;
} led_ctx_t;

esp_err_t led_init(led_handle_t *out_handle)
{
    led_ctx_t *ctx = NULL;

    if (!out_handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *out_handle = NULL;
    ctx         = calloc(1, sizeof(*ctx));
    if (!ctx)
    {
        return ESP_ERR_NO_MEM;
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num         = 8,
        .max_leds               = 1,
        .led_model              = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags =
            {
                .invert_out = false,
            },
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags =
            {
                .with_dma = false,
            },
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &ctx->strip);
    if (err != ESP_OK)
    {
        free(ctx);
        return err;
    }

    err = led_clear(ctx);
    if (err != ESP_OK)
    {
        led_strip_del(ctx->strip);
        free(ctx);
        return err;
    }

    *out_handle = ctx;
    return ESP_OK;
}

esp_err_t led_deinit(led_handle_t handle)
{
    led_ctx_t *ctx = handle;

    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = led_strip_del(ctx->strip);
    free(ctx);
    return err;
}

esp_err_t led_set_pixel(led_handle_t handle, uint8_t red, uint8_t green, uint8_t blue)
{
    led_ctx_t *ctx = handle;

    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = led_strip_set_pixel(ctx->strip, 0, red, green, blue);
    if (err != ESP_OK)
    {
        return err;
    }

    return led_strip_refresh(ctx->strip);
}

esp_err_t led_clear(led_handle_t handle)
{
    led_ctx_t *ctx = handle;

    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = led_strip_clear(ctx->strip);
    if (err != ESP_OK)
    {
        return err;
    }

    return led_strip_refresh(ctx->strip);
}
