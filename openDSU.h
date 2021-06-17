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

#define DSU_UNLOCKED 0
#define DSU_LOCKED 1

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


/* Global state variables */
extern struct dsu_state_struct dsu_program_state;

extern int (*dsu_socket)(int, int, int);
extern int (*dsu_bind)(int, const struct sockaddr *, socklen_t);
extern int (*dsu_listen)(int, int);
extern int (*dsu_accept)(int, struct sockaddr *restrict, socklen_t *restrict);
extern int (*dsu_accept4)(int, struct sockaddr *restrict, socklen_t *restrict, int);
extern int (*dsu_close)(int);
extern int (*dsu_select)(int, fd_set *, fd_set *, fd_set *, struct timeval *);


/*********************************************************/

#endif
