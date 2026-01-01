#include "console.h"

#include <stdio.h>
#include <stdlib.h>

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"
#include "shaft_encoder.h"
#include "hall_sensors.h"

static const char         *TAG = "console";
static drv8452_handle_t    s_drv_handle;
static shaft_encoder_handle_t s_encoder_handle;
static hall_sensors_handle_t  s_hall_handle;
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

static esp_err_t register_enable_command(void);
static esp_err_t register_frequency_command(void);
static esp_err_t register_direction_command(void);
static esp_err_t register_register_read_command(void);
static esp_err_t register_register_write_command(void);
static esp_err_t register_fault_clear_command(void);
static esp_err_t register_encoder_count_command(void);
static esp_err_t register_hall_state_command(void);

static int cmd_enable(int argc, char **argv);
static int cmd_frequency(int argc, char **argv);
static int cmd_direction(int argc, char **argv);
static int cmd_register_read(int argc, char **argv);
static int cmd_register_write(int argc, char **argv);
static int cmd_fault_clear(int argc, char **argv);
static int cmd_encoder_count(int argc, char **argv);
static int cmd_hall_state(int argc, char **argv);

esp_err_t console_start(drv8452_handle_t drv_handle,
                        shaft_encoder_handle_t encoder_handle,
                        hall_sensors_handle_t hall_handle)
{
    esp_err_t err;

    if (drv_handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    s_drv_handle = drv_handle;
    s_encoder_handle = encoder_handle;
    s_hall_handle = hall_handle;

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

    err = register_enable_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register enable command (%s)", esp_err_to_name(err));
        return err;
    }

    err = register_frequency_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register frequency command (%s)", esp_err_to_name(err));
        return err;
    }

    err = register_direction_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register direction command (%s)", esp_err_to_name(err));
        return err;
    }

    err = register_register_read_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register register_read command (%s)", esp_err_to_name(err));
        return err;
    }

    err = register_register_write_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register register_write command (%s)", esp_err_to_name(err));
        return err;
    }

    err = register_fault_clear_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register fault_clear command (%s)", esp_err_to_name(err));
        return err;
    }

    err = register_encoder_count_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register encoder_count command (%s)", esp_err_to_name(err));
        return err;
    }

    err = register_hall_state_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register hall_state command (%s)", esp_err_to_name(err));
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

static esp_err_t register_hall_state_command(void)
{
    const esp_console_cmd_t cmd = {
        .command  = "hall_state",
        .help     = "Read hall sensor state bitmask",
        .hint     = NULL,
        .func     = &cmd_hall_state,
        .argtable = NULL,
    };

    return esp_console_cmd_register(&cmd);
}

static esp_err_t register_encoder_count_command(void)
{
    const esp_console_cmd_t cmd = {
        .command  = "encoder_count",
        .help     = "Read shaft encoder count",
        .hint     = NULL,
        .func     = &cmd_encoder_count,
        .argtable = NULL,
    };

    return esp_console_cmd_register(&cmd);
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

static esp_err_t register_fault_clear_command(void)
{
    const esp_console_cmd_t cmd = {
        .command  = "fault_clear",
        .help     = "Clear DRV8452 latched fault (CTRL3.CLR_FLT)",
        .hint     = NULL,
        .func     = &cmd_fault_clear,
        .argtable = NULL,
    };

    return esp_console_cmd_register(&cmd);
}

static esp_err_t register_register_read_command(void)
{
    const esp_console_cmd_t cmd = {
        .command  = "register_read",
        .help     = "Read DRV8452 register: register_read <addr>",
        .hint     = NULL,
        .func     = &cmd_register_read,
        .argtable = NULL,
    };
    return esp_console_cmd_register(&cmd);
}

static esp_err_t register_register_write_command(void)
{
    const esp_console_cmd_t cmd = {
        .command  = "register_write",
        .help     = "Write DRV8452 register: register_write <addr> <value>",
        .hint     = NULL,
        .func     = &cmd_register_write,
        .argtable = NULL,
    };
    return esp_console_cmd_register(&cmd);
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
        ESP_ERROR_CHECK(drv8452_register_write(s_drv_handle, DRV8452_REG_CTRL1, DRV8452_CTRL1_EN_OUT_ENABLED));
        printf("Driver outputs enabled\n");
    }
    else
    {
        ESP_ERROR_CHECK(drv8452_register_write(s_drv_handle, DRV8452_REG_CTRL1, DRV8452_CTRL1_EN_OUT_DISABLED));
        printf("Driver outputs disabled\n");
    }

    return 0;
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

static int cmd_register_read(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: register_read <addr>\n");
        return 1;
    }
    if (s_drv_handle == NULL)
    {
        ESP_LOGE(TAG, "DRV8452 handle is not ready");
        return 1;
    }

    char         *endptr = NULL;
    unsigned long addr   = strtoul(argv[1], &endptr, 0);
    if (endptr == argv[1] || *endptr != '\0' || addr > 0x3F)
    {
        printf("Invalid address. Use 0..0x3F.\n");
        return 1;
    }

    uint8_t   val = 0;
    esp_err_t err = drv8452_register_read(s_drv_handle, (uint8_t) addr, &val);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Read failed (%s)", esp_err_to_name(err));
        return 1;
    }
    printf("REG[0x%02lX] = 0x%02X\n", addr, val);
    return 0;
}

static int cmd_register_write(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("Usage: register_write <addr> <value>\n");
        return 1;
    }
    if (s_drv_handle == NULL)
    {
        ESP_LOGE(TAG, "DRV8452 handle is not ready");
        return 1;
    }

    char         *endptr = NULL;
    unsigned long addr   = strtoul(argv[1], &endptr, 0);
    if (endptr == argv[1] || *endptr != '\0' || addr > 0x3F)
    {
        printf("Invalid address. Use 0..0x3F.\n");
        return 1;
    }

    endptr            = NULL;
    unsigned long val = strtoul(argv[2], &endptr, 0);
    if (endptr == argv[2] || *endptr != '\0' || val > 0xFF)
    {
        printf("Invalid value. Use 0..0xFF.\n");
        return 1;
    }

    esp_err_t err = drv8452_register_write(s_drv_handle, (uint8_t) addr, (uint8_t) val);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Write failed (%s)", esp_err_to_name(err));
        return 1;
    }
    printf("REG[0x%02lX] <= 0x%02lX\n", addr, val);
    return 0;
}

static int cmd_fault_clear(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    if (s_drv_handle == NULL)
    {
        ESP_LOGE(TAG, "DRV8452 handle is not ready");
        return 1;
    }

    esp_err_t err;
    uint8_t   fault_before = 0;
    uint8_t   fault_after  = 0;
    uint8_t   ctrl3        = 0;

    if (drv8452_register_read(s_drv_handle, DRV8452_REG_FAULT, &fault_before) == ESP_OK)
    {
        printf("FAULT before: 0x%02X\n", fault_before);
    }

    // Read CTRL3, set CLR_FLT high, then return it low while preserving other bits
    err = drv8452_register_read(s_drv_handle, DRV8452_REG_CTRL3, &ctrl3);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read CTRL3 (%s)", esp_err_to_name(err));
        return 1;
    }

    uint8_t ctrl3_set = (uint8_t) (ctrl3 | DRV8452_CTRL3_CLR_FLT_MASK);
    err               = drv8452_register_write(s_drv_handle, DRV8452_REG_CTRL3, ctrl3_set);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set CLR_FLT (%s)", esp_err_to_name(err));
        return 1;
    }

    uint8_t ctrl3_clr = (uint8_t) (ctrl3 & (uint8_t) (~DRV8452_CTRL3_CLR_FLT_MASK));
    err               = drv8452_register_write(s_drv_handle, DRV8452_REG_CTRL3, ctrl3_clr);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to clear CLR_FLT (%s)", esp_err_to_name(err));
        return 1;
    }

    if (drv8452_register_read(s_drv_handle, DRV8452_REG_FAULT, &fault_after) == ESP_OK)
    {
        printf("FAULT after:  0x%02X\n", fault_after);
    }

    printf("Fault cleared.\n");
    return 0;
}

static int cmd_encoder_count(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    if (s_encoder_handle == NULL)
    {
        ESP_LOGE(TAG, "Encoder handle is not ready");
        return 1;
    }

    int count = 0;
    esp_err_t err = shaft_encoder_get_count(s_encoder_handle, &count);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read encoder count (%s)", esp_err_to_name(err));
        return 1;
    }

    printf("Encoder count: %d\n", count);
    return 0;
}

static int cmd_hall_state(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    if (s_hall_handle == NULL)
    {
        ESP_LOGE(TAG, "Hall sensors handle is not ready");
        return 1;
    }

    uint8_t state = 0;
    esp_err_t err = hall_sensors_get_state(s_hall_handle, &state);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read hall state (%s)", esp_err_to_name(err));
        return 1;
    }

    printf("Hall state mask: 0x%02X (IN_STOP=%u IN_SLOW=%u OUT_SLOW=%u OUT_STOP=%u)\n",
           state,
           (state >> HALL_SENSOR_IN_STOP) & 1u,
           (state >> HALL_SENSOR_IN_SLOW) & 1u,
           (state >> HALL_SENSOR_OUT_SLOW) & 1u,
           (state >> HALL_SENSOR_OUT_STOP) & 1u);
    return 0;
}
