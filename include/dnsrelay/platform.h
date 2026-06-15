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

// 初始化当前平台的网络运行环境。
int dr_platform_init(void);
// 清理当前平台的网络运行环境。
void dr_platform_cleanup(void);
// 关闭跨平台抽象后的 socket 句柄。
void dr_close_socket(dr_socket_t sock);
// 返回当前毫秒时间。
uint64_t dr_now_ms(void);
// 为 socket 设置地址复用选项。
int dr_set_reuseaddr(dr_socket_t sock);
// 将 IPv4 文本转换为网络字节序整数。
int dr_parse_ipv4(const char *text, uint32_t *ipv4_be);
// 将 sockaddr 格式化为便于日志输出的地址文本。
void dr_format_sockaddr(const struct sockaddr *addr, socklen_t addr_len, char *buffer, size_t buffer_size);
// 比较两个 IPv4 sockaddr 是否表示同一地址和端口。
int dr_sockaddr_equal(const struct sockaddr *lhs, socklen_t lhs_len, const struct sockaddr *rhs, socklen_t rhs_len);
// 返回最近一次 socket 操作的系统错误码。
int dr_last_socket_error(void);

#endif
