#include "dnsrelay/dns_packet.h"

#include <stdio.h>
#include <string.h>

static unsigned char ascii_lower(unsigned char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return (unsigned char)(ch + ('a' - 'A'));
    }
    return ch;
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

static int skip_name(const uint8_t *packet, size_t packet_len, size_t offset, size_t *consumed) {
    size_t pos = offset;

    if (packet == NULL || consumed == NULL || offset >= packet_len) {
        return 0;
    }

    while (pos < packet_len) {
        uint8_t label_len = packet[pos];
        if ((label_len & 0xc0U) == 0xc0U) {
            uint16_t pointer;
            if (pos + 1U >= packet_len) {
                return 0;
            }
            pointer = (uint16_t)(((uint16_t)(label_len & 0x3fU) << 8) | packet[pos + 1U]);
            if (pointer >= packet_len) {
                return 0;
            }
            *consumed = pos - offset + 2U;
            return 1;
        }
        if (label_len == 0U) {
            *consumed = pos - offset + 1U;
            return 1;
        }
        if ((label_len & 0xc0U) != 0U || label_len > 63U || pos + 1U + label_len > packet_len) {
            return 0;
        }
        pos += 1U + (size_t)label_len;
    }

    return 0;
}

static int read_name(
    const uint8_t *packet,
    size_t packet_len,
    size_t offset,
    char *output,
    size_t output_size,
    size_t *consumed
) {
    size_t pos = offset;
    size_t written = 0U;
    size_t jumps = 0U;
    int jumped = 0;

    if (packet == NULL || output == NULL || consumed == NULL || output_size == 0U || offset >= packet_len) {
        return 0;
    }

    while (pos < packet_len) {
        uint8_t label_len = packet[pos];

        if ((label_len & 0xc0U) == 0xc0U) {
            uint16_t pointer;
            if (pos + 1U >= packet_len) {
                return 0;
            }
            pointer = (uint16_t)(((uint16_t)(label_len & 0x3fU) << 8) | packet[pos + 1U]);
            if (!jumped) {
                *consumed = pos - offset + 2U;
            }
            if (++jumps > 16U || pointer >= packet_len) {
                return 0;
            }
            pos = pointer;
            jumped = 1;
            continue;
        }

        if (label_len == 0U) {
            if (!jumped) {
                *consumed = pos - offset + 1U;
            }
            output[written] = '\0';
            return written > 0U;
        }

        if ((label_len & 0xc0U) != 0U || label_len > 63U || pos + 1U + label_len > packet_len) {
            return 0;
        }

        if (written != 0U) {
            if (written + 1U >= output_size) {
                return 0;
            }
            output[written++] = '.';
        }

        pos += 1U;
        while (label_len-- > 0U) {
            if (written + 1U >= output_size) {
                return 0;
            }
            output[written++] = (char)ascii_lower(packet[pos++]);
        }
    }

    return 0;
}

static int copy_question(const uint8_t *query, size_t query_len, uint8_t *out, size_t *question_end) {
    DrDnsHeader header;
    size_t qname_consumed = 0U;

    if (out == NULL || question_end == NULL || !dr_dns_parse_header(query, query_len, &header)) {
        return 0;
    }

    *question_end = DR_DNS_HEADER_SIZE;
    if (header.qdcount >= 1U && skip_name(query, query_len, DR_DNS_HEADER_SIZE, &qname_consumed)) {
        if (DR_DNS_HEADER_SIZE + qname_consumed + 4U <= query_len) {
            *question_end = DR_DNS_HEADER_SIZE + qname_consumed + 4U;
        }
    }
    if (*question_end > DR_DNS_MAX_PACKET_SIZE) {
        return 0;
    }

    memcpy(out, query, *question_end);
    return 1;
}

int dr_dns_parse_header(const uint8_t *packet, size_t packet_len, DrDnsHeader *header) {
    if (packet == NULL || header == NULL || packet_len < DR_DNS_HEADER_SIZE) {
        return 0;
    }

    header->id = read_u16(packet);
    header->flags = read_u16(packet + 2);
    header->qdcount = read_u16(packet + 4);
    header->ancount = read_u16(packet + 6);
    header->nscount = read_u16(packet + 8);
    header->arcount = read_u16(packet + 10);
    return 1;
}

int dr_dns_normalize_name(const char *input, char *output, size_t output_size) {
    size_t len;
    size_t index;
    size_t out_len;
    size_t label_len;

    if (input == NULL || output == NULL || output_size == 0U) {
        return 0;
    }

    len = strlen(input);
    while (len > 0U && input[len - 1U] == '.') {
        len -= 1U;
    }
    if (len == 0U || len > DR_DNS_MAX_DOMAIN_LEN || len + 1U > output_size) {
        return 0;
    }

    out_len = 0U;
    label_len = 0U;
    for (index = 0; index < len; ++index) {
        unsigned char ch = (unsigned char)input[index];
        if (ch == '.') {
            if (label_len == 0U) {
                return 0;
            }
            output[out_len++] = '.';
            label_len = 0U;
            continue;
        }
        label_len += 1U;
        if (label_len > 63U) {
            return 0;
        }
        output[out_len++] = (char)ascii_lower(ch);
    }
    if (label_len == 0U) {
        return 0;
    }
    output[out_len] = '\0';
    return 1;
}

uint16_t dr_dns_read_id(const uint8_t *packet, size_t packet_len) {
    return packet != NULL && packet_len >= 2U ? read_u16(packet) : 0U;
}

void dr_dns_write_id(uint8_t *packet, size_t packet_len, uint16_t id) {
    if (packet != NULL && packet_len >= 2U) {
        write_u16(packet, id);
    }
}

int dr_dns_is_response(const uint8_t *packet, size_t packet_len) {
    return packet != NULL && packet_len >= DR_DNS_HEADER_SIZE && (read_u16(packet + 2) & 0x8000U) != 0U;
}

uint8_t dr_dns_get_opcode(const uint8_t *packet, size_t packet_len) {
    return packet != NULL && packet_len >= DR_DNS_HEADER_SIZE ? (uint8_t)((read_u16(packet + 2) >> 11) & 0x0fU) : 0xffU;
}

uint8_t dr_dns_get_rcode(const uint8_t *packet, size_t packet_len) {
    return packet != NULL && packet_len >= DR_DNS_HEADER_SIZE ? (uint8_t)(read_u16(packet + 2) & 0x0fU) : 0xffU;
}

int dr_dns_parse_query(const uint8_t *packet, size_t packet_len, DrParsedQuery *parsed, char *errbuf, size_t errbuf_size) {
    DrDnsHeader header;
    size_t consumed = 0U;
    size_t qtype_offset = 0U;

    if (errbuf != NULL && errbuf_size > 0U) {
        errbuf[0] = '\0';
    }

    if (parsed == NULL) {
        if (errbuf != NULL && errbuf_size > 0U) {
            snprintf(errbuf, errbuf_size, "output query is null");
        }
        return 0;
    }
    memset(parsed, 0, sizeof(*parsed));
    if (!dr_dns_parse_header(packet, packet_len, &header)) {
        if (errbuf != NULL && errbuf_size > 0U) {
            snprintf(errbuf, errbuf_size, "packet too short");
        }
        return 0;
    }

    parsed->id = read_u16(packet);
    parsed->flags = read_u16(packet + 2);
    parsed->opcode = (uint8_t)((parsed->flags >> 11) & 0x0fU);
    parsed->qr = (uint8_t)((parsed->flags >> 15) & 0x01U);

    if (read_u16(packet + 4) != 1U) {
        if (errbuf != NULL && errbuf_size > 0U) {
            snprintf(errbuf, errbuf_size, "expected exactly one question");
        }
        return 0;
    }

    if (!read_name(packet, packet_len, DR_DNS_HEADER_SIZE, parsed->qname, sizeof(parsed->qname), &consumed)) {
        if (errbuf != NULL && errbuf_size > 0U) {
            snprintf(errbuf, errbuf_size, "failed to decode qname");
        }
        return 0;
    }

    qtype_offset = DR_DNS_HEADER_SIZE + consumed;
    if (qtype_offset + 4U > packet_len) {
        if (errbuf != NULL && errbuf_size > 0U) {
            snprintf(errbuf, errbuf_size, "question too short");
        }
        return 0;
    }

    parsed->qtype = read_u16(packet + qtype_offset);
    parsed->qclass = read_u16(packet + qtype_offset + 2U);
    parsed->question_end_offset = qtype_offset + 4U;
    return 1;
}

int dr_dns_build_a_response(
    const uint8_t *query,
    size_t query_len,
    const DrParsedQuery *parsed,
    uint32_t ipv4_be,
    uint32_t ttl,
    uint8_t *out,
    size_t *out_len
) {
    uint16_t flags;
    size_t answer_offset;

    if (out_len != NULL) {
        *out_len = 0U;
    }
    if (query == NULL || parsed == NULL || out == NULL || out_len == NULL ||
        parsed->question_end_offset > query_len ||
        parsed->question_end_offset + 16U > DR_DNS_MAX_PACKET_SIZE) {
        return 0;
    }

    memcpy(out, query, parsed->question_end_offset);
    flags = (uint16_t)(0x8000U | (parsed->flags & 0x0100U) | 0x0080U);
    write_u16(out, parsed->id);
    write_u16(out + 2U, flags);
    write_u16(out + 4U, 1U);
    write_u16(out + 6U, 1U);
    write_u16(out + 8U, 0U);
    write_u16(out + 10U, 0U);

    answer_offset = parsed->question_end_offset;
    write_u16(out + answer_offset, 0xc00cU);
    write_u16(out + answer_offset + 2U, 1U);
    write_u16(out + answer_offset + 4U, 1U);
    write_u32(out + answer_offset + 6U, ttl);
    write_u16(out + answer_offset + 10U, 4U);
    memcpy(out + answer_offset + 12U, &ipv4_be, 4U);
    *out_len = answer_offset + 16U;
    return 1;
}

int dr_dns_build_error_response(
    const uint8_t *query,
    size_t query_len,
    uint8_t rcode,
    uint8_t *out,
    size_t *out_len
) {
    DrDnsHeader header;
    uint16_t flags = 0U;
    size_t question_end = DR_DNS_HEADER_SIZE;
    uint16_t qdcount = 0U;

    if (out_len != NULL) {
        *out_len = 0U;
    }
    if (query == NULL || out == NULL || out_len == NULL) {
        return 0;
    }
    if (!dr_dns_parse_header(query, query_len, &header)) {
        return 0;
    }
    if (!copy_question(query, query_len, out, &question_end)) {
        return 0;
    }
    if (question_end > DR_DNS_HEADER_SIZE) {
        qdcount = 1U;
    }

    flags = (uint16_t)(0x8000U | (header.flags & 0x7800U) | (header.flags & 0x0100U) | 0x0080U | (uint16_t)(rcode & 0x0fU));
    write_u16(out, header.id);
    write_u16(out + 2U, flags);
    write_u16(out + 4U, qdcount);
    write_u16(out + 6U, 0U);
    write_u16(out + 8U, 0U);
    write_u16(out + 10U, 0U);
    *out_len = question_end;
    return 1;
}

int dr_dns_collect_ttls(const uint8_t *packet, size_t packet_len, DrTtlPatchList *patches, uint32_t *min_ttl) {
    DrDnsHeader header;
    size_t offset = DR_DNS_HEADER_SIZE;
    uint32_t total_rrs;
    uint32_t rr_index;
    uint32_t current_min = 0xffffffffU;

    if (min_ttl != NULL) {
        *min_ttl = 0U;
    }
    if (patches == NULL || !dr_dns_parse_header(packet, packet_len, &header)) {
        return 0;
    }

    memset(patches, 0, sizeof(*patches));
    for (rr_index = 0; rr_index < header.qdcount; ++rr_index) {
        size_t consumed = 0U;
        if (!skip_name(packet, packet_len, offset, &consumed) || offset + consumed + 4U > packet_len) {
            return 0;
        }
        offset += consumed + 4U;
    }

    total_rrs = (uint32_t)header.ancount + (uint32_t)header.nscount + (uint32_t)header.arcount;
    for (rr_index = 0; rr_index < total_rrs; ++rr_index) {
        size_t consumed = 0U;
        uint32_t ttl = 0U;
        uint16_t rdlength = 0U;
        uint16_t rr_type = 0U;
        size_t ttl_offset = 0U;

        if (!skip_name(packet, packet_len, offset, &consumed)) {
            return 0;
        }
        if (offset + consumed + 10U > packet_len) {
            return 0;
        }

        rr_type = read_u16(packet + offset + consumed);
        ttl_offset = offset + consumed + 4U;
        ttl = read_u32(packet + ttl_offset);
        rdlength = read_u16(packet + ttl_offset + 4U);

        if (rr_type != 41U) {
            if (patches->count < DR_DNS_MAX_TTL_PATCHES) {
                patches->items[patches->count].ttl_offset = ttl_offset;
                patches->items[patches->count].original_ttl = ttl;
                patches->count += 1U;
            } else {
                return 0;
            }

            if (ttl < current_min) {
                current_min = ttl;
            }
        }
        offset += consumed + 10U + rdlength;
        if (offset > packet_len) {
            return 0;
        }
    }

    if (min_ttl != NULL) {
        *min_ttl = current_min == 0xffffffffU ? 0U : current_min;
    }
    return 1;
}

void dr_dns_apply_ttl_patches(uint8_t *packet, size_t packet_len, const DrTtlPatchList *patches, uint32_t elapsed_sec) {
    size_t index;
    if (packet == NULL || patches == NULL) {
        return;
    }
    for (index = 0; index < patches->count; ++index) {
        uint32_t ttl = patches->items[index].original_ttl;
        if (patches->items[index].ttl_offset + 4U > packet_len) {
            continue;
        }
        if (elapsed_sec >= ttl) {
            ttl = 0U;
        } else {
            ttl -= elapsed_sec;
        }
        write_u32(packet + patches->items[index].ttl_offset, ttl);
    }
}
