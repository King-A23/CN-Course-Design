#ifndef DNSRELAY_DNS_PACKET_H
#define DNSRELAY_DNS_PACKET_H

#include <stddef.h>
#include <stdint.h>

#define DR_DNS_HEADER_SIZE 12
#define DR_DNS_MAX_PACKET_SIZE 512
#define DR_DNS_MAX_DOMAIN_LEN 255
#define DR_DNS_MAX_TTL_PATCHES 64

#define DR_DNS_RCODE_NOERROR 0
#define DR_DNS_RCODE_FORMERR 1
#define DR_DNS_RCODE_SERVFAIL 2
#define DR_DNS_RCODE_NXDOMAIN 3
#define DR_DNS_RCODE_NOTIMP 4
#define DR_DNS_RCODE_REFUSED 5

typedef struct DrDnsHeader {
    uint16_t id; // DNS 事务 ID
    uint16_t flags; // DNS 头部标志位
    uint16_t qdcount; // 问题记录数量
    uint16_t ancount; // 回答记录数量
    uint16_t nscount; // 权威记录数量
    uint16_t arcount; // 附加记录数量
} DrDnsHeader;

typedef struct DrParsedQuery {
    uint16_t id; // 查询报文事务 ID
    uint16_t flags; // 查询报文头部标志位
    uint16_t qtype; // 查询类型
    uint16_t qclass; // 查询类别
    uint8_t opcode; // DNS 操作码
    uint8_t qr; // 查询或响应标记
    char qname[DR_DNS_MAX_DOMAIN_LEN + 1]; // 规范化后的查询域名
    size_t question_end_offset; // 问题段结束位置偏移
} DrParsedQuery;

typedef struct DrTtlPatch {
    size_t ttl_offset; // TTL 字段在报文中的偏移
    uint32_t original_ttl; // 响应报文中的原始 TTL 值
} DrTtlPatch;

typedef struct DrTtlPatchList {
    DrTtlPatch items[DR_DNS_MAX_TTL_PATCHES]; // 需要修正的 TTL 字段数组
    size_t count; // 已记录的 TTL 字段数量
} DrTtlPatchList;

int dr_dns_parse_header(const uint8_t *packet, size_t packet_len, DrDnsHeader *header);
int dr_dns_parse_query(const uint8_t *packet, size_t packet_len, DrParsedQuery *parsed, char *errbuf, size_t errbuf_size);
int dr_dns_normalize_name(const char *input, char *output, size_t output_size);
uint16_t dr_dns_read_id(const uint8_t *packet, size_t packet_len);
void dr_dns_write_id(uint8_t *packet, size_t packet_len, uint16_t id);
int dr_dns_is_response(const uint8_t *packet, size_t packet_len);
uint8_t dr_dns_get_opcode(const uint8_t *packet, size_t packet_len);
uint8_t dr_dns_get_rcode(const uint8_t *packet, size_t packet_len);
int dr_dns_build_a_response(
    const uint8_t *query,
    size_t query_len,
    const DrParsedQuery *parsed,
    uint32_t ipv4_be,
    uint32_t ttl,
    uint8_t *out,
    size_t *out_len
);
int dr_dns_build_error_response(
    const uint8_t *query,
    size_t query_len,
    uint8_t rcode,
    uint8_t *out,
    size_t *out_len
);
int dr_dns_collect_ttls(const uint8_t *packet, size_t packet_len, DrTtlPatchList *patches, uint32_t *min_ttl);
void dr_dns_apply_ttl_patches(uint8_t *packet, size_t packet_len, const DrTtlPatchList *patches, uint32_t elapsed_sec);

#endif
