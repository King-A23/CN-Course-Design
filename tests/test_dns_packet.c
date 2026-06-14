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
    0x05, 'T', 'e', 'S', 't', '1',
    0x00,
    0x00, 0x01,
    0x00, 0x01
};

static const uint8_t k_root_query[] = {
    0x33, 0x33,
    0x01, 0x00,
    0x00, 0x01,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00,
    0x00, 0x01,
    0x00, 0x01
};

static const uint8_t k_compressed_query[] = {
    0xab, 0xcd,
    0x01, 0x00,
    0x00, 0x01,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0xc0, 0x12,
    0x00, 0x01,
    0x00, 0x01,
    0x03, 'W', 'w', 'W',
    0x07, 'E', 'x', 'a', 'm', 'p', 'l', 'e',
    0x00
};

static void write_u16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)((value >> 8) & 0xffU);
    bytes[1] = (uint8_t)(value & 0xffU);
}

static void write_u32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)((value >> 24) & 0xffU);
    bytes[1] = (uint8_t)((value >> 16) & 0xffU);
    bytes[2] = (uint8_t)((value >> 8) & 0xffU);
    bytes[3] = (uint8_t)(value & 0xffU);
}

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
}

static uint32_t read_u32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) |
           (uint32_t)bytes[3];
}

static int test_parse_normal_query(void) {
    DrParsedQuery parsed;
    DrDnsHeader header;
    char normalized[DR_DNS_MAX_DOMAIN_LEN + 1];
    char errbuf[128];

    CHECK(!dr_dns_parse_header(NULL, sizeof(k_query), &header));
    CHECK(!dr_dns_parse_header(k_query, 4U, &header));
    CHECK(dr_dns_parse_header(k_query, sizeof(k_query), &header));
    CHECK(header.id == 0x1234U);
    CHECK(header.flags == 0x0100U);
    CHECK(header.qdcount == 1U);

    CHECK(dr_dns_parse_query(k_query, sizeof(k_query), &parsed, errbuf, sizeof(errbuf)));
    CHECK(parsed.id == 0x1234U);
    CHECK(parsed.opcode == 0U);
    CHECK(parsed.qr == 0U);
    CHECK(strcmp(parsed.qname, "test1") == 0);
    CHECK(parsed.qtype == 1U);
    CHECK(parsed.qclass == 1U);
    CHECK(parsed.question_end_offset == sizeof(k_query));

    CHECK(dr_dns_normalize_name("WWW.Example.COM.", normalized, sizeof(normalized)));
    CHECK(strcmp(normalized, "www.example.com") == 0);
    CHECK(!dr_dns_normalize_name("", normalized, sizeof(normalized)));
    CHECK(!dr_dns_normalize_name(".example.com", normalized, sizeof(normalized)));
    CHECK(!dr_dns_normalize_name("example..com", normalized, sizeof(normalized)));
    return 0;
}

static int test_parse_root_query(void) {
    DrParsedQuery parsed;
    char errbuf[128];

    CHECK(dr_dns_parse_query(k_root_query, sizeof(k_root_query), &parsed, errbuf, sizeof(errbuf)));
    CHECK(parsed.id == 0x3333U);
    CHECK(strcmp(parsed.qname, ".") == 0);
    CHECK(parsed.qtype == 1U);
    CHECK(parsed.qclass == 1U);
    CHECK(parsed.question_end_offset == sizeof(k_root_query));
    return 0;
}

static int test_parse_compressed_query(void) {
    DrParsedQuery parsed;
    char errbuf[128];

    CHECK(dr_dns_parse_query(k_compressed_query, sizeof(k_compressed_query), &parsed, errbuf, sizeof(errbuf)));
    CHECK(strcmp(parsed.qname, "www.example") == 0);
    CHECK(parsed.question_end_offset == 18U);
    return 0;
}

static int test_parse_rejects_malformed_queries(void) {
    uint8_t packet[64];
    DrParsedQuery parsed;
    char errbuf[128];

    memcpy(packet, k_query, sizeof(k_query));
    packet[4] = 0x00;
    packet[5] = 0x00;
    CHECK(!dr_dns_parse_query(packet, sizeof(k_query), &parsed, errbuf, sizeof(errbuf)));

    memcpy(packet, k_query, sizeof(k_query));
    packet[12] = 0x40;
    CHECK(!dr_dns_parse_query(packet, sizeof(k_query), &parsed, errbuf, sizeof(errbuf)));

    memcpy(packet, k_query, sizeof(k_query));
    packet[12] = 0xc0;
    packet[13] = 0xff;
    CHECK(!dr_dns_parse_query(packet, sizeof(k_query), &parsed, errbuf, sizeof(errbuf)));

    CHECK(!dr_dns_parse_query(k_query, sizeof(k_query) - 1U, &parsed, errbuf, sizeof(errbuf)));

    memcpy(packet, k_query, sizeof(k_query));
    packet[12] = 0x05;
    CHECK(!dr_dns_parse_query(packet, 15U, &parsed, errbuf, sizeof(errbuf)));
    CHECK(!dr_dns_parse_query(k_query, sizeof(k_query), NULL, errbuf, sizeof(errbuf)));
    return 0;
}

static int test_build_responses_and_ttl_patches(void) {
    DrParsedQuery parsed;
    DrTtlPatchList patches;
    uint8_t response[DR_DNS_MAX_PACKET_SIZE];
    size_t response_len = 0U;
    uint32_t ip_be = 0U;
    uint32_t min_ttl = 0U;
    char errbuf[128];

    CHECK(dr_dns_parse_query(k_query, sizeof(k_query), &parsed, errbuf, sizeof(errbuf)));
    CHECK(dr_parse_ipv4("11.22.33.44", &ip_be));
    CHECK(dr_dns_build_a_response(k_query, sizeof(k_query), &parsed, ip_be, 60U, response, &response_len));
    CHECK(response_len == sizeof(k_query) + 16U);
    CHECK(dr_dns_is_response(response, response_len));
    CHECK(dr_dns_get_opcode(response, response_len) == 0U);
    CHECK(dr_dns_get_rcode(response, response_len) == DR_DNS_RCODE_NOERROR);
    CHECK(read_u16(response) == 0x1234U);
    CHECK(read_u16(response + 2U) == 0x8180U);
    CHECK(read_u16(response + 4U) == 1U);
    CHECK(read_u16(response + 6U) == 1U);
    CHECK(read_u16(response + parsed.question_end_offset) == 0xc00cU);
    CHECK(read_u32(response + parsed.question_end_offset + 6U) == 60U);
    CHECK(response[parsed.question_end_offset + 12U] == 11U);
    CHECK(response[parsed.question_end_offset + 13U] == 22U);
    CHECK(response[parsed.question_end_offset + 14U] == 33U);
    CHECK(response[parsed.question_end_offset + 15U] == 44U);

    CHECK(dr_dns_collect_ttls(response, response_len, &patches, &min_ttl));
    CHECK(patches.count == 1U);
    CHECK(min_ttl == 60U);
    dr_dns_apply_ttl_patches(response, response_len, &patches, 10U);
    CHECK(read_u32(response + parsed.question_end_offset + 6U) == 50U);

    CHECK(dr_dns_build_error_response(k_query, sizeof(k_query), DR_DNS_RCODE_NXDOMAIN, response, &response_len));
    CHECK(response_len == sizeof(k_query));
    CHECK(dr_dns_get_rcode(response, response_len) == DR_DNS_RCODE_NXDOMAIN);
    CHECK(read_u16(response + 6U) == 0U);

    CHECK(dr_dns_build_error_response(k_query, sizeof(k_query), DR_DNS_RCODE_SERVFAIL, response, &response_len));
    CHECK(dr_dns_get_rcode(response, response_len) == DR_DNS_RCODE_SERVFAIL);

    CHECK(dr_dns_build_error_response(k_query, sizeof(k_query), DR_DNS_RCODE_REFUSED, response, &response_len));
    CHECK(dr_dns_get_rcode(response, response_len) == DR_DNS_RCODE_REFUSED);

    memcpy(response, k_query, sizeof(k_query));
    write_u16(response + 4U, 0U);
    CHECK(dr_dns_build_error_response(response, DR_DNS_HEADER_SIZE, DR_DNS_RCODE_FORMERR, response, &response_len));
    CHECK(response_len == DR_DNS_HEADER_SIZE);
    CHECK(dr_dns_get_rcode(response, response_len) == DR_DNS_RCODE_FORMERR);

    CHECK(!dr_dns_build_a_response(NULL, sizeof(k_query), &parsed, ip_be, 60U, response, &response_len));
    CHECK(!dr_dns_build_error_response(NULL, sizeof(k_query), DR_DNS_RCODE_FORMERR, response, &response_len));
    return 0;
}

static int test_compressed_query_responses_are_canonicalized(void) {
    DrParsedQuery parsed;
    uint8_t response[DR_DNS_MAX_PACKET_SIZE];
    size_t response_len = 0U;
    uint32_t ip_be = 0U;
    char errbuf[128];

    CHECK(dr_dns_parse_query(k_compressed_query, sizeof(k_compressed_query), &parsed, errbuf, sizeof(errbuf)));
    CHECK(dr_parse_ipv4("11.22.33.44", &ip_be));
    CHECK(dr_dns_build_a_response(k_compressed_query, sizeof(k_compressed_query), &parsed, ip_be, 60U, response, &response_len));
    CHECK(response_len == 45U);
    CHECK(response[12] == 3U);
    CHECK(memcmp(response + 13U, "www", 3U) == 0);
    CHECK(response[16] == 7U);
    CHECK(memcmp(response + 17U, "example", 7U) == 0);
    CHECK(response[24] == 0U);
    CHECK(read_u16(response + 29U) == 0xc00cU);

    CHECK(dr_dns_build_error_response(k_compressed_query, sizeof(k_compressed_query), DR_DNS_RCODE_NXDOMAIN, response, &response_len));
    CHECK(response_len == 29U);
    CHECK(response[12] == 3U);
    CHECK(memcmp(response + 13U, "www", 3U) == 0);
    CHECK(response[24] == 0U);
    CHECK(dr_dns_get_rcode(response, response_len) == DR_DNS_RCODE_NXDOMAIN);
    return 0;
}

static int test_collect_ttls_skips_edns_opt(void) {
    DrParsedQuery parsed;
    DrTtlPatchList patches;
    uint8_t response[DR_DNS_MAX_PACKET_SIZE];
    size_t response_len = 0U;
    size_t opt_offset = 0U;
    uint32_t ip_be = 0U;
    uint32_t min_ttl = 0U;
    char errbuf[128];

    CHECK(dr_dns_parse_query(k_query, sizeof(k_query), &parsed, errbuf, sizeof(errbuf)));
    CHECK(dr_parse_ipv4("11.22.33.44", &ip_be));
    CHECK(dr_dns_build_a_response(k_query, sizeof(k_query), &parsed, ip_be, 120U, response, &response_len));

    write_u16(response + 10U, 1U);
    opt_offset = response_len;
    response[response_len++] = 0x00;
    write_u16(response + response_len, 41U);
    response_len += 2U;
    write_u16(response + response_len, 4096U);
    response_len += 2U;
    write_u32(response + response_len, 0xdeadbeefU);
    response_len += 4U;
    write_u16(response + response_len, 0U);
    response_len += 2U;

    CHECK(dr_dns_collect_ttls(response, response_len, &patches, &min_ttl));
    CHECK(patches.count == 1U);
    CHECK(min_ttl == 120U);
    dr_dns_apply_ttl_patches(response, response_len, &patches, 30U);
    CHECK(read_u32(response + parsed.question_end_offset + 6U) == 90U);
    CHECK(read_u32(response + opt_offset + 5U) == 0xdeadbeefU);
    return 0;
}

int main(void) {
    CHECK(test_parse_normal_query() == 0);
    CHECK(test_parse_root_query() == 0);
    CHECK(test_parse_compressed_query() == 0);
    CHECK(test_parse_rejects_malformed_queries() == 0);
    CHECK(test_build_responses_and_ttl_patches() == 0);
    CHECK(test_compressed_query_responses_are_canonicalized() == 0);
    CHECK(test_collect_ttls_skips_edns_opt() == 0);
    return 0;
}
