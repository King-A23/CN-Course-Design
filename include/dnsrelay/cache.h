#ifndef DNSRELAY_CACHE_H
#define DNSRELAY_CACHE_H

#include "dnsrelay/dns_packet.h"
#include <stddef.h>
#include <stdint.h>

typedef struct DrCacheEntry {
    int in_use;
    char qname[DR_DNS_MAX_DOMAIN_LEN + 1];
    uint16_t qtype;
    uint16_t qclass;
    uint8_t packet[DR_DNS_MAX_PACKET_SIZE];
    size_t packet_len;
    DrTtlPatchList ttl_patches;
    uint64_t stored_at_ms;
    uint64_t expire_at_ms;
    uint64_t last_used_tick;
} DrCacheEntry;

typedef struct DrCache {
    DrCacheEntry *entries;
    size_t capacity;
    size_t size;
    uint64_t tick;
} DrCache;

int dr_cache_init(DrCache *cache, size_t capacity);
void dr_cache_free(DrCache *cache);
void dr_cache_expire(DrCache *cache, uint64_t now_ms);
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
size_t dr_cache_size(const DrCache *cache);

#endif
