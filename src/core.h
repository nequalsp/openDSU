#ifndef DSU_CORE
#define DSU_CORE


#include "state.h"


#ifdef DEBUG
#define DSU_DEBUG 1
#else
#define DSU_DEBUG 0
#endif


#define HAVE_MSGHDR_MSG_CONTROL 1   // Look this up. !!!!!


#if DSU_DEBUG == 1
#define DSU_DEBUG_PRINT(format, ...) {printf(format, ## __VA_ARGS__); fprintf(dsu_program_state.logfd, format, ## __VA_ARGS__); fflush(dsu_program_state.logfd);}
#else
#define DSU_DEBUG_PRINT(format, ...)
#endif


#define DSU_PGID 0
#define DSU_VERSION 1
#define DSU_TRANSFER 2


#define DSU_ACTIVE 0
#define DSU_INACTIVE 1
    

#define DSU_UNLOCKED 0
#define DSU_LOCKED 1


#define DSU_NON_INTERNAL_FD 0
#define DSU_INTERNAL_FD 1
#define DSU_MONITOR_FD 2

//#define DSU_MONITOR 0
//#define DSU_NONMONITOR 1


#define DSU_COMM "dsu_com"
#define DSU_COMM_LEN 14
#define MAXNUMOFPROC 5


#define DSU_LOG "/var/log/dsu"
#define DSU_LOG_LEN 12


extern struct dsu_state_struct dsu_program_state;


int dsu_request_fd(struct dsu_socket_list *dsu_sockfd);
int dsu_termination_detection();
void dsu_terminate();
int dsu_monitor_init(struct dsu_socket_list *dsu_sockfd);
void dsu_monitor_fd(struct dsu_socket_list *dsu_sockfd);


int (*dsu_socket)(int, int, int);
int (*dsu_bind)(int, const struct sockaddr *, socklen_t);
int (*dsu_listen)(int, int);
int (*dsu_accept)(int, struct sockaddr *restrict, socklen_t *restrict);
int (*dsu_accept4)(int, struct sockaddr *restrict, socklen_t *restrict, int);
int (*dsu_close)(int);


#endif
