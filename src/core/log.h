#include <stdarg.h>

#pragma once

typedef void log_fn(void*, int, const char*, va_list);

enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR };

#define log_debug(...) log_log(LOG_DEBUG, __VA_ARGS__)
#define log_info(...) log_log(LOG_INFO, __VA_ARGS__)
#define log_warn(...) log_log(LOG_WARN, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __VA_ARGS__)

void log_writev(int level, const char* format, va_list args);
void log_write(int level, const char* format, ...);
void log_register(log_fn* callback, void* context);
void log_log(int level, const char* format, ...);
