#include "dnsrelay/local_table.h"

#include <stdio.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void) {
    DrLocalTable table;
    DrLocalLookupResult blocked;
    DrLocalLookupResult local_hit;
    char errbuf[128];

    CHECK(dr_local_table_load(&table, "dnsrelay.txt", errbuf, sizeof(errbuf)));
    CHECK(dr_local_table_size(&table) > 0U);

    blocked = dr_local_table_lookup(&table, "www.5dsoft.com");
    CHECK(blocked.kind == DR_LOCAL_BLOCKED);

    blocked = dr_local_table_lookup(&table, "WWW.5DSOFT.COM");
    CHECK(blocked.kind == DR_LOCAL_BLOCKED);

    local_hit = dr_local_table_lookup(&table, "test1");
    CHECK(local_hit.kind == DR_LOCAL_HIT);

    local_hit = dr_local_table_lookup(&table, "not-in-file.example");
    CHECK(local_hit.kind == DR_LOCAL_MISS);

    dr_local_table_free(&table);
    return 0;
}
