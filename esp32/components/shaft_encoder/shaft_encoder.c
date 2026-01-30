#include "shaft_encoder.h"

#include <stdlib.h>

#include "driver/pulse_cnt.h"

typedef struct shaft_encoder_t
{
    pcnt_unit_handle_t    unit;
    pcnt_channel_handle_t channel;
} shaft_encoder_ctx_t;

static esp_err_t shaft_encoder_configure_channel(shaft_encoder_ctx_t *ctx, const shaft_encoder_config_t *config);
static void      shaft_encoder_cleanup(shaft_encoder_ctx_t *ctx);

esp_err_t shaft_encoder_init(const shaft_encoder_config_t *config, shaft_encoder_handle_t *out_handle)
{
    esp_err_t            err;
    shaft_encoder_ctx_t *ctx;

    if (!config || !out_handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (config->count_gpio_num < 0 || config->direction_gpio_num < 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (config->low_limit >= 0 || config->high_limit <= 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *out_handle = NULL;
    ctx         = calloc(1, sizeof(shaft_encoder_ctx_t));
    if (!ctx)
    {
        return ESP_ERR_NO_MEM;
    }

    pcnt_unit_config_t unit_config = {
        .low_limit  = config->low_limit,
        .high_limit = config->high_limit,
    };

    err = pcnt_new_unit(&unit_config, &ctx->unit);
    if (err != ESP_OK)
    {
        shaft_encoder_cleanup(ctx);
        return err;
    }

    if (config->glitch_filter_ns > 0)
    {
        pcnt_glitch_filter_config_t filter_config = {
            .max_glitch_ns = config->glitch_filter_ns,
        };
        err = pcnt_unit_set_glitch_filter(ctx->unit, &filter_config);
        if (err != ESP_OK)
        {
            shaft_encoder_cleanup(ctx);
            return err;
        }
    }

    err = shaft_encoder_configure_channel(ctx, config);
    if (err != ESP_OK)
    {
        shaft_encoder_cleanup(ctx);
        return err;
    }

    err = pcnt_unit_enable(ctx->unit);
    if (err != ESP_OK)
    {
        shaft_encoder_cleanup(ctx);
        return err;
    }

    err = pcnt_unit_clear_count(ctx->unit);
    if (err != ESP_OK)
    {
        shaft_encoder_cleanup(ctx);
        return err;
    }

    err = pcnt_unit_start(ctx->unit);
    if (err != ESP_OK)
    {
        shaft_encoder_cleanup(ctx);
        return err;
    }

    *out_handle = ctx;
    return ESP_OK;
}

esp_err_t shaft_encoder_start(shaft_encoder_handle_t handle)
{
    shaft_encoder_ctx_t *ctx = handle;

    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    return pcnt_unit_start(ctx->unit);
}

esp_err_t shaft_encoder_stop(shaft_encoder_handle_t handle)
{
    shaft_encoder_ctx_t *ctx = handle;

    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    return pcnt_unit_stop(ctx->unit);
}

esp_err_t shaft_encoder_clear(shaft_encoder_handle_t handle)
{
    shaft_encoder_ctx_t *ctx = handle;

    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    return pcnt_unit_clear_count(ctx->unit);
}

esp_err_t shaft_encoder_get_count(shaft_encoder_handle_t handle, int32_t *count)
{
    shaft_encoder_ctx_t *ctx = handle;

    if (!handle || !count)
    {
        return ESP_ERR_INVALID_ARG;
    }

    return pcnt_unit_get_count(ctx->unit, (int *) count);
}

esp_err_t shaft_encoder_deinit(shaft_encoder_handle_t handle)
{
    shaft_encoder_ctx_t *ctx = handle;

    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    shaft_encoder_cleanup(ctx);
    free(ctx);
    return ESP_OK;
}

static esp_err_t shaft_encoder_configure_channel(shaft_encoder_ctx_t *ctx, const shaft_encoder_config_t *config)
{
    esp_err_t          err;
    pcnt_chan_config_t chan_config = {
        .edge_gpio_num  = config->count_gpio_num,
        .level_gpio_num = config->direction_gpio_num,
    };

    err = pcnt_new_channel(ctx->unit, &chan_config, &ctx->channel);
    if (err != ESP_OK)
    {
        return err;
    }

    err = pcnt_channel_set_edge_action(ctx->channel, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
    if (err != ESP_OK)
    {
        return err;
    }

    pcnt_channel_level_action_t high_action = PCNT_CHANNEL_LEVEL_ACTION_KEEP;
    pcnt_channel_level_action_t low_action  = PCNT_CHANNEL_LEVEL_ACTION_INVERSE;
    if (config->invert_direction)
    {
        high_action = PCNT_CHANNEL_LEVEL_ACTION_INVERSE;
        low_action  = PCNT_CHANNEL_LEVEL_ACTION_KEEP;
    }

    return pcnt_channel_set_level_action(ctx->channel, high_action, low_action);
}

static void shaft_encoder_cleanup(shaft_encoder_ctx_t *ctx)
{
    if (!ctx)
    {
        return;
    }

    if (ctx->unit)
    {
        pcnt_unit_stop(ctx->unit);
        pcnt_unit_disable(ctx->unit);
    }

    if (ctx->channel)
    {
        pcnt_del_channel(ctx->channel);
        ctx->channel = NULL;
    }

    if (ctx->unit)
    {
        pcnt_del_unit(ctx->unit);
        ctx->unit = NULL;
    }
}
