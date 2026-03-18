#include "log_buffer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static char           s_buf[LOG_BUF_SIZE];
static size_t         s_head         = 0; /* index of oldest byte */
static size_t         s_fill         = 0; /* number of valid bytes currently stored */
static portMUX_TYPE   s_mux          = portMUX_INITIALIZER_UNLOCKED;
static vprintf_like_t s_orig_vprintf = NULL;

static void write_byte(char c)
{
    if (s_fill < LOG_BUF_SIZE)
    {
        s_buf[(s_head + s_fill) % LOG_BUF_SIZE] = c;
        s_fill++;
    }
    else
    {
        /* Buffer full: overwrite oldest byte */
        s_buf[s_head] = c;
        s_head        = (s_head + 1) % LOG_BUF_SIZE;
    }
}

static int log_vprintf_hook(const char *fmt, va_list args)
{
    int ret = 0;

    /* Forward to original output (UART) first */
    if (s_orig_vprintf)
    {
        va_list args_copy;
        va_copy(args_copy, args);
        ret = s_orig_vprintf(fmt, args_copy);
        va_end(args_copy);
    }

    /* Format into a temporary line buffer */
    char tmp[256];
    int  len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    if (len <= 0)
    {
        return ret;
    }
    if (len >= (int) sizeof(tmp))
    {
        len = (int) sizeof(tmp) - 1;
    }

    portENTER_CRITICAL(&s_mux);
    for (int i = 0; i < len; i++)
    {
        write_byte(tmp[i]);
    }
    portEXIT_CRITICAL(&s_mux);

    return ret;
}

void log_buffer_install(void)
{
    s_orig_vprintf = esp_log_set_vprintf(log_vprintf_hook);
}

size_t log_buffer_get(char *out, size_t max_len)
{
    if (!out || max_len == 0)
    {
        return 0;
    }

    portENTER_CRITICAL(&s_mux);
    size_t copy_len = (s_fill < max_len - 1) ? s_fill : (max_len - 1);
    size_t start    = (s_head + (s_fill - copy_len)) % LOG_BUF_SIZE;
    for (size_t i = 0; i < copy_len; i++)
    {
        out[i] = s_buf[(start + i) % LOG_BUF_SIZE];
    }
    portEXIT_CRITICAL(&s_mux);

    out[copy_len] = '\0';
    return copy_len;
}
