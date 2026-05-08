#include "dnsrelay/dns_packet.h"
#include "dnsrelay/platform.h"

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
    0x05, 't', 'e', 's', 't', '1',
    0x00,
    0x00, 0x01,
    0x00, 0x01
};

int main(void) {
    DrParsedQuery parsed;
    DrDnsHeader header;
    DrTtlPatchList patches;
    uint8_t response[DR_DNS_MAX_PACKET_SIZE];
    size_t response_len = 0U;
    uint32_t ip_be = 0U;
    uint32_t min_ttl = 0U;
    char errbuf[128];

    CHECK(dr_dns_parse_header(k_query, sizeof(k_query), &header));
    CHECK(header.id == 0x1234U);
    CHECK(header.qdcount == 1U);

    CHECK(dr_dns_parse_query(k_query, sizeof(k_query), &parsed, errbuf, sizeof(errbuf)));
    CHECK(strcmp(parsed.qname, "test1") == 0);
    CHECK(parsed.qtype == 1U);
    CHECK(parsed.qclass == 1U);

    CHECK(dr_parse_ipv4("11.111.11.111", &ip_be));
    CHECK(dr_dns_build_a_response(k_query, sizeof(k_query), &parsed, ip_be, 60U, response, &response_len));
    CHECK(response_len == sizeof(k_query) + 16U);
    CHECK(dr_dns_is_response(response, response_len));
    CHECK(dr_dns_get_rcode(response, response_len) == DR_DNS_RCODE_NOERROR);
    CHECK(dr_dns_collect_ttls(response, response_len, &patches, &min_ttl));
    CHECK(patches.count == 1U);
    CHECK(min_ttl == 60U);

    dr_dns_apply_ttl_patches(response, response_len, &patches, 10U);
    CHECK(response[parsed.question_end_offset + 6U] == 0x00);
    CHECK(response[parsed.question_end_offset + 7U] == 0x00);
    CHECK(response[parsed.question_end_offset + 8U] == 0x00);
    CHECK(response[parsed.question_end_offset + 9U] == 0x32);

    CHECK(dr_dns_build_error_response(k_query, sizeof(k_query), DR_DNS_RCODE_NXDOMAIN, response, &response_len));
    CHECK(dr_dns_get_rcode(response, response_len) == DR_DNS_RCODE_NXDOMAIN);
    CHECK(response_len == sizeof(k_query));
    return 0;
}
