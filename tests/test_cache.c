#include "dnsrelay/cache.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static const uint8_t k_response[] = {
    0x11, 0x11,
    0x81, 0x80,
    0x00, 0x01,
    0x00, 0x01,
    0x00, 0x00,
    0x00, 0x00,
    0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
    0x03, 'c', 'o', 'm',
    0x00,
    0x00, 0x01,
    0x00, 0x01,
    0xc0, 0x0c,
    0x00, 0x01,
    0x00, 0x01,
    0x00, 0x00, 0x00, 0x3c,
    0x00, 0x04,
    0x01, 0x02, 0x03, 0x04
};

static uint32_t read_ttl(const uint8_t *packet) {
    return ((uint32_t)packet[35] << 24) |
           ((uint32_t)packet[36] << 16) |
           ((uint32_t)packet[37] << 8) |
           (uint32_t)packet[38];
}

int main(void) {
    DrCache cache;
    DrTtlPatchList patches;
    uint8_t out[DR_DNS_MAX_PACKET_SIZE];
    size_t out_len = 0U;
    uint32_t min_ttl = 0U;
    const uint64_t now_ms = 100000U;

    CHECK(dr_dns_collect_ttls(k_response, sizeof(k_response), &patches, &min_ttl));
    CHECK(patches.count == 1U);
    CHECK(min_ttl == 60U);

    CHECK(dr_cache_init(&cache, 2U));
    CHECK(dr_cache_put(&cache, "example.com", 1U, 1U, k_response, sizeof(k_response), &patches, min_ttl, now_ms));
    CHECK(dr_cache_lookup(&cache, "example.com", 1U, 1U, now_ms + 12000U, 0xabcdU, out, &out_len));
    CHECK(out_len == sizeof(k_response));
    CHECK(out[0] == 0xabU && out[1] == 0xcdU);
    CHECK(read_ttl(out) == 48U);

    CHECK(!dr_cache_lookup(&cache, "example.com", 1U, 1U, now_ms + 60000U, 0x2000U, out, &out_len));
    CHECK(dr_cache_size(&cache) == 0U);

    CHECK(dr_cache_put(&cache, "one.example", 1U, 1U, k_response, sizeof(k_response), &patches, min_ttl, now_ms));
    CHECK(dr_cache_put(&cache, "two.example", 1U, 1U, k_response, sizeof(k_response), &patches, min_ttl, now_ms));
    CHECK(dr_cache_lookup(&cache, "one.example", 1U, 1U, now_ms, 0x3000U, out, &out_len));
    CHECK(dr_cache_put(&cache, "three.example", 1U, 1U, k_response, sizeof(k_response), &patches, min_ttl, now_ms));
    CHECK(dr_cache_lookup(&cache, "one.example", 1U, 1U, now_ms, 0x3001U, out, &out_len));
    CHECK(!dr_cache_lookup(&cache, "two.example", 1U, 1U, now_ms, 0x3002U, out, &out_len));
    CHECK(dr_cache_lookup(&cache, "three.example", 1U, 1U, now_ms, 0x3003U, out, &out_len));

    dr_cache_free(&cache);
    return 0;
}
