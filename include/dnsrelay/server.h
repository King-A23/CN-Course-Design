#ifndef DNSRELAY_SERVER_H
#define DNSRELAY_SERVER_H

#include "dnsrelay/config.h"

// 按给定配置启动 DNS 中继服务主循环。
int dr_server_run(const DrConfig *config);

#endif
