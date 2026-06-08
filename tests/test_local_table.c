#include "dnsrelay/local_table.h"

#include "dnsrelay/platform.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static int write_test_table(const char *path) {
    FILE *file = fopen(path, "w");
    unsigned index;

    if (file == NULL) {
        return 0;
    }

    fprintf(file, "# comment line\n");
    fprintf(file, "; another comment line\n");
    fprintf(file, "bad-line-without-domain\n");
    fprintf(file, "not-an-ip invalid.example\n");
    fprintf(file, "192.0.2.10 Example.COM.\n");
    fprintf(file, "0.0.0.0 Blocked.EXAMPLE\n");
    fprintf(file, "192.0.2.1 duplicate.example\n");
    fprintf(file, "198.51.100.77 DUPLICATE.example\n");
    fprintf(file, "192.0.2.33 invalid..name\n");
    for (index = 0; index < 650U; ++index) {
        fprintf(file, "203.0.%u.%u bulk%03u.example\n", index / 250U, index % 250U + 1U, index);
    }

    fclose(file);
    return 1;
}

static int test_repository_table_still_loads(void) {
    DrLocalTable table;
    DrLocalLookupResult blocked;
    DrLocalLookupResult local_hit;
    char errbuf[128];

    CHECK(dr_local_table_load(&table, "dnsrelay.txt", errbuf, sizeof(errbuf)));
    CHECK(dr_local_table_size(&table) > 0U);

    blocked = dr_local_table_lookup(&table, "www.5dsoft.com");
    CHECK(blocked.kind == DR_LOCAL_BLOCKED);

    blocked = dr_local_table_lookup(&table, "WWW.5DSOFT.COM.");
    CHECK(blocked.kind == DR_LOCAL_BLOCKED);

    local_hit = dr_local_table_lookup(&table, "test1");
    CHECK(local_hit.kind == DR_LOCAL_HIT);

    local_hit = dr_local_table_lookup(&table, "not-in-file.example");
    CHECK(local_hit.kind == DR_LOCAL_MISS);

    dr_local_table_free(&table);
    return 0;
}

static int test_custom_table_rules_and_growth(void) {
    const char *path = "test_local_table_tmp.txt";
    DrLocalTable table;
    DrLocalLookupResult result;
    uint32_t expected_ip = 0U;
    char errbuf[128];

    remove(path);
    CHECK(write_test_table(path));
    CHECK(dr_local_table_load(&table, path, errbuf, sizeof(errbuf)));
    CHECK(dr_local_table_size(&table) == 653U);

    result = dr_local_table_lookup(&table, "example.com.");
    CHECK(result.kind == DR_LOCAL_HIT);
    CHECK(result.ttl == 60U);

    result = dr_local_table_lookup(&table, "BLOCKED.example");
    CHECK(result.kind == DR_LOCAL_BLOCKED);
    CHECK(result.ipv4_be == 0U);

    CHECK(dr_parse_ipv4("198.51.100.77", &expected_ip));
    result = dr_local_table_lookup(&table, "duplicate.example");
    CHECK(result.kind == DR_LOCAL_HIT);
    CHECK(result.ipv4_be == expected_ip);

    result = dr_local_table_lookup(&table, "bulk000.example");
    CHECK(result.kind == DR_LOCAL_HIT);
    result = dr_local_table_lookup(&table, "BULK649.EXAMPLE.");
    CHECK(result.kind == DR_LOCAL_HIT);
    result = dr_local_table_lookup(&table, "bulk650.example");
    CHECK(result.kind == DR_LOCAL_MISS);
    result = dr_local_table_lookup(&table, "invalid..name");
    CHECK(result.kind == DR_LOCAL_MISS);

    dr_local_table_free(&table);
    remove(path);
    return 0;
}

static int test_invalid_inputs_are_safe(void) {
    DrLocalLookupResult result;
    char errbuf[128];

    CHECK(!dr_local_table_load(NULL, "dnsrelay.txt", errbuf, sizeof(errbuf)));
    CHECK(!dr_local_table_load(&(DrLocalTable){0}, NULL, errbuf, sizeof(errbuf)));

    result = dr_local_table_lookup(NULL, "example.com");
    CHECK(result.kind == DR_LOCAL_MISS);
    result = dr_local_table_lookup(NULL, NULL);
    CHECK(result.kind == DR_LOCAL_MISS);
    return 0;
}

int main(void) {
    CHECK(test_repository_table_still_loads() == 0);
    CHECK(test_custom_table_rules_and_growth() == 0);
    CHECK(test_invalid_inputs_are_safe() == 0);
    return 0;
}
