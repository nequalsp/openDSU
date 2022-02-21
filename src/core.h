#ifndef DSU_CORE
#define DSU_CORE


#include "state.h"


#define HAVE_MSGHDR_MSG_CONTROL 1   // Look this up. !!!!!


#ifdef DEBUG
#define DSU_DEBUG_PRINT(format, ...) { fprintf(dsu_program_state.logfd, format, ## __VA_ARGS__); fflush(dsu_program_state.logfd);}
#else
#define DSU_DEBUG_PRINT(format, ...)
#endif


#ifdef TEST
#define DSU_TEST_PRINT(format, ...) { fprintf(dsu_program_state.logfd, format, ## __VA_ARGS__); fflush(dsu_program_state.logfd);}
#else
#define DSU_TEST_PRINT(format, ...)
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


#define DSU_COMM "dsu_com"
#define DSU_COMM_LEN 14
#define DSU_READY "dsu_ready"
#define DSU_READY_LEN 16

#define DSU_MAXNUMOFPROC 5


extern struct dsu_state_struct dsu_program_state;


int dsu_request_fd(struct dsu_socket_list *dsu_sockfd);
int dsu_termination_detection();
void dsu_terminate();
int dsu_monitor_init(struct dsu_socket_list *dsu_sockfd);
int dsu_activate_process(void);
int dsu_deactivate_process(void);


extern int (*dsu_socket)(int, int, int);
extern int (*dsu_bind)(int, const struct sockaddr *, socklen_t);
extern int (*dsu_accept)(int, struct sockaddr *restrict, socklen_t *restrict);
extern int (*dsu_accept4)(int, struct sockaddr *restrict, socklen_t *restrict, int);
extern int (*dsu_close)(int);


#endif
