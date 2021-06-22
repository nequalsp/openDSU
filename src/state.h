#ifndef DSU_STATE
#define DSU_STATE


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


/*  Linked list for sockets for communication between different versions. */
struct dsu_comfd_struct {
	

    int fd;


    struct dsu_comfd_struct *next;


};


/*  Linked list for sockets that are bind to port the application. */
struct dsu_socket_struct {
    

	int fd;
    int shadowfd;
    int port;
    

    /*  Communication. */
    struct sockaddr_un comfd_addr;
	struct dsu_comfd_struct *comfds;
    int comfd;
    

    /*  Status. */
	int monitoring;
	int version;
	int locked;
	

	/* 	Multi- process & threading. */
	sem_t *status_sem;
	sem_t *fd_sem;	
    int *status;


	struct dsu_socket_struct *next;

	
};


struct dsu_state_struct {

	
	#if DSU_DEBUG == 1
	FILE *logfd;
	#endif  


    /* Binded ports of the application. */
    struct dsu_socket_struct *sockets;
	struct dsu_socket_struct *binds;
    int accepted;

	
	/* Termination information. */
	int live;
	int *workers;
	sem_t *lock;


};


#define dsu_forall_sockets(x, y, ...) { struct dsu_socket_struct *dsu_socket_loop = x;\
                                        while (dsu_socket_loop != NULL) {\
                                            (*y)(dsu_socket_loop, ## __VA_ARGS__);\
                                            dsu_socket_loop = dsu_socket_loop->next;\
                                        }\
                                      }


struct dsu_socket_struct *dsu_sockets_add(struct dsu_socket_struct **head, struct dsu_socket_struct *new_node);


void dsu_sockets_remove_fd(struct dsu_socket_struct **head, int sockfd);


struct dsu_socket_struct *dsu_sockets_transfer_fd(struct dsu_socket_struct **dest, struct dsu_socket_struct **src, struct dsu_socket_struct *dsu_socketfd);


struct dsu_socket_struct *dsu_sockets_search_fd(struct dsu_socket_struct *head, int sockfd);


struct dsu_socket_struct *dsu_sockets_search_port(struct dsu_socket_struct *head, int port);


void dsu_socket_add_comfd(struct dsu_socket_struct *head, int comfd);


void dsu_socket_remove_comfd(struct dsu_socket_struct *head, int comfd);


struct dsu_socket_struct *dsu_sockets_search_comfd(struct dsu_socket_struct *head, int sockfd, int flag);


int dsu_shadowfd(int sockfd);


#endif


