#ifndef DSU_STATE
#define DSU_STATE


#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <semaphore.h>
#include <sys/epoll.h>


#include "log.h"


/*  Linked list for shadow datastructures of the file descriptors. */
struct dsu_socket_list {
    

	int fd;   
	int port;
    struct sockaddr_un comfd_addr;  // Required for accepting internal connections.
	int comfd;						// File descriptor for listening for internal connections.
    
	
    /*  Status. */
	int monitoring;					// Include or not to include in event handler.
	int version;					// New or old version.
	int locked;						// Obtained lock
	int transfer;					// Is communicating with other version.
	

	/* 	Multi- process & threading. */
	int lock;
	sem_t *fd_sem;					// Exclusive listening on file descriptor.
	sem_t *status_sem;				// Shared memory write lock.
    int *status;					// Shared memory.


	/*	Epoll. */
	struct epoll_event ev;


	struct dsu_socket_list *next;
	
};


struct dsu_state_struct {

	
	#if defined(DEBUG) || defined(ALERT)
	FILE *logfd;
	#endif  


    /* Binded ports of the application. */
	struct dsu_socket_list *binds;
    
	
	/* Termination information. Marked one if the process calls the event handler. */
	int live;


	/*	Communication between processes of the same application using shared memory protected by lock. */
	sem_t *lock;
	int *workers;
	


};


struct dsu_fd_list {
    

	int fd;
    

	struct dsu_fd_list *next;
};


#define dsu_forall_sockets(x, y, ...) { struct dsu_socket_list *dsu_socket_loop = x;\
                                        while (dsu_socket_loop != NULL) {\
                                            (*y)(dsu_socket_loop, ## __VA_ARGS__);\
                                            dsu_socket_loop = dsu_socket_loop->next;\
                                        }\
                                      }


/*	Initialize shadow data structure of socket. */
void dsu_socket_list_init(struct dsu_socket_list *dsu_socket);


/* 	Add file descriptor to list. */
struct dsu_socket_list *dsu_sockets_add(struct dsu_socket_list **head, struct dsu_socket_list *new_node);


/* 	Remove file descriptor to list. */
void dsu_sockets_remove_fd(struct dsu_socket_list **head, int sockfd);


/* 	Search for shadow datastructure based on file descriptor. */
struct dsu_socket_list *dsu_sockets_search_fd(struct dsu_socket_list *head, int fd);


void dsu_socket_add_fds(struct dsu_fd_list **node, int fd);


void dsu_socket_remove_fds(struct dsu_fd_list **node, int fd);


#endif
