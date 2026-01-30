#include "console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "argtable3/argtable3.h"
#include "driver/uart.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hall_sensors.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "position.h"
#include "sdkconfig.h"
#include "shaft_encoder.h"
#include "slider.h"
#include "slider_state_machine.h"

static const char            *TAG = "console";
static drv8452_handle_t       s_drv_handle;
static shaft_encoder_handle_t s_encoder_handle;
static hall_sensors_handle_t  s_hall_handle;
static led_handle_t           s_led_handle;
static esp_console_repl_t    *s_repl;

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

typedef struct led_args
{
    struct arg_int *red;
    struct arg_int *green;
    struct arg_int *blue;
    struct arg_end *end;
} led_args_t;

typedef struct slider_event_args
{
    struct arg_str *event;
    struct arg_end *end;
} slider_event_args_t;

typedef struct sleep_args
{
    struct arg_int *state;
    struct arg_end *end;
} sleep_args_t;

static frequency_args_t    s_frequency_args;
static enable_args_t       s_enable_args;
static direction_args_t    s_direction_args;
static led_args_t          s_led_args;
static slider_event_args_t s_slider_event_args;
static sleep_args_t        s_sleep_args;

static esp_err_t register_enable_command(void);
static esp_err_t register_frequency_command(void);
static esp_err_t register_direction_command(void);
static esp_err_t register_register_read_command(void);
static esp_err_t register_register_write_command(void);
static esp_err_t register_fault_clear_command(void);
static esp_err_t register_encoder_count_command(void);
static esp_err_t register_hall_state_command(void);
static esp_err_t register_led_command(void);
static esp_err_t register_led_clear_command(void);
static esp_err_t register_position_set_command(void);
static esp_err_t register_position_get_command(void);
static esp_err_t register_slider_event_command(void);
static esp_err_t register_sleep_command(void);

static int     cmd_enable(int argc, char **argv);
static int     cmd_frequency(int argc, char **argv);
static int     cmd_direction(int argc, char **argv);
static int     cmd_register_read(int argc, char **argv);
static int     cmd_register_write(int argc, char **argv);
static int     cmd_fault_clear(int argc, char **argv);
static int     cmd_encoder_count(int argc, char **argv);
static int     cmd_hall_state(int argc, char **argv);
static int     cmd_led(int argc, char **argv);
static int     cmd_led_clear(int argc, char **argv);
static int     cmd_position_set(int argc, char **argv);
static int     cmd_position_get(int argc, char **argv);
static int     cmd_slider_event(int argc, char **argv);
static int     cmd_sleep(int argc, char **argv);
static void    tcp_console_start(void);
static void    tcp_console_task(void *arg);
static void    tcp_console_install_stdout(void);
static int     tcp_console_open(const char *path, int flags, int mode);
static int     tcp_console_close(int fd);
static ssize_t tcp_console_write(int fd, const void *data, size_t size);
static int     tcp_console_fstat(int fd, struct stat *st);

static int               s_tcp_client_fd = -1;
static SemaphoreHandle_t s_tcp_client_lock;

esp_err_t console_start(drv8452_handle_t drv_handle, shaft_encoder_handle_t encoder_handle,
                        hall_sensors_handle_t hall_handle, led_handle_t led_handle)
{
    esp_err_t err;

    if (drv_handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    s_drv_handle     = drv_handle;
    s_encoder_handle = encoder_handle;
    s_hall_handle    = hall_handle;
    s_led_handle     = led_handle;

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

    err = register_led_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register led command (%s)", esp_err_to_name(err));
        return err;
    }

    err = register_led_clear_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register led_clear command (%s)", esp_err_to_name(err));
        return err;
    }

    err = register_position_set_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register position_set command (%s)", esp_err_to_name(err));
        return err;
    }

    err = register_position_get_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register position_get command (%s)", esp_err_to_name(err));
        return err;
    }

    err = register_slider_event_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register slider_event command (%s)", esp_err_to_name(err));
        return err;
    }

    err = register_sleep_command();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register sleep command (%s)", esp_err_to_name(err));
        return err;
    }

    err = esp_console_start_repl(s_repl);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start console REPL (%s)", esp_err_to_name(err));
        return err;
    }

    tcp_console_install_stdout();
    tcp_console_start();

    ESP_LOGI(TAG, "Console ready. Type 'help' to list commands.");
    return ESP_OK;
}

static void tcp_console_start(void)
{
    const uint32_t stack_size = 4096;
    s_tcp_client_lock         = xSemaphoreCreateMutex();
    if (xTaskCreate(tcp_console_task, "tcp_console", stack_size, NULL, 4, NULL) != pdPASS)
    {
        ESP_LOGW(TAG, "Failed to start TCP console task");
    }
}

static void tcp_console_install_stdout(void)
{
    static const esp_vfs_t vfs = {
        .flags = ESP_VFS_FLAG_DEFAULT,
        .write = tcp_console_write,
        .open  = tcp_console_open,
        .close = tcp_console_close,
        .fstat = tcp_console_fstat,
    };

    if (esp_vfs_register("/dev/tcpcon", &vfs, NULL) != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to register TCP console VFS");
        return;
    }

    freopen("/dev/tcpcon", "w", stdout);
    freopen("/dev/tcpcon", "w", stderr);
    setvbuf(stdout, NULL, _IOLBF, 0);
}

static int tcp_console_open(const char *path, int flags, int mode)
{
    (void) path;
    (void) flags;
    (void) mode;
    return 0;
}

static int tcp_console_close(int fd)
{
    (void) fd;
    return 0;
}

static ssize_t tcp_console_write(int fd, const void *data, size_t size)
{
    (void) fd;

    if (data && size > 0)
    {
        uart_write_bytes(CONFIG_ESP_CONSOLE_UART_NUM, data, size);
    }

    if (data && size > 0 && s_tcp_client_lock && xSemaphoreTake(s_tcp_client_lock, 0) == pdTRUE)
    {
        int sock = s_tcp_client_fd;
        xSemaphoreGive(s_tcp_client_lock);
        if (sock >= 0)
        {
            send(sock, data, size, 0);
        }
    }

    return size;
}

static int tcp_console_fstat(int fd, struct stat *st)
{
    (void) fd;
    if (!st)
    {
        return -1;
    }
    st->st_mode = S_IFCHR;
    return 0;
}

static void tcp_console_task(void *arg)
{
    (void) arg;

    const int port        = 23;
    int       listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0)
    {
        ESP_LOGE(TAG, "TCP socket create failed");
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(listen_sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        ESP_LOGE(TAG, "TCP bind failed");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_sock, 1) < 0)
    {
        ESP_LOGE(TAG, "TCP listen failed");
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TCP console listening on port %d", port);

    while (true)
    {
        struct sockaddr_in6 source_addr;
        socklen_t           addr_len = sizeof(source_addr);
        int                 sock     = accept(listen_sock, (struct sockaddr *) &source_addr, &addr_len);
        if (sock < 0)
        {
            ESP_LOGW(TAG, "TCP accept failed");
            continue;
        }

        if (s_tcp_client_lock && xSemaphoreTake(s_tcp_client_lock, portMAX_DELAY) == pdTRUE)
        {
            s_tcp_client_fd = sock;
            xSemaphoreGive(s_tcp_client_lock);
        }

        const char *banner = "Connected to tv_slider console\r\nType 'help' for commands.\r\n";
        send(sock, banner, strlen(banner), 0);

        char line[128];
        int  len = 0;
        send(sock, "tv_slider> ", 11, 0);

        while (true)
        {
            unsigned char ch = 0;
            int           r  = recv(sock, &ch, 1, 0);
            if (r <= 0)
            {
                break;
            }

            if (ch == 0xFF)
            {
                unsigned char ignore[2];
                recv(sock, ignore, sizeof(ignore), 0);
                continue;
            }

            if (ch == '\r' || ch == '\n')
            {
                line[len] = '\0';
                if (len > 0)
                {
                    int       cmd_ret = 0;
                    esp_err_t err     = esp_console_run(line, &cmd_ret);
                    if (err == ESP_OK && cmd_ret == ESP_OK)
                    {
                        send(sock, "OK\r\n", 4, 0);
                    }
                    else
                    {
                        send(sock, "ERR\r\n", 5, 0);
                    }
                    len = 0;
                }
                send(sock, "tv_slider> ", 11, 0);
                continue;
            }

            if (ch == '\b' || ch == 0x7F)
            {
                if (len > 0)
                {
                    len--;
                }
                continue;
            }

            if (len < (int) (sizeof(line) - 1))
            {
                line[len++] = (char) ch;
            }
        }

        shutdown(sock, 0);
        close(sock);
        if (s_tcp_client_lock && xSemaphoreTake(s_tcp_client_lock, portMAX_DELAY) == pdTRUE)
        {
            if (s_tcp_client_fd == sock)
            {
                s_tcp_client_fd = -1;
            }
            xSemaphoreGive(s_tcp_client_lock);
        }
    }
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

static esp_err_t register_sleep_command(void)
{
    s_sleep_args.state = arg_int1(NULL, NULL, "<0|1>", "0 disables, 1 enables sleep mode");
    s_sleep_args.end   = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command  = "sleep",
        .help     = "Enable or disable DRV8452 sleep mode",
        .hint     = NULL,
        .func     = &cmd_sleep,
        .argtable = &s_sleep_args,
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

static esp_err_t register_led_command(void)
{
    s_led_args.red   = arg_int1(NULL, NULL, "<red>", "Red value 0-255");
    s_led_args.green = arg_int1(NULL, NULL, "<green>", "Green value 0-255");
    s_led_args.blue  = arg_int1(NULL, NULL, "<blue>", "Blue value 0-255");
    s_led_args.end   = arg_end(4);

    const esp_console_cmd_t cmd = {
        .command  = "led",
        .help     = "Set LED color: led <red> <green> <blue>",
        .hint     = NULL,
        .func     = &cmd_led,
        .argtable = &s_led_args,
    };

    return esp_console_cmd_register(&cmd);
}

static esp_err_t register_led_clear_command(void)
{
    const esp_console_cmd_t cmd = {
        .command  = "led_clear",
        .help     = "Clear the LED strip",
        .hint     = NULL,
        .func     = &cmd_led_clear,
        .argtable = NULL,
    };

    return esp_console_cmd_register(&cmd);
}

static esp_err_t register_position_set_command(void)
{
    const esp_console_cmd_t cmd = {
        .command  = "position_set",
        .help     = "Set the reported position for UI testing",
        .hint     = NULL,
        .func     = &cmd_position_set,
        .argtable = NULL,
    };

    return esp_console_cmd_register(&cmd);
}

static esp_err_t register_position_get_command(void)
{
    const esp_console_cmd_t cmd = {
        .command  = "position_get",
        .help     = "Read the reported position value",
        .hint     = NULL,
        .func     = &cmd_position_get,
        .argtable = NULL,
    };

    return esp_console_cmd_register(&cmd);
}

static esp_err_t register_slider_event_command(void)
{
    s_slider_event_args.event = arg_str1(NULL, NULL, "<event>",
                                         "Event name: cmd_move_in|cmd_move_out|cmd_stop|cmd_clear_fault|motor_fault|"
                                         "sensor_in_slow|sensor_in_stop|sensor_out_slow|sensor_out_stop|timer_expired");
    s_slider_event_args.end   = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command  = "slider_event",
        .help     = "Post event to slider state machine",
        .hint     = NULL,
        .func     = &cmd_slider_event,
        .argtable = &s_slider_event_args,
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

static int cmd_sleep(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &s_sleep_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, s_sleep_args.end, argv[0]);
        return 1;
    }

    int state = s_sleep_args.state->ival[0];
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

    esp_err_t err = drv8452_sleep(s_drv_handle, state == 1);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set sleep mode (%s)", esp_err_to_name(err));
        return 1;
    }

    printf("Sleep mode %s\n", state ? "enabled" : "disabled");
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

static int cmd_led(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &s_led_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, s_led_args.end, argv[0]);
        return 1;
    }

    if (s_led_handle == NULL)
    {
        ESP_LOGE(TAG, "LED handle is not ready");
        return 1;
    }

    int red   = s_led_args.red->ival[0];
    int green = s_led_args.green->ival[0];
    int blue  = s_led_args.blue->ival[0];

    if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255)
    {
        printf("RGB values must be 0-255\n");
        return 1;
    }

    esp_err_t err = led_set_pixel(s_led_handle, (uint8_t) red, (uint8_t) green, (uint8_t) blue);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set LED pixel (%s)", esp_err_to_name(err));
        return 1;
    }

    printf("LED set to R=%d G=%d B=%d\n", red, green, blue);
    return 0;
}

static int cmd_led_clear(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    if (s_led_handle == NULL)
    {
        ESP_LOGE(TAG, "LED handle is not ready");
        return 1;
    }

    esp_err_t err = led_clear(s_led_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to clear LED (%s)", esp_err_to_name(err));
        return 1;
    }

    printf("LED cleared\n");
    return 0;
}

static int cmd_position_set(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: position_set <-1..100>\n");
        return 1;
    }

    const char *input    = argv[1];
    char       *endptr   = NULL;
    long        position = strtol(input, &endptr, 10);
    if (endptr == input || *endptr != '\0')
    {
        printf("Position must be -1 or 0-100\n");
        return 1;
    }

    if (position < -1 || position > 100)
    {
        printf("Position must be -1 or 0-100\n");
        return 1;
    }

    position_set((int32_t) position);
    printf("Position set to %ld\n", position);
    return 0;
}

static int cmd_position_get(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    int32_t position = position_get();
    printf("Position: %ld\n", (long) position);
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

    int32_t   count = 0;
    esp_err_t err   = shaft_encoder_get_count(s_encoder_handle, &count);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read encoder count (%s)", esp_err_to_name(err));
        return 1;
    }

    printf("Encoder count: %ld\n", count);
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

    uint8_t   state = 0;
    esp_err_t err   = hall_sensors_get_state(s_hall_handle, &state);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read hall state (%s)", esp_err_to_name(err));
        return 1;
    }

    printf("Hall state mask: 0x%02X (IN_STOP=%u IN_SLOW=%u OUT_SLOW=%u OUT_STOP=%u)\n", state,
           (state >> HALL_SENSOR_IN_STOP) & 1u, (state >> HALL_SENSOR_IN_SLOW) & 1u,
           (state >> HALL_SENSOR_OUT_SLOW) & 1u, (state >> HALL_SENSOR_OUT_STOP) & 1u);
    return 0;
}

static int cmd_slider_event(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &s_slider_event_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, s_slider_event_args.end, argv[0]);
        return 1;
    }

    const char                  *event_str = s_slider_event_args.event->sval[0];
    slider_state_machine_EventId event;

    if (strcmp(event_str, "cmd_move_in") == 0)
    {
        event = slider_state_machine_EventId_CMD_MOVE_IN;
    }
    else if (strcmp(event_str, "cmd_move_out") == 0)
    {
        event = slider_state_machine_EventId_CMD_MOVE_OUT;
    }
    else if (strcmp(event_str, "cmd_stop") == 0)
    {
        event = slider_state_machine_EventId_CMD_STOP;
    }
    else if (strcmp(event_str, "cmd_clear_fault") == 0)
    {
        event = slider_state_machine_EventId_CMD_CLEAR_FAULT;
    }
    else if (strcmp(event_str, "motor_fault") == 0)
    {
        event = slider_state_machine_EventId_MOTOR_FAULT;
    }
    else if (strcmp(event_str, "sensor_in_slow") == 0)
    {
        event = slider_state_machine_EventId_SENSOR_IN_SLOW;
    }
    else if (strcmp(event_str, "sensor_in_stop") == 0)
    {
        event = slider_state_machine_EventId_SENSOR_IN_STOP;
    }
    else if (strcmp(event_str, "sensor_out_slow") == 0)
    {
        event = slider_state_machine_EventId_SENSOR_OUT_SLOW;
    }
    else if (strcmp(event_str, "sensor_out_stop") == 0)
    {
        event = slider_state_machine_EventId_SENSOR_OUT_STOP;
    }
    else if (strcmp(event_str, "timer_expired") == 0)
    {
        event = slider_state_machine_EventId_TIMER_EXPIRED;
    }
    else
    {
        printf("Unknown event: %s\n", event_str);
        return 1;
    }

    if (slider_post_event(event))
    {
        printf("Event '%s' posted successfully\n", event_str);
        return 0;
    }
    else
    {
        printf("Failed to post event '%s'\n", event_str);
        return 1;
    }
}