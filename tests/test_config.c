#include "dnsrelay/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define SETENV(name, value) _putenv_s((name), (value))
#define UNSETENV(name) _putenv_s((name), "")
#else
#define SETENV(name, value) setenv((name), (value), 1)
#define UNSETENV(name) unsetenv((name))
#endif

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static int test_help_option(void) {
    DrConfig config;
    char errbuf[128];
    char *help_argv[] = {"dnsrelay", "--help"};
    char *help_extra_argv[] = {"dnsrelay", "-h", "extra"};

    CHECK(dr_config_parse(&config, 2, help_argv, errbuf, sizeof(errbuf)));
    CHECK(config.show_help);

    CHECK(!dr_config_parse(&config, 3, help_extra_argv, errbuf, sizeof(errbuf)));
    CHECK(strstr(errbuf, "help option") != NULL);
    return 0;
}

static int test_port_environment_overrides(void) {
    DrConfig config;

    CHECK(SETENV("DNSRELAY_BIND_PORT", "10053") == 0);
    CHECK(SETENV("DNSRELAY_UPSTREAM_PORT", "1053") == 0);
    dr_config_set_defaults(&config);
    CHECK(config.bind_port == 10053U);
    CHECK(config.upstream_port == 1053U);

    CHECK(SETENV("DNSRELAY_BIND_PORT", "not-a-port") == 0);
    CHECK(SETENV("DNSRELAY_UPSTREAM_PORT", "0") == 0);
    dr_config_set_defaults(&config);
    CHECK(config.bind_port == (uint16_t)DNSRELAY_DEV_PORT);
    CHECK(config.upstream_port == (uint16_t)DR_DEFAULT_UPSTREAM_PORT);

    CHECK(UNSETENV("DNSRELAY_BIND_PORT") == 0);
    CHECK(UNSETENV("DNSRELAY_UPSTREAM_PORT") == 0);
    return 0;
}

int main(void) {
    CHECK(test_help_option() == 0);
    CHECK(test_port_environment_overrides() == 0);
    return 0;
}
