#ifndef DNSRELAY_CONFIG_H
#define DNSRELAY_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define DR_DEFAULT_UPSTREAM_IP "202.106.0.20"
#define DR_DEFAULT_UPSTREAM_PORT 53U
#define DR_DEFAULT_TABLE_FILE "dnsrelay.txt"
#define DR_DEFAULT_BIND_IP "0.0.0.0"
#define DR_DEFAULT_CACHE_CAPACITY 128U
#define DR_DEFAULT_UPSTREAM_TIMEOUT_MS 5000U
#define DR_MAX_PATH_LEN 512
#define DR_MAX_IPV4_TEXT_LEN 16

#ifndef DNSRELAY_DEV_PORT
#define DNSRELAY_DEV_PORT 53
#endif

typedef enum DrDebugLevel {
    DR_LOG_NONE = 0, // 不输出调试日志
    DR_LOG_BASIC = 1, // 输出基础调试日志
    DR_LOG_VERBOSE = 2 // 输出详细调试日志
} DrDebugLevel;

typedef struct DrConfig {
    DrDebugLevel debug_level; // 调试日志输出等级
    int show_help; // 是否只显示帮助信息
    char upstream_ip[DR_MAX_IPV4_TEXT_LEN]; // 上游 DNS 服务器 IPv4 地址
    uint16_t upstream_port; // 上游 DNS 服务器端口
    char table_path[DR_MAX_PATH_LEN]; // 本地域名表文件路径
    char bind_ip[DR_MAX_IPV4_TEXT_LEN]; // 本机监听 IPv4 地址
    uint16_t bind_port; // 本机监听端口
    size_t cache_capacity; // DNS 响应缓存容量
    uint32_t upstream_timeout_ms; // 等待上游响应的超时时间
} DrConfig;

void dr_config_set_defaults(DrConfig *config);
int dr_config_parse(DrConfig *config, int argc, char **argv, char *errbuf, size_t errbuf_size);
void dr_config_print_usage(const char *program_name);

#endif
