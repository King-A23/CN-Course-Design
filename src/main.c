#include "dnsrelay/config.h"
#include "dnsrelay/logger.h"
#include "dnsrelay/server.h"

#include <stdio.h>

int main(int argc, char **argv) {
    DrConfig config;
    char errbuf[128];

    if (!dr_config_parse(&config, argc, argv, errbuf, sizeof(errbuf))) {
        fprintf(stderr, "Argument error: %s\n", errbuf);
        dr_config_print_usage(argv[0]);
        return 1;
    }

    dr_logger_init(config.debug_level);
    return dr_server_run(&config);
}
