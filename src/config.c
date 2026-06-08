#include "dnsrelay/config.h"

#include "dnsrelay/platform.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_text(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) {
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static int parse_port_text(const char *text, uint16_t *port) {
    char *end = NULL;
    unsigned long value;

    if (text == NULL || *text == '\0') {
        return 0;
    }

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0UL || value > 65535UL) {
        return 0;
    }

    if (port != NULL) {
        *port = (uint16_t)value;
    }
    return 1;
}

static void apply_port_env(DrConfig *config) {
    uint16_t port;
    const char *bind_port = getenv("DNSRELAY_BIND_PORT");
    const char *upstream_port = getenv("DNSRELAY_UPSTREAM_PORT");

    if (parse_port_text(bind_port, &port)) {
        config->bind_port = port;
    }
    if (parse_port_text(upstream_port, &port)) {
        config->upstream_port = port;
    }
}

void dr_config_set_defaults(DrConfig *config) {
    memset(config, 0, sizeof(*config));
    config->debug_level = DR_LOG_NONE;
    copy_text(config->upstream_ip, sizeof(config->upstream_ip), DR_DEFAULT_UPSTREAM_IP);
    config->upstream_port = (uint16_t)DR_DEFAULT_UPSTREAM_PORT;
    copy_text(config->table_path, sizeof(config->table_path), DR_DEFAULT_TABLE_FILE);
    copy_text(config->bind_ip, sizeof(config->bind_ip), DR_DEFAULT_BIND_IP);
    config->bind_port = (uint16_t)DNSRELAY_DEV_PORT;
    config->cache_capacity = DR_DEFAULT_CACHE_CAPACITY;
    config->upstream_timeout_ms = DR_DEFAULT_UPSTREAM_TIMEOUT_MS;
    apply_port_env(config);
}

static int set_error(char *errbuf, size_t errbuf_size, const char *message) {
    if (errbuf != NULL && errbuf_size > 0) {
        copy_text(errbuf, errbuf_size, message);
    }
    return 0;
}

int dr_config_parse(DrConfig *config, int argc, char **argv, char *errbuf, size_t errbuf_size) {
    int index = 1;
    uint32_t parsed_ipv4 = 0;

    dr_config_set_defaults(config);

    if (index < argc && (strcmp(argv[index], "-h") == 0 || strcmp(argv[index], "--help") == 0)) {
        config->show_help = 1;
        index += 1;
    }

    if (config->show_help) {
        if (index != argc) {
            return set_error(errbuf, errbuf_size, "help option does not take extra arguments");
        }
        return 1;
    }

    if (index < argc && strcmp(argv[index], "-d") == 0) {
        config->debug_level = DR_LOG_BASIC;
        index += 1;
    } else if (index < argc && strcmp(argv[index], "-dd") == 0) {
        config->debug_level = DR_LOG_VERBOSE;
        index += 1;
    }

    if (index < argc) {
        if (!dr_parse_ipv4(argv[index], &parsed_ipv4)) {
            return set_error(errbuf, errbuf_size, "invalid upstream DNS IPv4 address");
        }
        copy_text(config->upstream_ip, sizeof(config->upstream_ip), argv[index]);
        index += 1;
    }

    if (index < argc) {
        copy_text(config->table_path, sizeof(config->table_path), argv[index]);
        index += 1;
    }

    if (index != argc) {
        return set_error(errbuf, errbuf_size, "too many arguments");
    }

    return 1;
}

void dr_config_print_usage(const char *program_name) {
    printf("Usage: %s [-d | -dd] [dns-server-ipaddr] [filename]\n", program_name);
    printf("Defaults:\n");
    printf("  upstream DNS : %s\n", DR_DEFAULT_UPSTREAM_IP);
    printf("  table file   : %s\n", DR_DEFAULT_TABLE_FILE);
    printf("  bind address : %s:%u\n", DR_DEFAULT_BIND_IP, (unsigned)DNSRELAY_DEV_PORT);
}
