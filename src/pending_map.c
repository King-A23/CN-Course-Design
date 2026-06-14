#include "dnsrelay/pending_map.h"

#include <stdlib.h>
#include <string.h>

#define DR_PENDING_SLOT_COUNT 65536U

int dr_pending_map_init(DrPendingMap *map) {
    memset(map, 0, sizeof(*map));
    map->slots = (DrPendingRequest *)calloc(DR_PENDING_SLOT_COUNT, sizeof(DrPendingRequest));
    if (map->slots == NULL) {
        return 0;
    }
    map->next_id = 1;
    return 1;
}

void dr_pending_map_free(DrPendingMap *map) {
    free(map->slots);
    memset(map, 0, sizeof(*map));
}

DrPendingRequest *dr_pending_map_get(DrPendingMap *map, uint16_t upstream_id) {
    if (map == NULL || map->slots == NULL) {
        return NULL;
    }
    if (!map->slots[upstream_id].active) {
        return NULL;
    }
    return &map->slots[upstream_id];
}

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
) {
    uint32_t attempts;
    uint16_t candidate;

    if (map == NULL || map->slots == NULL || client_addr == NULL || qname == NULL ||
        query_packet == NULL || query_len == 0U || query_len > DR_DNS_MAX_PACKET_SIZE ||
        client_addr_len == 0 || (size_t)client_addr_len > sizeof(struct sockaddr_storage)) {
        return 0;
    }

    candidate = map->next_id;
    if (candidate == 0) {
        candidate = 1;
    }

    for (attempts = 0; attempts < DR_PENDING_SLOT_COUNT - 1U; ++attempts) {
        if (!map->slots[candidate].active) {
            DrPendingRequest *request = &map->slots[candidate];
            memset(request, 0, sizeof(*request));
            request->active = 1;
            request->upstream_id = candidate;
            request->client_id = client_id;
            memcpy(&request->client_addr, client_addr, (size_t)client_addr_len);
            request->client_addr_len = client_addr_len;
            memcpy(request->query_packet, query_packet, query_len);
            request->query_len = query_len;
            request->started_ms = started_ms;
            strncpy(request->qname, qname, sizeof(request->qname) - 1);
            request->qtype = qtype;
            request->qclass = qclass;
            map->active_count += 1U;
            map->next_id = (uint16_t)(candidate + 1U);
            if (map->next_id == 0) {
                map->next_id = 1;
            }
            if (upstream_id != NULL) {
                *upstream_id = candidate;
            }
            return 1;
        }
        candidate = (uint16_t)(candidate + 1U);
        if (candidate == 0) {
            candidate = 1;
        }
    }

    return 0;
}

int dr_pending_map_remove(DrPendingMap *map, uint16_t upstream_id, DrPendingRequest *out_request) {
    DrPendingRequest *request = dr_pending_map_get(map, upstream_id);
    if (request == NULL) {
        return 0;
    }
    if (out_request != NULL) {
        *out_request = *request;
    }
    memset(request, 0, sizeof(*request));
    map->active_count -= 1U;
    return 1;
}

int dr_pending_map_pop_expired(
    DrPendingMap *map,
    uint64_t now_ms,
    uint32_t timeout_ms,
    DrPendingRequest *out_request
) {
    uint32_t index;

    if (map == NULL || map->slots == NULL) {
        return 0;
    }

    for (index = 0; index < DR_PENDING_SLOT_COUNT; ++index) {
        DrPendingRequest *request = &map->slots[index];
        if (request->active && now_ms - request->started_ms >= (uint64_t)timeout_ms) {
            if (out_request != NULL) {
                *out_request = *request;
            }
            memset(request, 0, sizeof(*request));
            map->active_count -= 1U;
            return 1;
        }
    }

    return 0;
}

size_t dr_pending_map_active_count(const DrPendingMap *map) {
    return map != NULL ? map->active_count : 0U;
}
