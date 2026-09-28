/*
 * MIT License
 * C-language port of PageManager log macros.
 */
#ifndef __PM_LOG_H
#define __PM_LOG_H

#define PAGE_MANAGER_USE_LOG 0

#if !defined(ARDUINO) && PAGE_MANAGER_USE_LOG
#include <stdio.h>
#  define _PM_LOG(format, ...)      do{log_message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, format, ##__VA_ARGS__);}while(0)
#  define PM_LOG_INFO(format, ...)  _PM_LOG("[Info] " format, ##__VA_ARGS__)
#  define PM_LOG_WARN(format, ...)  _PM_LOG("[Warn] " format, ##__VA_ARGS__)
#  define PM_LOG_ERROR(format, ...) _PM_LOG("[Error] " format, ##__VA_ARGS__)
#else
#  define PM_LOG_INFO(...)
#  define PM_LOG_WARN(...)
#  define PM_LOG_ERROR(...)
#endif

#endif