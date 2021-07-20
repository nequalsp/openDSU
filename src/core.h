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
#define DSU_DEBUG_PRINT(format, ...) { fprintf(dsu_program_state.logfd, format, ## __VA_ARGS__); fflush(dsu_program_state.logfd);}
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


#define DSU_COMM "dsu_com"
#define DSU_COMM_LEN 14
#define DSU_MAXNUMOFPROC 5


#define DSU_LOG "/var/log/dsu"
#define DSU_LOG_LEN 12


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
extern int (*dsu_ioctl)(int, unsigned long, char *);
extern int (*dsu_getsockopt)(int, int, int, void *restrict, socklen_t *restrict);
extern int (*dsu_setsockopt)(int, int, int, const void *, socklen_t);
extern int (*dsu_getsockname)(int, struct sockaddr *restrict, socklen_t *restrict);
extern int (*dsu_getpeername)(int, struct sockaddr *restrict, socklen_t *restrict);
extern ssize_t (*dsu_read)(int, void *, size_t);
extern ssize_t (*dsu_recv)(int, void *, size_t, int);
extern ssize_t (*dsu_recvfrom)(int, void *restrict, size_t, int, struct sockaddr *restrict, socklen_t *restrict);
extern ssize_t (*dsu_recvmsg)(int, struct msghdr *, int);
extern ssize_t (*dsu_write)(int, const void *, size_t);
extern ssize_t (*dsu_send)(int, const void *, size_t, int);
extern ssize_t (*dsu_sendto)(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
extern ssize_t (*dsu_sendmsg)(int, const struct msghdr *, int);
extern int (*dsu_fcntl)(int, int, char *);
extern int (*dsu_ioctl)(int, unsigned long, char *);


#endif
