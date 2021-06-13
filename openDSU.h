#ifndef openDSU
#define openDSU

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <semaphore.h>

#ifdef DEBUG
#define DSU_DEBUG 1
#else
#define DSU_DEBUG 0
#endif


#define DSU_RUNNING_VERSION 0
#define DSU_NEW_VERSION 1


#define HAVE_MSGHDR_MSG_CONTROL 1   // Look this up. !!!!!


#define DSU_PGID 0
#define DSU_COUNTER 1
#define DSU_STATUS 2

#define DSU_ACTIVE 0
#define DSU_INACTIVE 1
    
#define DSU_LISTEN 0
#define DSU_DUAL 1
#define DSU_TRANSFERED 2
#define DSU_LOCKED 3

#define DSU_COMM "dsu_com"
#define DSU_COMM_LEN 14
#define MAXNUMOFPROC 5

#define DSU_LOG "/var/log/dsu"
#define DSU_LOG_LEN 12


struct dsu_comfd_struct {
    int value;
    struct dsu_comfd_struct *next;
};

/*  Ordered linked list for sockets that are bind to port the application.  */
struct dsu_socket_struct {
    int sockfd;
    int shadowfd;
    int port;
    
    /*  Communication. */
    struct sockaddr_un comfd_addr;
    int comfd;
    struct dsu_comfd_struct *comfds;
    key_t status_key;
    sem_t *sem_id;
    int locked;
    
    /*  Status. */
    
    long *status;
    long internal[3];
};

struct dsu_sockets_struct {
    struct dsu_socket_struct value;
    struct dsu_sockets_struct *next;
};



struct dsu_state_struct {

	#if DSU_DEBUG == 1
	FILE *logfd;
	#endif  

    /* Binded ports of the application. */
    struct dsu_sockets_struct *sockets;
	struct dsu_sockets_struct *binds;
    int binded;
    int accepted;

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

#define DSU_INIT dsu_init()

#define socket(domain, type, protocol) dsu_socket(domain, type, protocol)
#define bind(sockfd, addr, addrlen) dsu_bind(sockfd, addr, addrlen)
#define listen(sockfd, backlog) dsu_listen(sockfd, backlog)
#define close(sockfd) dsu_close(sockfd)
#define accept4(sockfd, addr, addrlen, flags) dsu_accept4(sockfd, addr, addrlen, flags)
#define accept(sockfd, addr, addrlen) dsu_accept(sockfd, addr, addrlen)

#define select(nfds, readfds, writefds, exceptfds, timeout) dsu_select(nfds, readfds, writefds, exceptfds, timeout)

/*********************************************************/

#endif
