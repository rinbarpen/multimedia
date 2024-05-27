#pragma once

#if (defined(_WIN32) || defined(WIN32))
# define __WIN__
#elif (defined(__linux__) || defined(__linux))
# define __LINUX__
#endif

#if (defined(_WIN32) || defined(WIN32)) && !defined(WIN_SOCK_DEFINED)
# define WIN_SOCK_DEFINED
# define WIN32_LEAN_AND_MEAN
# ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#  define _WINSOCK_DEPRECATED_NO_WARNINGS
# endif
# ifndef _CRT_SECURE_NO_WARNINGS
#  define _CRT_SECURE_NO_WARNINGS
# endif
# define FD_SETSIZE 1024

# pragma comment(lib, "Ws2_32.lib")

# include <WinSock2.h>
# include <Windows.h>
# include <WS2tcpip.h>
# include <iphlpapi.h>
# include <Winerror.h>

using sockfd_t = SOCKET;
# define SHUT_RD    0
# define SHUT_WR    1
# define SHUT_RDWR  2

#elif (defined(__linux__) || defined(__linux)) && !defined(LINUX_SOCK_DEFINED)
# define LINUX_SOCK_DEFINED

# include <sys/types.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <sys/epoll.h>
# include <sys/select.h>
# include <netinet/in.h>
# include <netinet/ether.h>
# include <netinet/ip.h>
# include <netpacket/packet.h>
# include <arpa/inet.h>
# include <net/ethernet.h>
# include <net/route.h>
# include <net/if.h>
# include <netdb.h>
# include <unistd.h>
# include <fcntl.h>
# include <errno.h>

using sockfd_t = int;
# define INVALID_SOCKET ~(0)
# define SOCKET_ERROR (-1)

#else
# error "Only support win and linux"
#endif
