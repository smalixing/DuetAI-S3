#ifndef __HAL_LOG_H__
#define __HAL_LOG_H__

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

void hal_systime_get(char buf[24]);
uint64_t hal_timestamp_get(void);

enum {
    HAL_LOG_LEVEL_VERBOSE = 0,
    HAL_LOG_LEVEL_DEBUG,
    HAL_LOG_LEVEL_INFO,
    HAL_LOG_LEVEL_WARNING,
    HAL_LOG_LEVEL_ERROR,
};

#define HAL_LOG_COLORS
#ifdef HAL_LOG_COLORS
#define LOG_COLOR_BLACK   "30"
#define LOG_COLOR_RED     "31"
#define LOG_COLOR_GREEN   "32"
#define LOG_COLOR_BROWN   "33"
#define LOG_COLOR_BLUE    "34"
#define LOG_COLOR_PURPLE  "35"
#define LOG_COLOR_CYAN    "36"
#define LOG_COLOR(COLOR)  "\033[0;" COLOR "m"
#define LOG_BOLD(COLOR)   "\033[1;" COLOR "m"
#define LOG_RESET_COLOR   "\033[0m"
#define LOG_COLOR_E       LOG_COLOR(LOG_COLOR_RED)
#define LOG_COLOR_W       LOG_COLOR(LOG_COLOR_BROWN)
#define LOG_COLOR_I       LOG_COLOR(LOG_COLOR_GREEN)
#define LOG_COLOR_D
#define LOG_COLOR_V
#else //HAL_LOG_COLORS
#define LOG_COLOR_E
#define LOG_COLOR_W
#define LOG_COLOR_I
#define LOG_COLOR_D
#define LOG_COLOR_V
#define LOG_RESET_COLOR
#endif //HAL_LOG_COLORS

#define LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " (%llu)[%s] %s: " format LOG_RESET_COLOR "\n"

void hal_log_set_level(int level);

void hal_log_printf(int level, const char *fmt, ...);

extern int _hal_log_level;

#define hal_log_verbose(fmt, ...) \
do { \
    if (_hal_log_level > HAL_LOG_LEVEL_VERBOSE) break; \
    char timebuf[24]; \
    hal_systime_get(timebuf); \
    printf(LOG_FORMAT(V, fmt), hal_timestamp_get(), timebuf, __func__, ## __VA_ARGS__); \
} while (0)

#define hal_log_debug(fmt, ...) \
do { \
    if (_hal_log_level > HAL_LOG_LEVEL_DEBUG) break; \
    char timebuf[24]; \
    hal_systime_get(timebuf); \
    printf(LOG_FORMAT(D, fmt), hal_timestamp_get(), timebuf, __func__, ## __VA_ARGS__); \
} while (0)

#define hal_log_info(fmt, ...) \
do { \
    if (_hal_log_level > HAL_LOG_LEVEL_INFO) break; \
    char timebuf[24]; \
    hal_systime_get(timebuf); \
    printf(LOG_FORMAT(I, fmt), hal_timestamp_get(), timebuf, __func__, ## __VA_ARGS__); \
} while (0)

#define hal_log_warn(fmt, ...) \
do { \
    if (_hal_log_level > HAL_LOG_LEVEL_WARNING) break; \
    char timebuf[24]; \
    hal_systime_get(timebuf); \
    printf(LOG_FORMAT(W, fmt), hal_timestamp_get(), timebuf, __func__, ## __VA_ARGS__); \
} while (0)

#define hal_log_err(fmt, ...) \
do { \
    char timebuf[24]; \
    hal_systime_get(timebuf); \
    printf(LOG_FORMAT(E, fmt), hal_timestamp_get(), timebuf, __func__, ## __VA_ARGS__); \
} while (0)

void hal_log_printf_hex(int level, const void *buf, int len);

#define hal_log_hex_verbose(buf, len)       hal_log_printf_hex(HAL_LOG_LEVEL_VERBOSE, buf, len)
#define hal_log_hex_debug(buf, len)         hal_log_printf_hex(HAL_LOG_LEVEL_DEBUG, buf, len)
#define hal_log_hex_info(buf, len)          hal_log_printf_hex(HAL_LOG_LEVEL_INFO, buf, len)
#define hal_log_hex_warn(buf, len)          hal_log_printf_hex(HAL_LOG_LEVEL_WARNING, buf, len)
#define hal_log_hex_err(buf, len)           hal_log_printf_hex(HAL_LOG_LEVEL_ERROR, buf, len)

#endif
