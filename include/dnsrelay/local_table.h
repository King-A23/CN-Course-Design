#ifndef DNSRELAY_LOCAL_TABLE_H
#define DNSRELAY_LOCAL_TABLE_H

#include <stddef.h>
#include <stdint.h>

typedef enum DrLocalLookupKind {
    DR_LOCAL_MISS = 0, // 本地域名表未命中
    DR_LOCAL_HIT = 1, // 本地域名表命中并返回 IP
    DR_LOCAL_BLOCKED = 2 // 本地域名表命中拦截规则
} DrLocalLookupKind;

typedef struct DrLocalLookupResult {
    DrLocalLookupKind kind; // 查询结果类型
    uint32_t ipv4_be; // 命中的 IPv4 地址网络字节序
    uint32_t ttl; // 本地响应使用的 TTL 秒数
} DrLocalLookupResult;

typedef struct DrLocalEntry {
    char *domain; // 本地域名规则中的域名
    uint32_t ipv4_be; // 规则对应的 IPv4 地址网络字节序
    int in_use; // 槽位是否已被规则占用
} DrLocalEntry;

typedef struct DrLocalTable {
    DrLocalEntry *entries; // 本地域名规则数组
    size_t capacity; // 规则表最大容量
    size_t size; // 当前已加载规则数量
    uint32_t local_ttl; // 本地构造响应时使用的 TTL 秒数
} DrLocalTable;

// 从 dnsrelay.txt 样式文件加载本地域名规则。
int dr_local_table_load(DrLocalTable *table, const char *path, char *errbuf, size_t errbuf_size);
// 释放本地域名表中的域名字符串和表空间。
void dr_local_table_free(DrLocalTable *table);
// 查询域名是否命中本地解析、屏蔽规则或未命中。
DrLocalLookupResult dr_local_table_lookup(const DrLocalTable *table, const char *qname);
// 返回本地域名表中已加载的规则数量。
size_t dr_local_table_size(const DrLocalTable *table);

#endif
