#ifndef DNSRELAY_LOGGER_H
#define DNSRELAY_LOGGER_H

#include "dnsrelay/config.h"

// 设置全局日志输出等级。
void dr_logger_init(DrDebugLevel level);
// 获取当前全局日志等级。
DrDebugLevel dr_logger_level(void);
// 输出基础调试日志。
void dr_log_basic(const char *fmt, ...);
// 输出详细调试日志。
void dr_log_verbose(const char *fmt, ...);
// 输出错误日志。
void dr_log_error(const char *fmt, ...);

#endif
