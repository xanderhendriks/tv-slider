#include "shaft_encoder.h"

#include <stdlib.h>

#include "driver/pulse_cnt.h"
#include "esp_attr.h"

typedef struct shaft_encoder_t
{
    pcnt_unit_handle_t    unit;
    pcnt_channel_handle_t channel;
    int32_t               low_limit;
    int32_t               high_limit;
    volatile int32_t      overflow_accumulator;
} shaft_encoder_ctx_t;

static esp_err_t shaft_encoder_configure_channel(shaft_encoder_ctx_t *ctx, const shaft_encoder_config_t *config);
static void      shaft_encoder_cleanup(shaft_encoder_ctx_t *ctx);
static bool      shaft_encoder_on_reach(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx);

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

    ctx->low_limit  = config->low_limit;
    ctx->high_limit = config->high_limit;

    pcnt_event_callbacks_t cbs = {
        .on_reach = shaft_encoder_on_reach,
    };
    err = pcnt_unit_register_event_callbacks(ctx->unit, &cbs, ctx);
    if (err != ESP_OK)
    {
        shaft_encoder_cleanup(ctx);
        return err;
    }

    err = pcnt_unit_add_watch_point(ctx->unit, config->low_limit);
    if (err != ESP_OK)
    {
        shaft_encoder_cleanup(ctx);
        return err;
    }

    err = pcnt_unit_add_watch_point(ctx->unit, config->high_limit);
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

    ctx->overflow_accumulator = 0;
    return pcnt_unit_clear_count(ctx->unit);
}

esp_err_t shaft_encoder_set_count(shaft_encoder_handle_t handle, int32_t count)
{
    shaft_encoder_ctx_t *ctx = handle;

    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = pcnt_unit_clear_count(ctx->unit);
    if (err != ESP_OK)
    {
        return err;
    }
    ctx->overflow_accumulator = count;
    return ESP_OK;
}

esp_err_t shaft_encoder_get_count(shaft_encoder_handle_t handle, int32_t *count)
{
    shaft_encoder_ctx_t *ctx = handle;

    if (!handle || !count)
    {
        return ESP_ERR_INVALID_ARG;
    }

    int       hw_count;
    esp_err_t err = pcnt_unit_get_count(ctx->unit, &hw_count);
    if (err != ESP_OK)
    {
        return err;
    }

    *count = (int32_t) hw_count + ctx->overflow_accumulator;
    return ESP_OK;
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

static bool IRAM_ATTR shaft_encoder_on_reach(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata,
                                             void *user_ctx)
{
    shaft_encoder_ctx_t *ctx = user_ctx;
    // When the hardware counter hits low_limit or high_limit it resets to 0.
    // Compensate by adding the limit value so the accumulated total stays continuous.
    if (edata->watch_point_value == ctx->low_limit)
    {
        ctx->overflow_accumulator += ctx->low_limit;
    }
    else
    {
        ctx->overflow_accumulator += ctx->high_limit;
    }
    return false;  // no higher-priority task woken
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
