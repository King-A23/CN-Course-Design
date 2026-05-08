#ifndef DNSRELAY_PLATFORM_H
#define DNSRELAY_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET dr_socket_t;
#define DR_INVALID_SOCKET INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef int dr_socket_t;
#define DR_INVALID_SOCKET (-1)
#endif

int dr_platform_init(void);
void dr_platform_cleanup(void);
void dr_close_socket(dr_socket_t sock);
uint64_t dr_now_ms(void);
int dr_set_reuseaddr(dr_socket_t sock);
int dr_parse_ipv4(const char *text, uint32_t *ipv4_be);
void dr_format_sockaddr(const struct sockaddr *addr, socklen_t addr_len, char *buffer, size_t buffer_size);
int dr_sockaddr_equal(const struct sockaddr *lhs, socklen_t lhs_len, const struct sockaddr *rhs, socklen_t rhs_len);
int dr_last_socket_error(void);

#endif
