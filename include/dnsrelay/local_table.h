#ifndef DNSRELAY_LOCAL_TABLE_H
#define DNSRELAY_LOCAL_TABLE_H

#include <stddef.h>
#include <stdint.h>

typedef enum DrLocalLookupKind {
    DR_LOCAL_MISS = 0,
    DR_LOCAL_HIT = 1,
    DR_LOCAL_BLOCKED = 2
} DrLocalLookupKind;

typedef struct DrLocalLookupResult {
    DrLocalLookupKind kind;
    uint32_t ipv4_be;
    uint32_t ttl;
} DrLocalLookupResult;

typedef struct DrLocalEntry {
    char *domain;
    uint32_t ipv4_be;
    int in_use;
} DrLocalEntry;

typedef struct DrLocalTable {
    DrLocalEntry *entries;
    size_t capacity;
    size_t size;
    uint32_t local_ttl;
} DrLocalTable;

int dr_local_table_load(DrLocalTable *table, const char *path, char *errbuf, size_t errbuf_size);
void dr_local_table_free(DrLocalTable *table);
DrLocalLookupResult dr_local_table_lookup(const DrLocalTable *table, const char *qname);
size_t dr_local_table_size(const DrLocalTable *table);

#endif
