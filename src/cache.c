#include "dnsrelay/cache.h"

#include <stdlib.h>
#include <string.h>

// 清空单个缓存槽位的全部状态。
static void clear_entry(DrCacheEntry *entry) {
    memset(entry, 0, sizeof(*entry));
}

// 判断缓存项是否与查询域名、类型和类别完全匹配。
static int is_match(const DrCacheEntry *entry, const char *qname, uint16_t qtype, uint16_t qclass) {
    return entry->in_use && entry->qtype == qtype && entry->qclass == qclass && strcmp(entry->qname, qname) == 0;
}

// 查找缓存项，同时顺手清理扫描过程中遇到的过期项。
static size_t find_entry(DrCache *cache, const char *qname, uint16_t qtype, uint16_t qclass, uint64_t now_ms) {
    size_t index;
    for (index = 0; index < cache->capacity; ++index) {
        if (cache->entries[index].in_use && cache->entries[index].expire_at_ms <= now_ms) {
            clear_entry(&cache->entries[index]);
            cache->size -= 1U;
            continue;
        }
        if (is_match(&cache->entries[index], qname, qtype, qclass)) {
            return index;
        }
    }
    return cache->capacity;
}

// 选择写入槽位，优先空槽，否则按最久未使用策略淘汰。
static size_t choose_victim(DrCache *cache) {
    size_t index;
    size_t best_index = 0U;
    uint64_t best_tick = 0U;

    for (index = 0; index < cache->capacity; ++index) {
        if (!cache->entries[index].in_use) {
            return index;
        }
    }

    // 所有槽位都被占用时，last_used_tick 最小的项就是 LRU 淘汰对象。
    best_tick = cache->entries[0].last_used_tick;
    for (index = 1; index < cache->capacity; ++index) {
        if (cache->entries[index].last_used_tick < best_tick) {
            best_tick = cache->entries[index].last_used_tick;
            best_index = index;
        }
    }
    return best_index;
}

// 初始化 DNS 响应缓存并按容量分配存储空间。
int dr_cache_init(DrCache *cache, size_t capacity) {
    memset(cache, 0, sizeof(*cache));
    if (capacity == 0U) {
        return 1;
    }
    cache->entries = (DrCacheEntry *)calloc(capacity, sizeof(DrCacheEntry));
    if (cache->entries == NULL) {
        return 0;
    }
    cache->capacity = capacity;
    return 1;
}

// 释放缓存占用的内存并重置结构体状态。
void dr_cache_free(DrCache *cache) {
    free(cache->entries);
    memset(cache, 0, sizeof(*cache));
}

// 遍历缓存并清理已经超过有效期的响应。
void dr_cache_expire(DrCache *cache, uint64_t now_ms) {
    size_t index;
    if (cache == NULL || cache->entries == NULL) {
        return;
    }
    for (index = 0; index < cache->capacity; ++index) {
        if (cache->entries[index].in_use && cache->entries[index].expire_at_ms <= now_ms) {
            clear_entry(&cache->entries[index]);
            cache->size -= 1U;
        }
    }
}

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
) {
    DrCacheEntry *entry = NULL;
    size_t slot;

    if (cache == NULL || cache->entries == NULL || cache->capacity == 0U || packet_len > DR_DNS_MAX_PACKET_SIZE || min_ttl == 0U) {
        return 0;
    }

    // 写入前先过期清理，避免旧响应继续占用缓存容量。
    dr_cache_expire(cache, now_ms);
    slot = find_entry(cache, qname, qtype, qclass, now_ms);
    if (slot == cache->capacity) {
        slot = choose_victim(cache);
        if (!cache->entries[slot].in_use) {
            cache->size += 1U;
        }
    }

    entry = &cache->entries[slot];
    clear_entry(entry);
    entry->in_use = 1;
    strncpy(entry->qname, qname, sizeof(entry->qname) - 1);
    entry->qtype = qtype;
    entry->qclass = qclass;
    memcpy(entry->packet, packet, packet_len);
    entry->packet_len = packet_len;
    entry->ttl_patches = *ttl_patches;
    entry->stored_at_ms = now_ms;
    entry->expire_at_ms = now_ms + (uint64_t)min_ttl * 1000ULL;
    entry->last_used_tick = ++cache->tick;
    return 1;
}

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
) {
    size_t slot;
    DrCacheEntry *entry = NULL;
    uint32_t elapsed_sec;

    if (cache == NULL || cache->entries == NULL || cache->capacity == 0U) {
        return 0;
    }

    slot = find_entry(cache, qname, qtype, qclass, now_ms);
    if (slot == cache->capacity) {
        return 0;
    }

    entry = &cache->entries[slot];
    memcpy(out_packet, entry->packet, entry->packet_len);
    *out_packet_len = entry->packet_len;
    // 缓存包来自旧请求，返回给当前客户端前必须改回新的事务 ID。
    dr_dns_write_id(out_packet, *out_packet_len, client_id);
    elapsed_sec = (uint32_t)((now_ms - entry->stored_at_ms) / 1000ULL);
    dr_dns_apply_ttl_patches(out_packet, *out_packet_len, &entry->ttl_patches, elapsed_sec);
    entry->last_used_tick = ++cache->tick;
    return 1;
}

// 返回当前仍然有效的缓存项数量。
size_t dr_cache_size(const DrCache *cache) {
    return cache != NULL ? cache->size : 0U;
}
