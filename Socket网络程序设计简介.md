# 网络程序设计

## Socket概述

## Socket
- 协议栈实现
  - 传输层以上由用户态应用程序实现
  - 传输层和网络互联层协议在内核中实现（路由协议由用户态进程实现）
  - 第一第二层一般由硬件实现
- UNIX提供给应用程序使用网络功能的方法
  - BSD将设备和通信管道组织成文件方式，创建方式不同，访问方法相同
    - 终端设备
    - 管道
    - 通信服务Socket
  - AT&T UNIX的TLI编程接口
- Socket编程接口面向网络通信，不仅仅用于TCP/IP
  - 利用虚拟loopback接口(127.0.0.1)，可实现同台计算机进程间通信

## TCP与UDP
- TCP
  - 面向连接
  - 可靠
  - 字节流传输
    - 不保证报文边界
- UDP
  - 面向数据报
  - 不可靠
    - 错报，丢报，乱序，流量控制
  - 数据报传输
  - 广播和组播

## 网络字节顺序
- CPU字节顺序
  - Big Endian (大尾)，即将低位字节排在前
    - Power PC，SPARC，Motorola
  - Little Endian (小尾)
    - Intel X86
- 网络字节顺序
  - big endian (大尾) 排序方式 , 与X86相反
- 网络字节转换的库函数
  - htonl ntohl 四字节整数(long)
  - htons ntohs 两字节整数(short)

## socket系统调用
- int socket( int family, int type, int protocol);
  - 创建socket文件描述符，端点名未指定
  - family：协议栈，TCP/IP为AF_INET
  - type：套接字类型
    - 流式虚电路：SOCK_STREAM
    - 数据报：SOCK_DGRAM
    - IP、ICMP：SOCK_RAW
  - protocol：协议（常为0），IPPROTO_TCP、 IPPROTO_UDP
  - 返回值：文件描述符sockfd

## socket系统调用
- int bind( int sockfd, struct sockaddr *name, int namelen)
  - 为文件描述符sockfd设定本地端点名name，也可以用在客户端程序
  - 端口被其他进程占用时，失败返回-1
(附有 sockaddr_in 和 in_addr 的结构体定义代码)

## socket系统调用
- int connect( int sockfd, struct sockaddr *name, size_t name_len);
  - 对应sockfd，设定对端端点名到name
  - TCP执行三次握手，等待对方应答，进程睡眠
  - 连接成功返回0，失败返回-1
  - UDP:无连接建立，对端端点名记录到内核中sockfd对应的数据结构
- int listen( int sockfd, int backlog);
  - 开始监听到达的连接请求，连接到sockfd关联的端点名上
  - backlog：最多的排队请求个数，常设为5
- int accept( int sockfd, struct sockaddr *name, int *namelen);
  - 接受一个连接请求，将远端端点名计入name
  - 依次取得在sockfd上排队的连接请求，建立并返回一个新的文件描述符
  - 若sockfd上无连接请求，accept调用被阻塞
  - TCP第二次握手后accept返回

## socket系统调用
- int read( int sockfd, void *buf, int nbyte);
  int write( int sockfd, void *buf, int nbyte);
  - 在用户缓存buf和文件描述符sockfd对应的内核缓存之间搬送数据
  - read ：读到数据就返回；接收缓存无数据则阻塞
  - write：写完缓存就返回；发送连接忙则阻塞
  - 返回值：实际收到和写出的数据长度；出错返回-1；read返回0，表明连接被对方关闭
- int close( int sockfd);
  - 关闭连接，释放文件描述符
  - 发送缓冲区不为空时：
    - 默认继续发送缓冲区中数据；
    - 系统调用setsockop可以设置sockfd的SO_LINGER选项，（设定时限后）直接丢弃缓冲区数据

## TCP客户-服务器程序

## 客户端程序
- 创建文件描述符socket
- 建立连接connect
  - 进程阻塞，等待TCP连接建立
- 端点名的概念：IP地址+端口号
  - 本地端点名：可动态分配
  - 远端端点名：well-known port
- 发送数据
  - 发送速率大于通信速率，进程会被阻塞
- 关闭连接

## 客户端程序：client.c(1)
(C代码示例: client.c前半部分)

## 客户端程序：client.c(2)
(C代码示例: client.c后半部分)

## 服务端程序
- bind
  - 设定本地端点名
  - 也可以用在客户端程序
- listen
  - 进程不会在此被阻塞，仅仅给内核一个通知
- accept
  - 进程会在这里阻塞等待新连接到来
- 创建新进程时的文件描述符处理
- 问题：不能同时接纳多个连接
  - 多进程并发处理
  - 单进程并发处理

## 服务端程序：server0.c(1)
(C代码示例: server0.c前半部分)

## 服务端程序：server0.c(2)
(C代码示例: server0.c后半部分)
- read在data_sock上无数据时就阻塞，无法响应admin_sock上新连接的到达
- accept在admin_sock上无连接时就阻塞，无法响应data_sock上新数据的到达

## 多进程并发处理

## 多进程并发：server1.c(1)
(C代码示例: server1.c前半部分)

## 多进程并发：server1.c(2)
(C代码示例: server1.c后半部分)

## 多进程并发：server1a.c(1)
(C代码示例: server1a.c)

## 端点名相关的系统调用
- getpeername获取对方的端点名
  getpeername(int sockfd, struct sockaddr *name, int *namelen);
- getsockname获取本地的端点名
  getsockname(int sockfd, struct sockaddr *name, int *namelen);
注意：namelen是传入传出型参数，函数调用之前要先给整数namelen赋值，指出name缓冲区的可用字节数；函数返回时，namelen表示实际在name处写入了多少字节有效数据

## read/write系统调用的语义(1)
- read/write与TCP通信的时序
  主机A用write()通过TCP连接向B发送数据，B接收数据用read()
  (时序图分析)

## read/write系统调用的语义(2)
- read/write与TCP通信故障和流控
  - 流控问题
  - 断线
  - 对方重启动
  - Keepalive(默认两小时)
  - getsockopt/setsockopt可以设置保活间隔，重传次数，重传时间
- “粘连问题”
- read/write与UDP通信
  - 网络故障
  - 没有数据粘连
  - 没有流控功能
  - 不可靠

## read/write的其他版本
- int recv(int sockfd, void *buf, int nbyte, int flags); //加急数据
- int recvfrom(int sockfd, void *buf, int nbyte, int flags, struct sockaddr *from, int *fromlen); //得到对方端点名
- int send(int sockfd, void *buf, int nbyte, int flags);
- int sendto(int sockfd, void *buf, int nbyte, int flags, struct sockaddr *to, int tolen);
- recvfrom/sendto可以指定对方的端点名，常用于UDP
- Winsock只能用recv/send不可用read/write

## shutdown系统调用(1)
- shutdown是通用的套接字上的操作
- 执行后对通信的影响，会与具体的通信协议相关
- TCP协议
  - 允许关闭发送方向的半个连接
  - 没有一种机制让对方关闭它的发送，但TCP协议的流量控制机制，可以通知对方自己的接收窗口为0，对方的write会继续，并将数据堆积在发送缓冲区
- UDP协议
  - UDP关闭接收方向内核仅记下一个标记，不再提供数据，但无法阻止对方的发送而导致的网络上数据
  - 即使套接字关闭也不影响对方发出无人接收的数据报

## shutdown系统调用(2)
- int shutdown(int sockfd, int howto);
  - 禁止发送或接收。socket提供全双工通信，两个方向上都可以收发数据，shutdown提供了对于一个方向的通信控制
- 参数howto取值
  - SHUT_RD：不能再接收数据，随后read均返回0
  - SHUT_WR：不能再发送数据，对方read返回0；本方向再次write会导致SIGPIPE信号
  - SHUT_RDWR：禁止这个sockfd上的任何收发

## socket控制
- Socket控制
  int getsockopt(int sokfd, int level, int optname, void *optval, int *optlen);
  int setsockopt(int sockfd, int level, int optname, void *optval, int optlen);
  int ioctl(int fd, int cmd, void *arg);
- 无阻塞I/O
  #include <sys/fcntl.h>
  int flags;
  flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NDELAY);
  - 发送缓冲区满，write立即以-1返回，errno置为EWOULDBLOCK（或EAGAIN）
  - 发送缓冲区半满，write返回实际发送的字节数
  - 接收缓冲区空，read立即以-1返回，errno置为EWOULDBLOCK

## 单进程并发处理

## select：多路I/O
- 引入select系统调用的原因
  - 使得用户进程可同时等待多个事件发生
  - 用户进程告知内核多个事件，某一个或多个事件发生时select返回，否则，进程睡眠等待
- int select(int maxfdp1, fd_set *rfds, fd_set *wfds, fd_set *efds, struct timeval *timeout);
  - 例如：告知内核在rfds集合{4,5,7}中的任何文件描述符“读准备好”，或在wfds集合{3,7}中的任何文件描述符“写准备好”，或在efds集合{4,5,8}中的任何文件描述符有“异常情况”发生
  - 集合参数是传入传出型，select返回后会被修改，只有准备好的文件描述符，仍出现在集合中
  - 集合参数允许传NULL，表示不关心这方面事件

## select：“准备好”
- 什么叫“准备好”
  - rfds中某文件描述符的read不会阻塞
  - wfds中某文件描述符的write不会阻塞
  - efds中某文件描述符发生了异常情况
    - TCP协议，只有加急数据到达才算“异常情况”
    - 对方连接关闭或网络故障，不算“异常情况”
- “准备好”后可以进行的操作
  - 当“读准备好”时，调用read会立刻返回-1/0/字节数
  - 当“写准备好”时，调用write可以写多少字节？
    - >=1个字节
    - “无阻塞I/O”方式

## 集合操作
预定义数据类型fd_set（在C语言头文件定义）
- void FD_ZERO(fd_set *fds);
  将fds清零：将集合fds设置为“空集”
- void FD_SET(int fd, fd_set *fds);
  向集合fds中加入一个元素fd
- void FD_CLR(int fd, fd_set *fds);
  从集合fds中删除一个元素fd
- int FD_ISSET(int fd, fd_set *fds);
  判断元素fd是否在集合fds内

## select：时间
- select的最后一个参数timeout
  struct timeval {
  long tv_sec; /* 秒 */
  long tv_usec; /* 微秒 */
  };
  - 定时值不为0：select在某一个描述符I/O就绪时立即返回；否则等待但不超过timeout规定的时限
    - 尽管timeout可指定微秒级精度的时间段，依赖于硬件和软件的设定，实际实现一般是10毫秒级别
  - 定时值为0：select立即返回(无阻塞方式查询)
  - 空指针NULL：select等待到至少有一个文件描述符准备好后才返回，否则无限期地等下去

## 单进程并发：server2.c(1)
(C代码示例: server2.c 第一部分)

## 单进程并发：server2.c(2)
(C代码示例: server2.c 第二部分)

## 单进程并发：server2.c(3)
(C代码示例: server2.c 第三部分)

## libevent：开源跨平台轻量级事件管理库
- C语言编写的、轻量级的开源高性能事件通知库
- 事件驱动(event-driven)，高性能
- 跨平台(Windows/Linux/BSD/MacOS)
- 支持多种I/O多路复用技术：select，epoll，kqueue等
- 支持I/O，定时器和信号等事件
- 可作为一些应用的底层的网络库

## UDP通信

## UDP通信程序
- 接收
  - 没有数据到达时，read调用会使得进程睡眠等待
  - 一般需区分数据来自何处，常用recvfrom获得对方的端点名
- 发送
  - 服务器端发送数据常用sendto，指定远端端点名
  - 对接收来的数据作应答，sendto引用的对方端点名利用recvfrom返回得到的端点名
- select定时
  - select可实现同时等待两个事件：收到数据和定时器超时
  - 用time(0)或者gettimeofday()获得时间坐标，计算时间间隔决定是否执行超时后的动作
- 死锁问题

## 客户端udpclient.c
(C代码示例: udpclient.c)

## 客户端程序udpclient.c(2)
- connect
  - 不产生网络流量，内核记下远端端点名
  - 之前未用bind指定本地端点名，系统自动分配本地端点名
- write
  - 使用前面的connect调用指定的端点名
  - UDP不是面向连接的协议，可在sendto参数中指定对方端点名，而且允许对方端点名不同
  - 每次都使用sendto发送数据，前面的connect调用没必要了
  - connect/第一次sendto可使得socket获得系统动态分配的本地端点名，未获得本地端点名之前不该执行read或recv以及recvfrom

## 服务端程序udpserver.c
(C代码示例: udpserver.c)

## 服务端程序udpserver2.c
(C代码示例: udpserver2.c)

## Socket:小节
- 协议栈实现：用户态还是核心态
- 三类特殊文件：管道、终端、socket
- Socket机制不仅用于TCP/IP
- TCP/UDP服务模型
- 网络字节顺序问题
- TCP客户服务器
- 端点名的概念
- 客户端程序的几个调用以及进程状态
- 服务器端程序
- 服务器程序的几个调用
- 多进程并发的文件描述符问题
- inetd的实现方法
- TCP服务端设多个socket原因
- read/write与进程状态，以及与TCP协议操作的时序关系
- Socket感知网络故障的手段与TCP协议的对应关系
- 获取端点名系统调用
- read/write的其他版本
- shutdown：功能以及与TCP协议的关系
- 无阻塞I/O
- 单进程并发处理方法
- 准确理解select调用：什么叫“准备好“
- 集合操作
- select的时间参数
- UDP通信