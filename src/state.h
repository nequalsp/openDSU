#ifndef DSU_STATE
#define DSU_STATE


#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <semaphore.h>
#include <sys/epoll.h>


#ifdef DEBUG
#define DSU_DEBUG 1
#else
#define DSU_DEBUG 0
#endif


/*  Linked list for sockets for communication between different versions. */
struct dsu_fd_list {

    int fd;

    struct dsu_fd_list *next;


};


/*  Linked list for shadow datastructures of the file descriptors. */
struct dsu_socket_list {
    

	int fd;
	struct dsu_fd_list *fds;		// Accepted connections.
    int shadowfd;
    int port;
	struct epoll_event ev;			// Needed in Epoll.
    

    /*  Linked list with accepted connections used for communication
		between different versions. */
    struct sockaddr_un comfd_addr;
	struct dsu_fd_list *comfds;		// File descriptors of acccepted internal connections.
    int comfd;						// File descriptor for listening for internal connections.
	

    /*  Status. */
	int monitoring;
	int version;
	int locked;
	int transfer;
    int blocking;
	

	/* 	Multi- process & threading. */
	sem_t *status_sem;
	sem_t *fd_sem;	
    int *status;


	struct dsu_socket_list *next;

	
};


struct dsu_state_struct {

	
	#if DSU_DEBUG == 1
	FILE *logfd;
	#endif  


    /* Binded ports of the application. */
    struct dsu_socket_list *sockets;
	struct dsu_socket_list *binds;
    
	
	/* Termination information. */
	int live;
	int *workers;
	sem_t *lock;


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

/* 	Transfer file descriptor to different list. */
struct dsu_socket_list *dsu_sockets_transfer_fd(struct dsu_socket_list **dest, struct dsu_socket_list **src, struct dsu_socket_list *dsu_socketfd);

/* 	Search for shadow datastructure based on file descriptor. */
struct dsu_socket_list *dsu_sockets_search_fd(struct dsu_socket_list *head, int fd);

/* 	Search for shadow datastructure based on file descriptor. */
struct dsu_socket_list *dsu_sockets_search_shadowfd(struct dsu_socket_list *head, int shadowfd);

/*	Search for shadow datastructure based on port. */
struct dsu_socket_list *dsu_sockets_search_port(struct dsu_socket_list *head, int port);

/* 	Add open "internal" connection to the shadow data structure. */
void dsu_socket_add_fds(struct dsu_socket_list *node, int comfd, int flag);

/* 	Remove open "internal" connection from the shadow data structure, after close. */
void dsu_socket_remove_fds(struct dsu_socket_list *node, int comfd, int flag);

/*	Search for shadow datastructure based on "internal" connection. */
struct dsu_socket_list *dsu_sockets_search_fds(struct dsu_socket_list *node, int sockfd, int flag);

/*	Switch user level file descriptor to shadow file descriptor (possible inhirited). */
int dsu_shadowfd(int fd);

/*	Switch shadow file descriptor back to user level file descriptor. */
int dsu_originalfd(int shadowfd);

/* 	Check if the file descriptor is an internal connection. */
int dsu_is_internal_conn(int fd);

#endif


