#include "dnsrelay/pending_map.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static const uint8_t k_query[] = {
    0x12, 0x34,
    0x01, 0x00,
    0x00, 0x01,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
    0x03, 'c', 'o', 'm',
    0x00,
    0x00, 0x01,
    0x00, 0x01
};

int main(void) {
    DrPendingMap map;
    struct sockaddr_in client_addr;
    DrPendingRequest *pending;
    DrPendingRequest removed;
    uint16_t upstream_id = 0U;

    CHECK(dr_pending_map_init(&map));
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(53000);
    client_addr.sin_addr.s_addr = htonl(0x7f000001U);

    CHECK(dr_pending_map_insert(
        &map,
        0x1234U,
        (const struct sockaddr *)&client_addr,
        (socklen_t)sizeof(client_addr),
        k_query,
        sizeof(k_query),
        "example.com",
        1U,
        1U,
        1000U,
        &upstream_id));
    CHECK(upstream_id != 0U);
    CHECK(dr_pending_map_active_count(&map) == 1U);
    pending = dr_pending_map_get(&map, upstream_id);
    CHECK(pending != NULL);
    CHECK(pending->query_len == sizeof(k_query));
    CHECK(memcmp(pending->query_packet, k_query, sizeof(k_query)) == 0);

    CHECK(dr_pending_map_remove(&map, upstream_id, &removed));
    CHECK(removed.client_id == 0x1234U);
    CHECK(strcmp(removed.qname, "example.com") == 0);
    CHECK(removed.query_len == sizeof(k_query));
    CHECK(memcmp(removed.query_packet, k_query, sizeof(k_query)) == 0);
    CHECK(dr_pending_map_active_count(&map) == 0U);

    CHECK(!dr_pending_map_insert(
        &map,
        0x1234U,
        (const struct sockaddr *)&client_addr,
        (socklen_t)sizeof(client_addr),
        k_query,
        0U,
        "invalid.example",
        1U,
        1U,
        1000U,
        &upstream_id));
    CHECK(!dr_pending_map_insert(
        &map,
        0x1234U,
        (const struct sockaddr *)&client_addr,
        (socklen_t)sizeof(client_addr),
        k_query,
        DR_DNS_MAX_PACKET_SIZE + 1U,
        "invalid.example",
        1U,
        1U,
        1000U,
        &upstream_id));
    CHECK(!dr_pending_map_insert(
        &map,
        0x1234U,
        (const struct sockaddr *)&client_addr,
        (socklen_t)sizeof(client_addr),
        NULL,
        sizeof(k_query),
        "invalid.example",
        1U,
        1U,
        1000U,
        &upstream_id));
    CHECK(dr_pending_map_active_count(&map) == 0U);

    CHECK(dr_pending_map_insert(
        &map,
        0x2000U,
        (const struct sockaddr *)&client_addr,
        (socklen_t)sizeof(client_addr),
        k_query,
        sizeof(k_query),
        "timeout.example",
        1U,
        1U,
        1000U,
        &upstream_id));
    CHECK(!dr_pending_map_pop_expired(&map, 5999U, 5000U, &removed));
    CHECK(dr_pending_map_active_count(&map) == 1U);
    CHECK(dr_pending_map_pop_expired(&map, 6000U, 5000U, &removed));
    CHECK(removed.client_id == 0x2000U);
    CHECK(strcmp(removed.qname, "timeout.example") == 0);
    CHECK(removed.query_len == sizeof(k_query));
    CHECK(memcmp(removed.query_packet, k_query, sizeof(k_query)) == 0);
    CHECK(dr_pending_map_active_count(&map) == 0U);
    CHECK(!dr_pending_map_pop_expired(&map, 7001U, 5000U, &removed));

    dr_pending_map_free(&map);
    return 0;
}
