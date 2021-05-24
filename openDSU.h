#ifndef openDSU
#define openDSU

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

/*  Ordered linked list for sockets that are bind to port the application.  */
struct dsu_socket_struct {
    int sockfd;
    int shadowfd;
    int port;
};

struct dsu_sockets_struct {
    struct dsu_socket_struct value;
    struct dsu_sockets_struct *next;
};

struct dsu_state_struct {
    /* State of application. */
    int version;
	FILE *logfd;    

    /* Binded ports of the application. */
    struct dsu_sockets_struct *sockets;
	struct dsu_sockets_struct *binds;
    struct dsu_sockets_struct *exchanged;
    struct dsu_sockets_struct *committed;
    
    /* Internal communication. */
    int connected;
    int sub_sockfd;    
    int sockfd;
    struct sockaddr_un sockfd_addr;

};

/* Global state variable */
extern struct dsu_state_struct dsu_program_state;

void dsu_init();

int dsu_socket(int domain, int type, int protocol);
int dsu_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int dsu_listen(int sockfd, int backlog);
int dsu_accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);
int dsu_accept4(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen, int flags);
int dsu_close(int sockfd);

int dsu_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int dsu_epoll_create(int size);

#define DSU_INIT dsu_init()

#define socket(domain, type, protocol) dsu_socket(domain, type, protocol)
#define bind(sockfd, addr, addrlen) dsu_bind(sockfd, addr, addrlen)
#define listen(sockfd, backlog) dsu_listen(sockfd, backlog)
#define close(sockfd) dsu_close(sockfd)
#define accept4(sockfd, addr, addrlen, flags) dsu_accept4(sockfd, addr, addrlen, flags)
#define accept(sockfd, addr, addrlen) dsu_accept(sockfd, addr, addrlen)

#define select(nfds, readfds, writefds, exceptfds, timeout) dsu_select(nfds, readfds, writefds, exceptfds, timeout)
#define epoll_create(size) dsu_epoll_create(size)

/*********************************************************/

#endif
