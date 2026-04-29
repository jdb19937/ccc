#ifndef VULCANUS_SYS_SOCKET_H
#define VULCANUS_SYS_SOCKET_H

#include <sys/types.h>

typedef unsigned socklen_t;
typedef unsigned char sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
};

#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_INET   2
#define AF_INET6 30

#define PF_UNSPEC AF_UNSPEC
#define PF_UNIX   AF_UNIX
#define PF_INET   AF_INET

#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_RAW    3

#define SOL_SOCKET 0xffff
#define SO_REUSEADDR 0x0004

#define MSG_OOB       0x1
#define MSG_PEEK      0x2
#define MSG_DONTROUTE 0x4

#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

int     socket(int dom, int tp, int proto);
int     socketpair(int dom, int tp, int proto, int sv[2]);
int     bind(int s, const struct sockaddr *a, socklen_t l);
int     connect(int s, const struct sockaddr *a, socklen_t l);
int     listen(int s, int back);
int     accept(int s, struct sockaddr *a, socklen_t *l);
ssize_t send(int s, const void *b, size_t n, int f);
ssize_t recv(int s, void *b, size_t n, int f);
ssize_t sendto(int s, const void *b, size_t n, int f, const struct sockaddr *a, socklen_t l);
ssize_t recvfrom(int s, void *b, size_t n, int f, struct sockaddr *a, socklen_t *l);
int     shutdown(int s, int how);
int     setsockopt(int s, int lv, int o, const void *v, socklen_t l);
int     getsockopt(int s, int lv, int o, void *v, socklen_t *l);

#endif
