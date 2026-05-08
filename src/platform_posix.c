#ifndef _WIN32

#include "dnsrelay/platform.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

int dr_platform_init(void) {
    return 1;
}

void dr_platform_cleanup(void) {
}

void dr_close_socket(dr_socket_t sock) {
    if (sock != DR_INVALID_SOCKET) {
        close(sock);
    }
}

uint64_t dr_now_ms(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000ULL);
}

int dr_set_reuseaddr(dr_socket_t sock) {
    int value = 1;
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&value, (socklen_t)sizeof(value)) == 0;
}

int dr_parse_ipv4(const char *text, uint32_t *ipv4_be) {
    struct in_addr addr;

    if (inet_pton(AF_INET, text, &addr) != 1) {
        return 0;
    }
    if (ipv4_be != NULL) {
        *ipv4_be = addr.s_addr;
    }
    return 1;
}

void dr_format_sockaddr(const struct sockaddr *addr, socklen_t addr_len, char *buffer, size_t buffer_size) {
    char ip[INET_ADDRSTRLEN] = {0};
    const struct sockaddr_in *ipv4 = NULL;

    (void)addr_len;
    if (buffer_size == 0) {
        return;
    }
    buffer[0] = '\0';

    if (addr == NULL || addr->sa_family != AF_INET) {
        snprintf(buffer, buffer_size, "<unknown>");
        return;
    }

    ipv4 = (const struct sockaddr_in *)addr;
    inet_ntop(AF_INET, &ipv4->sin_addr, ip, sizeof(ip));
    snprintf(buffer, buffer_size, "%s:%u", ip, (unsigned)ntohs(ipv4->sin_port));
}

int dr_sockaddr_equal(const struct sockaddr *lhs, socklen_t lhs_len, const struct sockaddr *rhs, socklen_t rhs_len) {
    const struct sockaddr_in *lhs4 = NULL;
    const struct sockaddr_in *rhs4 = NULL;

    (void)lhs_len;
    (void)rhs_len;
    if (lhs == NULL || rhs == NULL || lhs->sa_family != AF_INET || rhs->sa_family != AF_INET) {
        return 0;
    }

    lhs4 = (const struct sockaddr_in *)lhs;
    rhs4 = (const struct sockaddr_in *)rhs;
    return lhs4->sin_port == rhs4->sin_port && lhs4->sin_addr.s_addr == rhs4->sin_addr.s_addr;
}

int dr_last_socket_error(void) {
    return errno;
}

#else
typedef int dnsrelay_platform_posix_dummy;
#endif
