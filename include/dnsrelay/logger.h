#ifndef DNSRELAY_LOGGER_H
#define DNSRELAY_LOGGER_H

#include "dnsrelay/config.h"

void dr_logger_init(DrDebugLevel level);
DrDebugLevel dr_logger_level(void);
void dr_log_basic(const char *fmt, ...);
void dr_log_verbose(const char *fmt, ...);
void dr_log_error(const char *fmt, ...);

#endif
