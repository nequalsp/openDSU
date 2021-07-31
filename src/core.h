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


#define DSU_COMM "dsu_com"
#define DSU_COMM_LEN 14
#define DSU_MAXNUMOFPROC 5




extern struct dsu_state_struct dsu_program_state;


void dsu_activate_process(void);
#define DSU_DEACTIVATE dsu_deactivate_process()
void dsu_deactivate_process(void);
#define DSU_TERMINATION dsu_termination()
void dsu_termination(void);


void dsu_send_fd(struct dsu_socket_list *dsu_sockfd);
void dsu_handle_state(struct dsu_socket_list *dsu_sockfd);
#define DSU_PRE_EVENT dsu_pre_event()
void dsu_pre_event(void);


extern int (*dsu_bind)(int, const struct sockaddr *, socklen_t);
extern int (*dsu_close)(int);



#endif
