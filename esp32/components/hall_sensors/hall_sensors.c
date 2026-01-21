#include "hall_sensors.h"

#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"

typedef struct hall_sensor_isr_arg_t hall_sensor_isr_arg_t;

typedef struct hall_sensors_t
{
    int                    gpio_nums[4];
    bool                   isr_registered[4];
    hall_sensor_isr_arg_t *isr_args[4];
    hall_sensor_callback_t callback;
    void                  *user_ctx;
    bool                   active_low;
} hall_sensors_ctx_t;

struct hall_sensor_isr_arg_t
{
    hall_sensors_ctx_t *ctx;
    hall_sensor_t       sensor;
};

static const char *TAG = "hall_sensors";

static esp_err_t hall_sensors_config_gpio(const hall_sensors_config_t *config, hall_sensors_ctx_t *ctx,
                                          hall_sensor_t sensor, int gpio_num);
static esp_err_t hall_sensors_install_isr_service(gpio_num_t gpio_num, gpio_isr_t isr_handler,
                                                  hall_sensor_isr_arg_t *arg);
static void      hall_sensors_remove_isrs(hall_sensors_ctx_t *ctx);
static void      hall_sensor_isr(void *arg);

esp_err_t hall_sensors_init(const hall_sensors_config_t *config, hall_sensors_handle_t *out_handle)
{
    hall_sensors_ctx_t *ctx;
    esp_err_t           err;

    if (!config || !out_handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    *out_handle = NULL;
    ctx         = calloc(1, sizeof(hall_sensors_ctx_t));
    if (!ctx)
    {
        return ESP_ERR_NO_MEM;
    }

    ctx->callback   = config->callback;
    ctx->user_ctx   = config->user_ctx;
    ctx->active_low = config->active_low;
    for (int i = 0; i < 4; ++i)
    {
        ctx->gpio_nums[i] = -1;
    }

    err = hall_sensors_config_gpio(config, ctx, HALL_SENSOR_IN_STOP, config->in_stop_gpio_num);
    if (err != ESP_OK)
    {
        hall_sensors_remove_isrs(ctx);
        free(ctx);
        return err;
    }

    err = hall_sensors_config_gpio(config, ctx, HALL_SENSOR_IN_SLOW, config->in_slow_gpio_num);
    if (err != ESP_OK)
    {
        hall_sensors_remove_isrs(ctx);
        free(ctx);
        return err;
    }

    err = hall_sensors_config_gpio(config, ctx, HALL_SENSOR_OUT_SLOW, config->out_slow_gpio_num);
    if (err != ESP_OK)
    {
        hall_sensors_remove_isrs(ctx);
        free(ctx);
        return err;
    }

    err = hall_sensors_config_gpio(config, ctx, HALL_SENSOR_OUT_STOP, config->out_stop_gpio_num);
    if (err != ESP_OK)
    {
        hall_sensors_remove_isrs(ctx);
        free(ctx);
        return err;
    }

    *out_handle = ctx;
    return ESP_OK;
}

esp_err_t hall_sensors_deinit(hall_sensors_handle_t handle)
{
    hall_sensors_ctx_t *ctx = handle;

    if (!handle)
    {
        return ESP_ERR_INVALID_ARG;
    }

    hall_sensors_remove_isrs(ctx);
    free(ctx);
    return ESP_OK;
}

esp_err_t hall_sensors_set_invert(hall_sensors_handle_t handle, bool invert)
{
    hall_sensors_ctx_t *ctx = handle;
    if (!ctx)
    {
        return ESP_ERR_INVALID_ARG;
    }
    ctx->active_low = invert;
    return ESP_OK;
}

esp_err_t hall_sensors_get_state(hall_sensors_handle_t handle, uint8_t *state_mask)
{
    hall_sensors_ctx_t *ctx  = handle;
    uint8_t             mask = 0;

    if (!handle || !state_mask)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < 4; ++i)
    {
        if (ctx->gpio_nums[i] < 0)
        {
            continue;
        }
        int  level  = gpio_get_level(ctx->gpio_nums[i]);
        bool active = ctx->active_low ? (level == 0) : (level != 0);
        if (active)
        {
            mask |= (uint8_t) (1u << i);
        }
    }

    *state_mask = mask;
    return ESP_OK;
}

static esp_err_t hall_sensors_config_gpio(const hall_sensors_config_t *config, hall_sensors_ctx_t *ctx,
                                          hall_sensor_t sensor, int gpio_num)
{
    if (gpio_num < 0)
    {
        return ESP_OK;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode         = GPIO_MODE_INPUT,
        .pull_down_en = config->pull_down_enable ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = config->pull_up_enable ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .intr_type =
            config->callback ? (config->active_low ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE) : GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init GPIO %d (%d)", gpio_num, err);
        return err;
    }

    ctx->gpio_nums[sensor]      = gpio_num;
    ctx->isr_registered[sensor] = false;

    if (config->callback)
    {
        hall_sensor_isr_arg_t *isr_arg = calloc(1, sizeof(*isr_arg));
        if (!isr_arg)
        {
            return ESP_ERR_NO_MEM;
        }
        isr_arg->ctx    = ctx;
        isr_arg->sensor = sensor;

        err = hall_sensors_install_isr_service(gpio_num, hall_sensor_isr, isr_arg);
        if (err != ESP_OK)
        {
            free(isr_arg);
            ESP_LOGE(TAG, "Failed to add ISR handler for GPIO %d (%d)", gpio_num, err);
            return err;
        }

        ctx->isr_registered[sensor] = true;
        ctx->isr_args[sensor]       = isr_arg;
    }

    return ESP_OK;
}

static esp_err_t hall_sensors_install_isr_service(gpio_num_t gpio_num, gpio_isr_t isr_handler,
                                                  hall_sensor_isr_arg_t *arg)
{
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err == ESP_ERR_INVALID_STATE)
    {
        err = ESP_OK;
    }
    if (err != ESP_OK)
    {
        return err;
    }

    return gpio_isr_handler_add(gpio_num, isr_handler, arg);
}

static void hall_sensors_remove_isrs(hall_sensors_ctx_t *ctx)
{
    if (!ctx)
    {
        return;
    }

    for (int i = 0; i < 4; ++i)
    {
        if (ctx->isr_registered[i] && ctx->gpio_nums[i] >= 0)
        {
            gpio_isr_handler_remove(ctx->gpio_nums[i]);
            ctx->isr_registered[i] = false;
        }
        if (ctx->isr_args[i])
        {
            free(ctx->isr_args[i]);
            ctx->isr_args[i] = NULL;
        }
    }
}

static void IRAM_ATTR hall_sensor_isr(void *arg)
{
    hall_sensor_isr_arg_t *isr_arg = (hall_sensor_isr_arg_t *) arg;
    if (!isr_arg || !isr_arg->ctx || !isr_arg->ctx->callback)
    {
        return;
    }
    isr_arg->ctx->callback(isr_arg->sensor, isr_arg->ctx->user_ctx);
}
