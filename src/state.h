#ifndef DSU_STATE
#define DSU_STATE


#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
//#include <semaphore.h>
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
    int port;
	
	
	struct dsu_fd_list *fds;		// Accepted connections.
    
	
	//struct epoll_event ev;			// Needed in Epoll.
    

    /*  Linked list with accepted connections used for communication
		between different versions. */
    struct sockaddr_un comfd_addr;
	struct dsu_fd_list *comfds;		// File descriptors of acccepted internal connections.
    int comfd;						// File descriptor for listening for internal connections.
	int comfd_close;
	

	int readyfd;					// File descriptor for listening for ready signal.
	int markreadyfd;				// File descriptor for sending ready signal.
    int ready;
	

	struct dsu_socket_list *next;

	
};


struct dsu_state_struct {

	
	#if defined(DEBUG) || defined(TEST)
	FILE *logfd;
	#endif  


    /* Binded ports of the application. */
    struct dsu_socket_list *sockets;
	struct dsu_socket_list *binds;
    
	int close;
	int accept;
    
	int ping;
	int wakeup;

	int activate;
	int deactivate;

    //int processes;
	//struct flock *write_lock;
    //struct flock *unlock;
    

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
//struct dsu_socket_list *dsu_sockets_search_shadowfd(struct dsu_socket_list *head, int shadowfd);

/*	Search for shadow datastructure based on port. */
struct dsu_socket_list *dsu_sockets_search_port(struct dsu_socket_list *head, int port);

/* 	Add open "internal" connection to the shadow data structure. */
void dsu_socket_add_fds(struct dsu_socket_list *node, int comfd, int flag);

/* 	Remove open "internal" connection from the shadow data structure, after close. */
void dsu_socket_remove_fds(struct dsu_socket_list *node, int comfd, int flag);

/*	Search for shadow datastructure based on "internal" connection. */
struct dsu_socket_list *dsu_sockets_search_fds(struct dsu_socket_list *node, int sockfd, int flag);

/* 	Check if the file descriptor is an internal connection. */
// dsu_is_internal_conn(int fd);

#endif


