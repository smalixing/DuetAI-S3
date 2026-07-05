#include "hal_log.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>

int _hal_log_level = HAL_LOG_LEVEL_VERBOSE;

void hal_log_set_level(int level)
{
    _hal_log_level = level;
}

#define BYTES_PER_LINE 16
void hal_log_printf_hex(int level, const void *buf, int len)
{
    if (level < _hal_log_level)
        return;

    if (len == 0)
        return;

    const unsigned char *ptr = (const unsigned char *)buf;
    char print_buf[3 * BYTES_PER_LINE + 1 + BYTES_PER_LINE + 1 + 1];
    do {
        int bytes_cur_line;
        if (len > BYTES_PER_LINE) {
            bytes_cur_line = BYTES_PER_LINE;
        } else {
            bytes_cur_line = len;
        }
        for (int i = 0; i < bytes_cur_line; i++) {
            sprintf(print_buf + 3 * i, "%02x ", ptr[i]);
            if (isprint(ptr[i])) {
                sprintf(print_buf + 3 * BYTES_PER_LINE + 1 + i, "%c", ptr[i]);
            } else {
                sprintf(print_buf + 3 * BYTES_PER_LINE + 1 + i, ".");
            }
        }
        for (int i = bytes_cur_line; i < BYTES_PER_LINE; i++) {
            sprintf(print_buf + 3 * i, "XX ");
            sprintf(print_buf + 3 * BYTES_PER_LINE + 1 + i, ".");
        }
        print_buf[3 * BYTES_PER_LINE] = '|';
        print_buf[3 * BYTES_PER_LINE + 1 + BYTES_PER_LINE] = '\n';
        print_buf[3 * BYTES_PER_LINE + 1 + BYTES_PER_LINE + 1] = 0;
        fwrite(print_buf, sizeof(print_buf), 1, stdout);
        ptr += bytes_cur_line;
        len -= bytes_cur_line;
    } while (len > 0);
}

void hal_log_printf(int level, const char *fmt, ...)
{
    va_list list;
    va_start(list, fmt);
    vprintf(fmt, list);
    va_end(list);
}

void hal_systime_get(char buf[24])
{
    struct timeval tv;
    struct tm timeinfo;

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &timeinfo);

    snprintf(buf, 24,
                "%02d-%02d %02d:%02d:%02d.%03ld",
                (unsigned int)timeinfo.tm_mon + 1,
                (unsigned int)timeinfo.tm_mday,
                (unsigned int)timeinfo.tm_hour,
                (unsigned int)timeinfo.tm_min,
                (unsigned int)timeinfo.tm_sec,
                (unsigned long)tv.tv_usec / 1000);
}

uint64_t hal_timestamp_get(void)
{
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (uint64_t)tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
}
