#include "dnsrelay/local_table.h"

#include "dnsrelay/dns_packet.h"
#include "dnsrelay/platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DR_LOCAL_TABLE_TTL 60U
#define DR_LOCAL_TABLE_INITIAL_CAPACITY 512U

static uint32_t hash_name(const char *text) {
    uint32_t value = 2166136261u;
    const unsigned char *cursor = (const unsigned char *)text;
    while (*cursor != '\0') {
        value ^= (uint32_t)(*cursor++);
        value *= 16777619u;
    }
    return value;
}

static char *dup_text(const char *text) {
    size_t len = strlen(text);
    char *copy = (char *)malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, len + 1);
    return copy;
}

static void clear_entries(DrLocalEntry *entries, size_t capacity) {
    size_t index;
    for (index = 0; index < capacity; ++index) {
        entries[index].domain = NULL;
        entries[index].ipv4_be = 0;
        entries[index].in_use = 0;
    }
}

static int ensure_table_ready(DrLocalTable *table) {
    if (table->entries != NULL) {
        return 1;
    }
    table->entries = (DrLocalEntry *)malloc(sizeof(DrLocalEntry) * DR_LOCAL_TABLE_INITIAL_CAPACITY);
    if (table->entries == NULL) {
        return 0;
    }
    table->capacity = DR_LOCAL_TABLE_INITIAL_CAPACITY;
    table->size = 0;
    table->local_ttl = DR_LOCAL_TABLE_TTL;
    clear_entries(table->entries, table->capacity);
    return 1;
}

static int insert_entry(DrLocalTable *table, const char *domain, uint32_t ipv4_be) {
    size_t mask;
    size_t slot;

    if (!ensure_table_ready(table)) {
        return 0;
    }

    mask = table->capacity - 1;
    slot = (size_t)(hash_name(domain) & (uint32_t)mask);

    while (table->entries[slot].in_use) {
        if (strcmp(table->entries[slot].domain, domain) == 0) {
            table->entries[slot].ipv4_be = ipv4_be;
            return 1;
        }
        slot = (slot + 1U) & mask;
    }

    table->entries[slot].domain = dup_text(domain);
    if (table->entries[slot].domain == NULL) {
        return 0;
    }
    table->entries[slot].ipv4_be = ipv4_be;
    table->entries[slot].in_use = 1;
    table->size += 1U;
    return 1;
}

int dr_local_table_load(DrLocalTable *table, const char *path, char *errbuf, size_t errbuf_size) {
    FILE *file = NULL;
    char line[1024];

    memset(table, 0, sizeof(*table));

    file = fopen(path, "r");
    if (file == NULL) {
        if (errbuf != NULL && errbuf_size > 0) {
            snprintf(errbuf, errbuf_size, "failed to open local table: %s", path);
        }
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char normalized[DR_DNS_MAX_DOMAIN_LEN + 1];
        char *ip_text = NULL;
        char *domain_text = NULL;
        uint32_t ipv4_be = 0;
        char *cursor = line;

        while (*cursor == ' ' || *cursor == '\t') {
            ++cursor;
        }
        if (*cursor == '\0' || *cursor == '\n' || *cursor == '\r' || *cursor == ';' || *cursor == '#') {
            continue;
        }

        ip_text = strtok(cursor, " \t\r\n");
        domain_text = strtok(NULL, " \t\r\n");
        if (ip_text == NULL || domain_text == NULL) {
            continue;
        }
        if (!dr_parse_ipv4(ip_text, &ipv4_be)) {
            continue;
        }
        if (!dr_dns_normalize_name(domain_text, normalized, sizeof(normalized))) {
            continue;
        }
        if (!insert_entry(table, normalized, ipv4_be)) {
            fclose(file);
            if (errbuf != NULL && errbuf_size > 0) {
                snprintf(errbuf, errbuf_size, "failed to insert local table entry");
            }
            dr_local_table_free(table);
            return 0;
        }
    }

    fclose(file);
    return 1;
}

void dr_local_table_free(DrLocalTable *table) {
    size_t index;

    if (table == NULL || table->entries == NULL) {
        return;
    }
    for (index = 0; index < table->capacity; ++index) {
        free(table->entries[index].domain);
    }
    free(table->entries);
    memset(table, 0, sizeof(*table));
}

DrLocalLookupResult dr_local_table_lookup(const DrLocalTable *table, const char *qname) {
    DrLocalLookupResult result;
    char normalized[DR_DNS_MAX_DOMAIN_LEN + 1];
    size_t mask;
    size_t slot;

    result.kind = DR_LOCAL_MISS;
    result.ipv4_be = 0;
    result.ttl = table != NULL ? table->local_ttl : DR_LOCAL_TABLE_TTL;

    if (table == NULL || table->entries == NULL || table->capacity == 0) {
        return result;
    }
    if (!dr_dns_normalize_name(qname, normalized, sizeof(normalized))) {
        return result;
    }

    mask = table->capacity - 1;
    slot = (size_t)(hash_name(normalized) & (uint32_t)mask);

    while (table->entries[slot].in_use) {
        if (strcmp(table->entries[slot].domain, normalized) == 0) {
            result.ipv4_be = table->entries[slot].ipv4_be;
            result.kind = result.ipv4_be == 0 ? DR_LOCAL_BLOCKED : DR_LOCAL_HIT;
            return result;
        }
        slot = (slot + 1U) & mask;
    }

    return result;
}

size_t dr_local_table_size(const DrLocalTable *table) {
    return table != NULL ? table->size : 0U;
}
