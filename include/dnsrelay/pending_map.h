#ifndef DNSRELAY_PENDING_MAP_H
#define DNSRELAY_PENDING_MAP_H

#include "dnsrelay/dns_packet.h"
#include "dnsrelay/platform.h"
#include <stdint.h>

typedef struct DrPendingRequest {
    int active; // 请求槽位是否正在等待上游响应
    uint16_t upstream_id; // 转发给上游 DNS 时使用的新事务 ID
    uint16_t client_id; // 客户端原始查询事务 ID
    struct sockaddr_storage client_addr; // 客户端地址
    socklen_t client_addr_len; // 客户端地址长度
    uint8_t query_packet[DR_DNS_MAX_PACKET_SIZE]; // 客户端原始查询报文副本
    size_t query_len; // 客户端原始查询报文长度
    uint64_t started_ms; // 请求转发给上游的毫秒时间
    char qname[DR_DNS_MAX_DOMAIN_LEN + 1]; // 查询域名
    uint16_t qtype; // 查询类型
    uint16_t qclass; // 查询类别
} DrPendingRequest;

typedef struct DrPendingMap {
    DrPendingRequest *slots; // 按上游事务 ID 索引的请求槽位数组
    uint16_t next_id; // 下一次尝试分配的上游事务 ID
    size_t active_count; // 当前等待上游响应的请求数量
} DrPendingMap;

// 初始化待处理请求映射表。
int dr_pending_map_init(DrPendingMap *map);
// 释放待处理请求映射表。
void dr_pending_map_free(DrPendingMap *map);
// 按上游请求 ID 获取仍在等待响应的请求。
DrPendingRequest *dr_pending_map_get(DrPendingMap *map, uint16_t upstream_id);
// 保存客户端请求信息并分配新的上游请求 ID。
int dr_pending_map_insert(
    DrPendingMap *map,
    uint16_t client_id,
    const struct sockaddr *client_addr,
    socklen_t client_addr_len,
    const uint8_t *query_packet,
    size_t query_len,
    const char *qname,
    uint16_t qtype,
    uint16_t qclass,
    uint64_t started_ms,
    uint16_t *upstream_id
);
// 删除指定上游请求 ID 的映射并可选返回原请求信息。
int dr_pending_map_remove(DrPendingMap *map, uint16_t upstream_id, DrPendingRequest *out_request);
// 弹出一个超时未收到响应的上游请求。
int dr_pending_map_pop_expired(
    DrPendingMap *map,
    uint64_t now_ms,
    uint32_t timeout_ms,
    DrPendingRequest *out_request
);
// 返回当前仍在等待上游响应的请求数量。
size_t dr_pending_map_active_count(const DrPendingMap *map);

#endif
