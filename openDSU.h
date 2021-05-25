#ifndef openDSU
#define openDSU

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>


#ifdef DEBUG
#define DSU_DEBUG 1
#else
#define DSU_DEBUG 0
#endif


#define DSU_RUNNING_VERSION 0
#define DSU_NEW_VERSION 1


#define HAVE_MSGHDR_MSG_CONTROL 1   // Look this up. !!!!!


#define DSU_MSG_SELECT_REQ 1
#define DSU_MSG_SELECT_RES 2
#define DSU_MSG_COMMIT_REQ 3
#define DSU_MSG_COMMIT_RES 4
#define DSU_MSG_EOL_REQ 5
#define DSU_MSG_EOL_RES 6


#define DSU_COMM "\0dsu_comm.unix"
#define DSU_COMM_LEN 14
#define DSU_LOG "/var/log/dsu"
#define DSU_LOG_LEN 12


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
	#if DSU_DEBUG == 1
	FILE *logfd;
	#endif  

    /* Binded ports of the application. */
    struct dsu_sockets_struct *sockets;
	struct dsu_sockets_struct *binds;
    struct dsu_sockets_struct *exchanged;
    struct dsu_sockets_struct *committed;
	struct dsu_sockets_struct *accepted;
    
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
