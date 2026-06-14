#include "dnsrelay/server.h"

#include "dnsrelay/cache.h"
#include "dnsrelay/dns_packet.h"
#include "dnsrelay/local_table.h"
#include "dnsrelay/logger.h"
#include "dnsrelay/pending_map.h"
#include "dnsrelay/platform.h"

#include <stdio.h>
#include <string.h>

typedef struct DrServer {
    DrConfig config;
    dr_socket_t sock;
    struct sockaddr_in bind_addr;
    struct sockaddr_in upstream_addr;
    DrLocalTable local_table;
    DrPendingMap pending_map;
    DrCache cache;
} DrServer;

static int send_packet(dr_socket_t sock, const uint8_t *packet, size_t packet_len, const struct sockaddr *addr, socklen_t addr_len) {
    int sent = sendto(sock, (const char *)packet, (int)packet_len, 0, addr, addr_len);
    return sent == (int)packet_len;
}

static int init_server_socket(DrServer *server) {
    uint32_t bind_ip_be = 0U;

    server->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (server->sock == DR_INVALID_SOCKET) {
        dr_log_error("failed to create UDP socket (err=%d)", dr_last_socket_error());
        return 0;
    }

    dr_set_reuseaddr(server->sock);

    memset(&server->bind_addr, 0, sizeof(server->bind_addr));
    server->bind_addr.sin_family = AF_INET;
    server->bind_addr.sin_port = htons(server->config.bind_port);
    if (!dr_parse_ipv4(server->config.bind_ip, &bind_ip_be)) {
        dr_log_error("invalid bind IPv4 address: %s", server->config.bind_ip);
        return 0;
    }
    server->bind_addr.sin_addr.s_addr = bind_ip_be;

    if (bind(server->sock, (const struct sockaddr *)&server->bind_addr, (socklen_t)sizeof(server->bind_addr)) != 0) {
        dr_log_error(
            "failed to bind %s:%u (err=%d)",
            server->config.bind_ip,
            (unsigned)server->config.bind_port,
            dr_last_socket_error()
        );
        return 0;
    }

    return 1;
}

static int init_upstream_addr(DrServer *server) {
    uint32_t upstream_ip_be = 0U;

    memset(&server->upstream_addr, 0, sizeof(server->upstream_addr));
    server->upstream_addr.sin_family = AF_INET;
    server->upstream_addr.sin_port = htons(server->config.upstream_port);

    if (!dr_parse_ipv4(server->config.upstream_ip, &upstream_ip_be)) {
        dr_log_error("invalid upstream IPv4 address: %s", server->config.upstream_ip);
        return 0;
    }
    server->upstream_addr.sin_addr.s_addr = upstream_ip_be;
    return 1;
}

static int maybe_cache_response(DrServer *server, const DrPendingRequest *request, const uint8_t *packet, size_t packet_len, uint64_t now_ms) {
    DrTtlPatchList patches;
    uint32_t min_ttl = 0U;

    if (dr_dns_get_rcode(packet, packet_len) != DR_DNS_RCODE_NOERROR) {
        return 0;
    }
    if (!dr_dns_collect_ttls(packet, packet_len, &patches, &min_ttl)) {
        return 0;
    }
    if (patches.count == 0U || min_ttl == 0U) {
        return 0;
    }
    return dr_cache_put(
        &server->cache,
        request->qname,
        request->qtype,
        request->qclass,
        packet,
        packet_len,
        &patches,
        min_ttl,
        now_ms
    );
}

static void log_query_basic(const struct sockaddr *client_addr, socklen_t client_addr_len, const DrParsedQuery *query) {
    char client_text[64];
    dr_format_sockaddr(client_addr, client_addr_len, client_text, sizeof(client_text));
    dr_log_basic("client=%s id=%u qname=%s qtype=%u qclass=%u", client_text, query->id, query->qname, query->qtype, query->qclass);
}

static int respond_with_error(
    DrServer *server,
    const uint8_t *query_packet,
    size_t query_len,
    uint8_t rcode,
    const struct sockaddr *client_addr,
    socklen_t client_addr_len
) {
    uint8_t response[DR_DNS_MAX_PACKET_SIZE];
    size_t response_len = 0U;
    if (!dr_dns_build_error_response(query_packet, query_len, rcode, response, &response_len)) {
        return 0;
    }
    return send_packet(server->sock, response, response_len, client_addr, client_addr_len);
}

static int handle_client_packet(
    DrServer *server,
    const uint8_t *packet,
    size_t packet_len,
    const struct sockaddr *client_addr,
    socklen_t client_addr_len,
    uint64_t now_ms
) {
    DrDnsHeader header;
    DrParsedQuery parsed;
    DrLocalLookupResult local;
    uint8_t response[DR_DNS_MAX_PACKET_SIZE];
    size_t response_len = 0U;
    uint16_t upstream_id = 0U;
    char errbuf[128];

    if (!dr_dns_parse_header(packet, packet_len, &header)) {
        dr_log_verbose("drop malformed packet shorter than DNS header");
        return 1;
    }

    if ((header.flags & 0x8000U) != 0U) {
        dr_log_verbose("ignore unexpected response packet from client side");
        return 1;
    }

    if (header.qdcount != 1U) {
        dr_log_verbose("reply FORMERR because qdcount=%u", header.qdcount);
        return respond_with_error(server, packet, packet_len, DR_DNS_RCODE_FORMERR, client_addr, client_addr_len);
    }

    if (dr_dns_get_opcode(packet, packet_len) != 0U) {
        dr_log_verbose("reply NOTIMP because opcode=%u", dr_dns_get_opcode(packet, packet_len));
        return respond_with_error(server, packet, packet_len, DR_DNS_RCODE_NOTIMP, client_addr, client_addr_len);
    }

    if (!dr_dns_parse_query(packet, packet_len, &parsed, errbuf, sizeof(errbuf))) {
        dr_log_verbose("reply FORMERR because parse failed: %s", errbuf);
        return respond_with_error(server, packet, packet_len, DR_DNS_RCODE_FORMERR, client_addr, client_addr_len);
    }

    log_query_basic(client_addr, client_addr_len, &parsed);

    if (parsed.qclass == 1U) {
        local = dr_local_table_lookup(&server->local_table, parsed.qname);
        if (local.kind == DR_LOCAL_BLOCKED) {
            if (!dr_dns_build_error_response(packet, packet_len, DR_DNS_RCODE_NXDOMAIN, response, &response_len)) {
                return 0;
            }
            dr_log_verbose("blocked domain %s", parsed.qname);
            return send_packet(server->sock, response, response_len, client_addr, client_addr_len);
        }
        if (parsed.qtype == 1U && local.kind == DR_LOCAL_HIT) {
            if (!dr_dns_build_a_response(packet, packet_len, &parsed, local.ipv4_be, local.ttl, response, &response_len)) {
                return 0;
            }
            dr_log_verbose("local hit for %s", parsed.qname);
            return send_packet(server->sock, response, response_len, client_addr, client_addr_len);
        }
    }

    if (dr_cache_lookup(
            &server->cache,
            parsed.qname,
            parsed.qtype,
            parsed.qclass,
            now_ms,
            parsed.id,
            response,
            &response_len)) {
        dr_log_verbose("cache hit for %s", parsed.qname);
        return send_packet(server->sock, response, response_len, client_addr, client_addr_len);
    }

    if (!dr_pending_map_insert(
            &server->pending_map,
            parsed.id,
            client_addr,
            client_addr_len,
            packet,
            packet_len,
            parsed.qname,
            parsed.qtype,
            parsed.qclass,
            now_ms,
            &upstream_id)) {
        dr_log_error("reply REFUSED because pending map insert failed for %s", parsed.qname);
        return respond_with_error(server, packet, packet_len, DR_DNS_RCODE_REFUSED, client_addr, client_addr_len);
    }

    memcpy(response, packet, packet_len);
    dr_dns_write_id(response, packet_len, upstream_id);
    if (!send_packet(server->sock, response, packet_len, (const struct sockaddr *)&server->upstream_addr, (socklen_t)sizeof(server->upstream_addr))) {
        dr_pending_map_remove(&server->pending_map, upstream_id, NULL);
        dr_log_error("failed to forward query for %s to upstream", parsed.qname);
        return 0;
    }

    dr_log_verbose("forwarded %s with upstream id=%u", parsed.qname, upstream_id);
    return 1;
}

static int handle_upstream_packet(DrServer *server, uint8_t *packet, size_t packet_len, uint64_t now_ms) {
    uint16_t upstream_id = dr_dns_read_id(packet, packet_len);
    DrPendingRequest *pending = NULL;
    DrPendingRequest request;
    DrParsedQuery response_query;
    char errbuf[128];

    pending = dr_pending_map_get(&server->pending_map, upstream_id);
    if (pending == NULL) {
        dr_log_verbose("drop late or unknown upstream response id=%u", upstream_id);
        return 1;
    }

    if (!dr_dns_parse_query(packet, packet_len, &response_query, errbuf, sizeof(errbuf))) {
        dr_log_verbose("drop upstream response id=%u because question parse failed: %s", upstream_id, errbuf);
        return 1;
    }
    if (strcmp(response_query.qname, pending->qname) != 0 ||
        response_query.qtype != pending->qtype ||
        response_query.qclass != pending->qclass) {
        dr_log_verbose(
            "drop upstream response id=%u because question mismatch: got %s/%u/%u expected %s/%u/%u",
            upstream_id,
            response_query.qname,
            response_query.qtype,
            response_query.qclass,
            pending->qname,
            pending->qtype,
            pending->qclass
        );
        return 1;
    }

    if (!dr_pending_map_remove(&server->pending_map, upstream_id, &request)) {
        return 1;
    }
    maybe_cache_response(server, &request, packet, packet_len, now_ms);
    dr_dns_write_id(packet, packet_len, request.client_id);
    dr_log_verbose("reply upstream response for %s to client id=%u", request.qname, request.client_id);
    return send_packet(
        server->sock,
        packet,
        packet_len,
        (const struct sockaddr *)&request.client_addr,
        request.client_addr_len
    );
}

int dr_server_run(const DrConfig *config) {
    DrServer server;
    int status = 1;

    memset(&server, 0, sizeof(server));
    server.config = *config;
    server.sock = DR_INVALID_SOCKET;

    if (!dr_platform_init()) {
        dr_log_error("platform initialization failed");
        return 1;
    }
    if (!init_upstream_addr(&server)) {
        status = 1;
        goto cleanup;
    }
    if (!dr_local_table_load(&server.local_table, server.config.table_path, NULL, 0U)) {
        dr_log_error("failed to load local table from %s", server.config.table_path);
        status = 1;
        goto cleanup;
    }
    if (!dr_pending_map_init(&server.pending_map)) {
        dr_log_error("failed to allocate pending map");
        status = 1;
        goto cleanup;
    }
    if (!dr_cache_init(&server.cache, server.config.cache_capacity)) {
        dr_log_error("failed to allocate cache");
        status = 1;
        goto cleanup;
    }
    if (!init_server_socket(&server)) {
        status = 1;
        goto cleanup;
    }

    dr_log_basic(
        "dnsrelay started bind=%s:%u upstream=%s:%u table=%s entries=%u cache=%u",
        server.config.bind_ip,
        (unsigned)server.config.bind_port,
        server.config.upstream_ip,
        (unsigned)server.config.upstream_port,
        server.config.table_path,
        (unsigned)dr_local_table_size(&server.local_table),
        (unsigned)server.config.cache_capacity
    );

    for (;;) {
        fd_set readfds;
        struct timeval timeout;
        struct sockaddr_storage src_addr;
        socklen_t src_addr_len = (socklen_t)sizeof(src_addr);
        uint8_t packet[DR_DNS_MAX_PACKET_SIZE];
        int ready;
        int received;
        uint64_t now_ms;
        size_t expired_count = 0U;
        DrPendingRequest expired_request;

        FD_ZERO(&readfds);
        FD_SET(server.sock, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;

        ready = select((int)server.sock + 1, &readfds, NULL, NULL, &timeout);
        now_ms = dr_now_ms();
        while (dr_pending_map_pop_expired(
                &server.pending_map,
                now_ms,
                server.config.upstream_timeout_ms,
                &expired_request)) {
            expired_count += 1U;
            if (!respond_with_error(
                    &server,
                    expired_request.query_packet,
                    expired_request.query_len,
                    DR_DNS_RCODE_SERVFAIL,
                    (const struct sockaddr *)&expired_request.client_addr,
                    expired_request.client_addr_len)) {
                dr_log_error("failed to reply SERVFAIL for timed out upstream request %s", expired_request.qname);
            }
        }
        dr_cache_expire(&server.cache, now_ms);
        if (expired_count > 0U) {
            dr_log_verbose("expired %u upstream requests", (unsigned)expired_count);
        }

        if (ready < 0) {
            dr_log_error("select failed (err=%d)", dr_last_socket_error());
            status = 1;
            break;
        }
        if (ready == 0) {
            continue;
        }

        received = recvfrom(
            server.sock,
            (char *)packet,
            (int)sizeof(packet),
            0,
            (struct sockaddr *)&src_addr,
            &src_addr_len
        );
        if (received <= 0) {
            dr_log_error("recvfrom failed (err=%d)", dr_last_socket_error());
            continue;
        }

        now_ms = dr_now_ms();
        if (dr_dns_is_response(packet, (size_t)received) &&
            dr_sockaddr_equal(
                (const struct sockaddr *)&src_addr,
                src_addr_len,
                (const struct sockaddr *)&server.upstream_addr,
                (socklen_t)sizeof(server.upstream_addr))) {
            handle_upstream_packet(&server, packet, (size_t)received, now_ms);
        } else {
            handle_client_packet(&server, packet, (size_t)received, (const struct sockaddr *)&src_addr, src_addr_len, now_ms);
        }
    }

cleanup:
    dr_close_socket(server.sock);
    dr_cache_free(&server.cache);
    dr_pending_map_free(&server.pending_map);
    dr_local_table_free(&server.local_table);
    dr_platform_cleanup();
    return status;
}
