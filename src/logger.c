#include "dnsrelay/logger.h"

#include "dnsrelay/platform.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static DrDebugLevel g_level = DR_LOG_NONE;

// 使用当前平台的安全接口把时间戳转换为本地时间。
static void localtime_safe(time_t now, struct tm *result) {
#ifdef _WIN32
    localtime_s(result, &now);
#else
    localtime_r(&now, result);
#endif
}

// 按指定级别格式化输出一行带时间戳的日志。
static void log_with_level(FILE *stream, const char *label, const char *fmt, va_list args) {
    struct tm tm_now;
    time_t now = time(NULL);
    uint64_t ms_now = dr_now_ms() % 1000ULL;
    char time_buf[32];

    localtime_safe(now, &tm_now);
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_now);
    fprintf(stream, "[%s.%03llu] %s ", time_buf, (unsigned long long)ms_now, label);
    vfprintf(stream, fmt, args);
    fputc('\n', stream);
    fflush(stream);
}

// 设置全局日志输出等级。
void dr_logger_init(DrDebugLevel level) {
    g_level = level;
}

// 获取当前全局日志等级。
DrDebugLevel dr_logger_level(void) {
    return g_level;
}

// 在基础调试等级开启时输出普通运行日志。
void dr_log_basic(const char *fmt, ...) {
    va_list args;

    if (g_level < DR_LOG_BASIC) {
        return;
    }
    va_start(args, fmt);
    log_with_level(stdout, "INFO ", fmt, args);
    va_end(args);
}

// 在详细调试等级开启时输出更细粒度的调试日志。
void dr_log_verbose(const char *fmt, ...) {
    va_list args;

    if (g_level < DR_LOG_VERBOSE) {
        return;
    }
    va_start(args, fmt);
    log_with_level(stdout, "DEBUG", fmt, args);
    va_end(args);
}

// 无条件向标准错误输出错误日志。
void dr_log_error(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    log_with_level(stderr, "ERROR", fmt, args);
    va_end(args);
}
