#pragma once

#include <stddef.h>

#define LOG_BUF_SIZE 8192

/**
 * Install a vprintf hook that captures all ESP log output into a ring buffer.
 * Must be called once before any log output you want to capture.
 * Original output (UART) is preserved.
 */
void log_buffer_install(void);

/**
 * Copy the current ring buffer contents (oldest first) into @p out.
 * At most @p max_len - 1 bytes are written and the string is NUL-terminated.
 * Returns the number of bytes written (not counting the NUL).
 */
size_t log_buffer_get(char *out, size_t max_len);
