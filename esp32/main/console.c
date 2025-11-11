#include "console.h"

#include <stdio.h>

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"

static const char         *TAG = "console";
static drv8452_handle_t    s_drv_handle;
static esp_console_repl_t *s_repl;

typedef struct frequency_args
{
    struct arg_int *value;
    struct arg_end *end;
} frequency_args_t;

typedef struct enable_args
{
    struct arg_int *state;
    struct arg_end *end;
} enable_args_t;

typedef struct direction_args
{
    struct arg_int *value;
    struct arg_end *end;
} direction_args_t;

static frequency_args_t s_frequency_args;
static enable_args_t    s_enable_args;
static direction_args_t s_direction_args;

static int       cmd_frequency(int argc, char **argv);
static int       cmd_enable(int argc, char **argv);
static int       cmd_direction(int argc, char **argv);
static esp_err_t register_frequency_command(void);
static esp_err_t register_enable_command(void);
static esp_err_t register_direction_command(void);

esp_err_t console_start(drv8452_handle_t drv_handle)
{
    esp_err_t err;

    if (drv_handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    s_drv_handle = drv_handle;

    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt                    = "tv_slider>";
    repl_config.max_cmdline_length        = 64;

    esp_console_dev_uart_config_t repl_uart = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();

    err = esp_console_new_repl_uart(&repl_uart, &repl_config, &s_repl);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create UART REPL (%s)", esp_err_to_name(err));
        return err;
    }

    esp_console_register_help_command();

    err = register_frequency_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register frequency command (%s)", esp_err_to_name(err));
        return err;
    }

    err = register_enable_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register enable command (%s)", esp_err_to_name(err));
        return err;
    }

    err = register_direction_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register direction command (%s)", esp_err_to_name(err));
        return err;
    }

    err = esp_console_start_repl(s_repl);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start console REPL (%s)", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Console ready. Type 'help' to list commands.");
    return ESP_OK;
}

static esp_err_t register_frequency_command(void)
{
    s_frequency_args.value = arg_int1(NULL, NULL, "<hz>", "Step frequency in Hz (0 stops the motor)");
    s_frequency_args.end   = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command  = "frequency",
        .help     = "Set the DRV8452 step frequency",
        .hint     = NULL,
        .func     = &cmd_frequency,
        .argtable = &s_frequency_args,
    };

    return esp_console_cmd_register(&cmd);
}

static esp_err_t register_enable_command(void)
{
    s_enable_args.state = arg_int1(NULL, NULL, "<0|1>", "0 disables, 1 enables driver outputs");
    s_enable_args.end   = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command  = "enable",
        .help     = "Enable or disable DRV8452 outputs",
        .hint     = NULL,
        .func     = &cmd_enable,
        .argtable = &s_enable_args,
    };

    return esp_console_cmd_register(&cmd);
}

static esp_err_t register_direction_command(void)
{
    s_direction_args.value = arg_int1(NULL, NULL, "<0|1>", "1 sets forward, 0 sets reverse direction");
    s_direction_args.end   = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command  = "direction",
        .help     = "Set DRV8452 motor direction",
        .hint     = NULL,
        .func     = &cmd_direction,
        .argtable = &s_direction_args,
    };

    return esp_console_cmd_register(&cmd);
}

static int cmd_frequency(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &s_frequency_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, s_frequency_args.end, argv[0]);
        return 1;
    }

    int hz = s_frequency_args.value->ival[0];
    if (hz < 0)
    {
        printf("Frequency must be >= 0 Hz\n");
        return 1;
    }

    if (s_drv_handle == NULL)
    {
        ESP_LOGE(TAG, "DRV8452 handle is not ready");
        return 1;
    }

    esp_err_t err = drv8452_step_frequency(s_drv_handle, (uint32_t) hz);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set frequency (%s)", esp_err_to_name(err));
        return 1;
    }

    printf("Step frequency set to %d Hz\n", hz);
    return 0;
}

static int cmd_enable(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &s_enable_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, s_enable_args.end, argv[0]);
        return 1;
    }

    int state = s_enable_args.state->ival[0];
    if (state != 0 && state != 1)
    {
        printf("State must be 0 (disable) or 1 (enable)\n");
        return 1;
    }

    if (s_drv_handle == NULL)
    {
        ESP_LOGE(TAG, "DRV8452 handle is not ready");
        return 1;
    }

    if (state == 1)
    {
        ESP_ERROR_CHECK(drv8452_register_write(s_drv_handle, DRV8452_REG_CTRL1, CTRL1_EN_OUT | CTRL1_CLR_FLT));
        printf("Driver outputs enabled\n");
    }
    else
    {
        ESP_ERROR_CHECK(drv8452_register_write(s_drv_handle, DRV8452_REG_CTRL1, 0));
        printf("Driver outputs disabled\n");
    }

    return 0;
}

static int cmd_direction(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &s_direction_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, s_direction_args.end, argv[0]);
        return 1;
    }

    int dir = s_direction_args.value->ival[0];
    if (dir != 0 && dir != 1)
    {
        printf("Direction must be 0 (reverse) or 1 (forward)\n");
        return 1;
    }

    if (s_drv_handle == NULL)
    {
        ESP_LOGE(TAG, "DRV8452 handle is not ready");
        return 1;
    }

    ESP_ERROR_CHECK(drv8452_direction(s_drv_handle, dir == 1));
    printf("Direction set to %s\n", dir ? "forward" : "reverse");
    return 0;
}
