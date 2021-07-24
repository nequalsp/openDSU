#ifndef DSU_CORE
#define DSU_CORE


#include "state.h"
#include "log.h"


#define HAVE_MSGHDR_MSG_CONTROL 1   // Look this up. !!!!!


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
#define DSU_MAXNUMOFPROC 5




extern struct dsu_state_struct dsu_program_state;




int dsu_request_fd(struct dsu_socket_list *dsu_sockfd);
int dsu_termination_detection();
void dsu_terminate();
int dsu_monitor_init(struct dsu_socket_list *dsu_sockfd);
void dsu_monitor_fd(struct dsu_socket_list *dsu_sockfd);
void dsu_configure_socket(struct dsu_socket_list *dsu_sockfd);
void dsu_activate_process(void);
void dsu_configure_process(void);
void dsu_accept_internal_connection(struct dsu_socket_list *dsu_sockfd);
struct dsu_fd_list *dsu_respond_internal_connection(struct dsu_socket_list *dsu_sockfd, struct dsu_fd_list *comfds);


#define DSU_INITIALIZE_EVENT dsu_initialize_event()
void dsu_initialize_event(void);



extern int (*dsu_socket)(int, int, int);
extern int (*dsu_bind)(int, const struct sockaddr *, socklen_t);
extern int (*dsu_listen)(int, int);
extern int (*dsu_accept)(int, struct sockaddr *restrict, socklen_t *restrict);
extern int (*dsu_accept4)(int, struct sockaddr *restrict, socklen_t *restrict, int);
extern int (*dsu_shutdown)(int, int);
extern int (*dsu_close)(int);
extern int (*dsu_fcntl)(int, int, char *);


#endif
