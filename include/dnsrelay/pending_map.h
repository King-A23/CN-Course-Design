#ifndef DNSRELAY_PENDING_MAP_H
#define DNSRELAY_PENDING_MAP_H

#include "dnsrelay/dns_packet.h"
#include "dnsrelay/platform.h"
#include <stdint.h>

typedef struct DrPendingRequest {
    int active;
    uint16_t upstream_id;
    uint16_t client_id;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;
    uint8_t query_packet[DR_DNS_MAX_PACKET_SIZE];
    size_t query_len;
    uint64_t started_ms;
    char qname[DR_DNS_MAX_DOMAIN_LEN + 1];
    uint16_t qtype;
    uint16_t qclass;
} DrPendingRequest;

typedef struct DrPendingMap {
    DrPendingRequest *slots;
    uint16_t next_id;
    size_t active_count;
} DrPendingMap;

int dr_pending_map_init(DrPendingMap *map);
void dr_pending_map_free(DrPendingMap *map);
DrPendingRequest *dr_pending_map_get(DrPendingMap *map, uint16_t upstream_id);
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
int dr_pending_map_remove(DrPendingMap *map, uint16_t upstream_id, DrPendingRequest *out_request);
int dr_pending_map_pop_expired(
    DrPendingMap *map,
    uint64_t now_ms,
    uint32_t timeout_ms,
    DrPendingRequest *out_request
);
size_t dr_pending_map_active_count(const DrPendingMap *map);

#endif
