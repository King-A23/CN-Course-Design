#ifndef DNSRELAY_CACHE_H
#define DNSRELAY_CACHE_H

#include "dnsrelay/dns_packet.h"
#include <stddef.h>
#include <stdint.h>

typedef struct DrCacheEntry {
    int in_use; // 槽位是否已被缓存项占用
    char qname[DR_DNS_MAX_DOMAIN_LEN + 1]; // 查询域名
    uint16_t qtype; // 查询类型
    uint16_t qclass; // 查询类别
    uint8_t packet[DR_DNS_MAX_PACKET_SIZE]; // 缓存的 DNS 响应报文副本
    size_t packet_len; // 缓存响应报文长度
    DrTtlPatchList ttl_patches; // 响应中需要按时间修正的 TTL 字段列表
    uint64_t stored_at_ms; // 写入缓存的毫秒时间
    uint64_t expire_at_ms; // 缓存项过期的毫秒时间
    uint64_t last_used_tick; // 最近一次命中的 LRU 访问序号
} DrCacheEntry;

typedef struct DrCache {
    DrCacheEntry *entries; // 缓存项数组
    size_t capacity; // 缓存最大容量
    size_t size; // 当前有效缓存项数量
    uint64_t tick; // LRU 访问序号计数器
} DrCache;

// 初始化 DNS 响应缓存并按容量分配存储空间。
int dr_cache_init(DrCache *cache, size_t capacity);
// 释放缓存占用的内存并重置结构体状态。
void dr_cache_free(DrCache *cache);
// 清理已经超过有效期的缓存项。
void dr_cache_expire(DrCache *cache, uint64_t now_ms);
// 写入一条可缓存的 DNS 响应，同时记录 TTL 修正信息。
int dr_cache_put(
    DrCache *cache,
    const char *qname,
    uint16_t qtype,
    uint16_t qclass,
    const uint8_t *packet,
    size_t packet_len,
    const DrTtlPatchList *ttl_patches,
    uint32_t min_ttl,
    uint64_t now_ms
);
// 按问题三元组查找缓存命中项，并改写响应 ID 与剩余 TTL。
int dr_cache_lookup(
    DrCache *cache,
    const char *qname,
    uint16_t qtype,
    uint16_t qclass,
    uint64_t now_ms,
    uint16_t client_id,
    uint8_t *out_packet,
    size_t *out_packet_len
);
// 返回当前仍然有效的缓存项数量。
size_t dr_cache_size(const DrCache *cache);

#endif
