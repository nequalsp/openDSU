#ifndef openDSU
#define openDSU

#include <assert.h>

#include <stdio.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>

#ifdef DEBUG
#define DSU_DEBUG 1
#else
#define DSU_DEBUG 0
#endif
#define DSU_DEBUG_PRINT(s) if (DSU_DEBUG) printf("%s", s);



/******************* State ******************************/

/*  Ordered linked list for sockets that are bind to port the application.  */
struct dsu_socket_struct {
    int sockfd;
    int shadowfd;
    int port;
};

struct dsu_sockets_struct {
    struct dsu_socket_struct value;
    struct dsu_sockets_struct *next;
};

struct dsu_socket_struct *dsu_sockets_add(struct dsu_sockets_struct **socket, struct dsu_socket_struct value) {        
    
    struct dsu_sockets_struct *new_socket = (struct dsu_sockets_struct *) malloc(sizeof(struct dsu_sockets_struct));
    memcpy(&new_socket->value, &value, sizeof(struct dsu_socket_struct));
    
    /*  First node of the list. */
    if (*socket == NULL) {
        new_socket->next = NULL;
        *socket = new_socket;
        return &new_socket->value;
    }        

    /*  Add to the last node of non-empty list. */
    while ((*socket)->next != NULL) {
        *socket = (*socket)->next;
    }
    
    (*socket)->next = new_socket;

    return &new_socket->value;

}

void dsu_sockets_remove_fd(struct dsu_sockets_struct **socket, int sockfd) {  
    
    /*  Empty list. */
    if (*socket == NULL) return;
    
    /*  List of size 1. */
    if ((*socket)->next == NULL) {
        if ((*socket)->value.sockfd == sockfd) {
            free(*socket);
            *socket = NULL;
            return;
        } else
            return;
    };    
    
    /*  List of size > 1. */
    struct dsu_sockets_struct *prev_socket = *socket;
    struct dsu_sockets_struct *cur_socket = (*socket)->next;
    
    while (cur_socket != NULL) {
        if (cur_socket->value.sockfd == sockfd) {
            prev_socket->next = cur_socket->next;
            free(cur_socket);
            return;
        }
        prev_socket = cur_socket;
        cur_socket = cur_socket->next;
    }
        
}

struct dsu_socket_struct *dsu_sockets_transfer_fd(struct dsu_sockets_struct **dest, struct dsu_sockets_struct **src, struct dsu_socket_struct *dsu_socketfd) {   
    struct dsu_socket_struct *new_dsu_socketfd = dsu_sockets_add(dest, *dsu_socketfd);
    dsu_sockets_remove_fd(src, dsu_socketfd->sockfd);
    return new_dsu_socketfd;
}


struct dsu_socket_struct *dsu_sockets_search_fd(struct dsu_sockets_struct *socket, int sockfd) {
    
    while (socket != NULL) {
        if (socket->value.sockfd == sockfd) return &socket->value;
        socket = socket->next;
    }
    
    return NULL;

}

struct dsu_socket_struct *dsu_sockets_search_port(struct dsu_sockets_struct *socket, int port) {

    while (socket != NULL) {
        if (socket->value.port == port) return &socket->value;
        socket = socket->next;
    }
    
    return NULL;

}




   
#define DSU_RUNNING_VERSION 0
#define DSU_NEW_VERSION 1
struct dsu_state_struct {
    /* State of application. */
    int version;
    
    /* Binded ports of the application. */
    struct dsu_sockets_struct *sockets;
	struct dsu_sockets_struct *binds;
    struct dsu_sockets_struct *exchanged;
    struct dsu_sockets_struct *committed;
    
    /* Internal communication. */
    int connected;
    int sub_sockfd;    
    int sockfd;
    struct sockaddr_un sockfd_addr;

};
#define dsu_state struct dsu_state_struct 

/* Global state variable */
dsu_state dsu_program_state;


/*********************************************************/

/********************* Communication *********************/
#define HAVE_MSGHDR_MSG_CONTROL 1   // Look this up. !!!!!

#define DSU_MSG_SELECT_REQ 1
#define DSU_MSG_SELECT_RES 2
#define DSU_MSG_COMMIT_REQ 3
#define DSU_MSG_COMMIT_RES 4
#define DSU_MSG_EOL_REQ 5
#define DSU_MSG_EOL_RES 6

int dsu_write_message(int fd, int type, int port) {
    
    /* Concat two integers into one character. */
    int data[2]; data[0] = type; data[1] = port;    
    char *ptr = (char*) &data;
    int nbytes = 2 * sizeof(int);
    
    struct iovec iov[1];
	iov[0].iov_base = ptr;
	iov[0].iov_len = nbytes;

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	return (sendmsg(fd, &msg, 0));
}

int dsu_read_message(int fd, int *type, int *port) {
    
    /* Concat two integers into one character. */
    int data[2]; data[0] = 0; data[1] = 0;    
    char *ptr = (char*) &data;
    int nbytes = 2 * sizeof(int);
    
    struct iovec iov[1];
	iov[0].iov_base = ptr;
	iov[0].iov_len = nbytes;
    
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

    int n;
	if ( (n = recvmsg(fd, &msg, 0)) <= 0 )
		return (n);
    
    *type = data[0];
    *port = data[1];

	return (n);
}

int dsu_write_fd(int fd, int sendfd, int port) {
    
    int32_t nport = htonl(port);
    char *ptr = (char*) &nport;
    int nbytes = sizeof(nport);

	struct msghdr msg;
	struct iovec iov[1];

	#ifdef  HAVE_MSGHDR_MSG_CONTROL
	union {
		struct cmsghdr cm;
		char    control[CMSG_SPACE(sizeof(int))];
	} control_un;
	struct cmsghdr *cmptr;

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);

	cmptr = CMSG_FIRSTHDR(&msg);
	cmptr->cmsg_len = CMSG_LEN(sizeof(int));
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_RIGHTS;
	*((int *) CMSG_DATA(cmptr)) = sendfd;
	#else
	msg.msg_accrights = (caddr_t) & sendfd;
	msg.msg_accrightslen = sizeof(int);
	#endif

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	iov[0].iov_base = ptr;
	iov[0].iov_len = nbytes;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	return (sendmsg(fd, &msg, 0));
}

int dsu_read_fd(int fd, int *recvfd, int *port) {
    
    int32_t nport;
    char *ptr = (char*)&nport;
    int nbytes = sizeof(nport);

	struct msghdr msg;
	struct iovec iov[1];
	int n;

	#ifdef HAVE_MSGHDR_MSG_CONTROL
	union {
		struct cmsghdr cm;
		char     control[CMSG_SPACE(sizeof (int))];
	} control_un;
	struct cmsghdr  *cmptr;

	msg.msg_control  = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);
	#else
	int newfd;

	msg.msg_accrights = (caddr_t) & newfd;
	msg.msg_accrightslen = sizeof(int);
	#endif

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	iov[0].iov_base = ptr;
	iov[0].iov_len = nbytes;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	if ( (n = recvmsg(fd, &msg, 0)) <= 0)
		return (n);

	#ifdef  HAVE_MSGHDR_MSG_CONTROL
	if ( (cmptr = CMSG_FIRSTHDR(&msg)) != NULL &&
		cmptr->cmsg_len == CMSG_LEN(sizeof(int))) {
		if (cmptr->cmsg_level != SOL_SOCKET) {
			*recvfd = -1; return -1;
        }
		if (cmptr->cmsg_type != SCM_RIGHTS) {
			*recvfd = -1; return -1;
        }
		*recvfd = *((int *) CMSG_DATA(cmptr));
	} else
		*recvfd = -1;
	#else
	if (msg.msg_accrightslen == sizeof(int))
		*recvfd = newfd;
	else
		*recvfd = -1;
	#endif
    
    *port = ntohl(nport);
	return (n);
}

/* Abstract domain socket cannot be used in portable programs. But has the advantage that
   it automatically disappear when all open references to the socket are closed. */
#define DSU_COMM "\0dsu_comm.unix\0"
#define DSU_COMM_LEN 15
void dsu_open_communication(void) {
    
    /*  Create Unix domain socket for communication between the DSU applications. */
    dsu_program_state.sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    bzero(&dsu_program_state.sockfd_addr, sizeof(dsu_program_state.sockfd_addr));
    dsu_program_state.sockfd_addr.sun_family = AF_UNIX;
    strncpy(dsu_program_state.sockfd_addr.sun_path, DSU_COMM, DSU_COMM_LEN);
    
    /*  Bind socket, if it fails and already exists we know that another DSU application is 
        already running. These applications need to know each others listening ports. */
    if ( bind(dsu_program_state.sockfd, (struct sockaddr *) &dsu_program_state.sockfd_addr, (socklen_t) sizeof(dsu_program_state.sockfd_addr)) != 0) {
        if ( errno == EADDRINUSE ) {
            /*  Other DSU application is running, ask for binded ports. */
            DSU_DEBUG_PRINT("New version\n");
            dsu_program_state.version = DSU_NEW_VERSION;
            /*  Connect to the running version. */
            DSU_DEBUG_PRINT(" - Connect to running version request\n");
            if ( connect(dsu_program_state.sockfd, (struct sockaddr *) &dsu_program_state.sockfd_addr, sizeof(dsu_program_state.sockfd_addr)) < 0) {
                perror("DSU connect: ");
            } else {
                dsu_program_state.connected = 1;
            }
            return;
        } else {
            perror("bind");
            exit(EXIT_FAILURE);
        }
    }
    
    DSU_DEBUG_PRINT("First version\n");
    /*  Continue normal execution. */
    dsu_program_state.version = DSU_RUNNING_VERSION;
    listen(dsu_program_state.sockfd, 1);
    return;
}

/*********************************************************/

/********************* POSIX *****************************/

void dsu_commit_socket(struct dsu_sockets_struct *exchange_socket) {
    DSU_DEBUG_PRINT(" - Commit port request\n");
    if (dsu_write_message(dsu_program_state.sockfd, DSU_MSG_COMMIT_REQ, exchange_socket->value.port) < 0) {
        perror("DSU write: ");
    }
    
    int port = -1; int type = -1;
    DSU_DEBUG_PRINT(" - Commit port response\n");
    if (dsu_read_message(dsu_program_state.sockfd, &type, &port) < 0) {
        perror("DSU read: ");
        /* Check incorrect type TO DO! */
    }

    /*  If the running program acknowledge the commit, transfer it to the committed sockets.  */
    if (port > 0)
        dsu_sockets_transfer_fd(&dsu_program_state.binds, &dsu_program_state.exchanged, &exchange_socket->value);
}

void dsu_set_shadowfd(fd_set *readfds, struct dsu_socket_struct *dsu_sockfd) {
     if (FD_ISSET(dsu_sockfd->sockfd, readfds)) {
        FD_CLR(dsu_sockfd->sockfd, readfds);
        FD_SET(dsu_sockfd->shadowfd, readfds);
    }
}

void dsu_set_originalfd(fd_set *readfds, struct dsu_socket_struct *dsu_sockfd) {
     if (FD_ISSET(dsu_sockfd->shadowfd, readfds)) {
        FD_CLR(dsu_sockfd->shadowfd, readfds);
        FD_SET(dsu_sockfd->sockfd, readfds);
     }
}

int dsu_shadowfd(int sockfd) {
    /*  If sockfd is in the list of binded sockets, return the shadowfd. If not in the list, it is not a
        socket that is binded to a port. */
    
    struct dsu_socket_struct *dsu_sockfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
    if (dsu_sockfd != NULL)
        return dsu_sockfd->shadowfd;
    
    return sockfd;
}

void dsu_init() {
    
    /*  Set default program state. */
    dsu_program_state.version = DSU_RUNNING_VERSION;
    
    /*  Initialize the linked lists. */
    dsu_program_state.sockets = NULL;
    dsu_program_state.binds = NULL;
    dsu_program_state.exchanged = NULL;
    dsu_program_state.committed = NULL;

    /*  Initialize internal communication. */
    dsu_program_state.sockfd = 0;
    dsu_program_state.sub_sockfd = 0;
    dsu_program_state.connected = 0;
    
    /*  Open communication between DSU programs. */
    dsu_open_communication();
    
    return;
}

int dsu_socket(int domain, int type, int protocol) {
    DSU_DEBUG_PRINT("Socket()\n");
    /*  With socket() an endpoint for communication is created and returns a file descriptor that refers to that 
        endpoint. The DSU library will connect the file descriptor to a shadow file descriptor. The shadow file 
        descriptor may be recieved from running version. */
    
    int sockfd = socket(domain, type, protocol);
    if (sockfd > 0) {
        /* After successfull creation, add socket to the DSU state. */
        struct dsu_socket_struct dsu_socket;
        dsu_socket.port = 0;
        dsu_socket.sockfd = sockfd;
        dsu_socket.shadowfd = sockfd;
        dsu_sockets_add(&dsu_program_state.sockets, dsu_socket);
    }
    
    return sockfd;
}

int dsu_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    DSU_DEBUG_PRINT("Bind()\n");
    /*  Bind is used to accept a client connection on an socket, this means it is a "public" socket 
        that is ready to accept requests. */
    
    /* Find the metadata of sockfd, and transfer the socket to the state binds. */
    struct dsu_socket_struct *dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.sockets, sockfd); 
    if (dsu_socketfd == NULL) {
        /*  The socket was not correctly captured in the socket() call. Therefore, we need to return
            error that socket is not correct. On error, -1 is returned, and errno is set to indicate 
            the error. */
        errno = EBADF;
        return -1;
    }
    
    /*  To be able to map a socket to the correct socket in the new version the port must be known. 
        therefore we assume it is of the form sockaddr_in. This assumption must be solved and is still
        TO DO. */
    struct sockaddr_in *addr_t; addr_t = (struct sockaddr_in *) addr;
    dsu_socketfd->port = ntohs(addr_t->sin_port);

    /*  If the program is the new version. Ask the file descriptor from the running version. */
    if (dsu_program_state.version == DSU_NEW_VERSION) {
        
        /*  Ask the running program for the file descriptor of corresponding port. */
        DSU_DEBUG_PRINT(" - Select port request\n");
        int test = dsu_write_message(dsu_program_state.sockfd, DSU_MSG_SELECT_REQ, dsu_socketfd->port);
        if (test < 0) perror("write: ");
        /*  Recieve file descriptor from running version version */
        int port = 0;
        DSU_DEBUG_PRINT(" - Select port response\n");
        dsu_read_fd(dsu_program_state.sockfd, &dsu_socketfd->shadowfd, &port);
        if (port >= 0) {
            /*  On success zero is returned. */
            printf("%d-%d\n", dsu_socketfd->sockfd, dsu_socketfd->shadowfd);
            dsu_sockets_transfer_fd(&dsu_program_state.exchanged, &dsu_program_state.sockets, dsu_socketfd);
            return 0;
        }
        
        
    }

    /*  Running program does not have the file descriptor, then proceed with normal execution. */
    dsu_sockets_transfer_fd(&dsu_program_state.binds, &dsu_program_state.sockets, dsu_socketfd);
    return bind(sockfd, addr, addrlen);
}

int dsu_listen(int sockfd, int backlog) {
    DSU_DEBUG_PRINT("Listen()\n");
    /*  listen() marks the socket referred to by sockfd as a passive socket, that is, as a socket that will be
        used to accept incoming connection requests using accept(). */
    
    /*  If it is the running version, run normal listen() function. */
    if (dsu_program_state.version == DSU_RUNNING_VERSION)
        return listen(sockfd, sockfd);
    
    /*  If it is the new version, verify if the socket is recieved from the running version. */
    struct dsu_socket_struct *dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.exchanged, sockfd);
    if (dsu_socketfd == NULL) {
        /*  The socket was not correctly captured in the socket() call. Therefore, we need to return
            error that socket is not correct. On error, -1 is returned, and errno is set to indicate 
            the error. */
        errno = EBADF;
        return -1;
    }
    /*  If the sockfd and shadowfd are equal, it cannot be recieved from running version. */
    else if (dsu_socketfd->sockfd == dsu_socketfd->shadowfd)
        /*  The socket is not recieved from old version and the normal flow must be used. */
        return listen(sockfd, sockfd);
    
    /*  The socket is recieved from the old version and listen() does not need to be called. */
    return 0;
}

int dsu_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    DSU_DEBUG_PRINT("Select()\n");
    /*  select() allows a program to monitor multiple file descriptors, waiting until one or more of the file descriptors 
        become "ready". The DSU library will modify this list by removing file descriptors that are transfered, and change
        file descriptors to their shadow file descriptors. */

    /* The new version is ready to start listening. Commit the exchanged public sockets. */
    if (dsu_program_state.version == DSU_NEW_VERSION) {
        struct dsu_sockets_struct *exchange_socket = dsu_program_state.exchanged;
        while (exchange_socket != NULL) {
            dsu_commit_socket(exchange_socket);
            exchange_socket = exchange_socket->next;
        }
    }
    
    /*  Remove committed (exchanged) file descriptor. */
    struct dsu_sockets_struct *committed_socket = dsu_program_state.committed;
    while (committed_socket != NULL) {
        FD_CLR(committed_socket->value.sockfd, readfds);
        committed_socket = committed_socket->next;
    }
    if (dsu_program_state.version == DSU_NEW_VERSION) {
        committed_socket = dsu_program_state.exchanged;
        while (committed_socket != NULL) {
            FD_CLR(committed_socket->value.sockfd, readfds);
            committed_socket = committed_socket->next;
        }
    }
    
    /*  Convert to shadow file descriptors, this must be done for binded 
        sockets and sockets that are exchanged but not committed. */
    struct dsu_sockets_struct *bind_socket = dsu_program_state.binds;
    while (bind_socket != NULL) {
       dsu_set_shadowfd(readfds, &bind_socket->value);
       bind_socket = bind_socket->next;
    }
    if (dsu_program_state.version == DSU_RUNNING_VERSION) {
        bind_socket = dsu_program_state.exchanged;
        while (bind_socket != NULL) {
            dsu_set_shadowfd(readfds, &bind_socket->value);
            bind_socket = bind_socket->next;
        }
    }
    
    /*  The program can terminate if no file descriptors are monitored. This would mean that no new connections can be accepted. */
    fd_set empty; FD_ZERO(&empty);        
    //if (memcmp(readfds, &empty, sizeof(empty)) == 0) {
    //    /*  Disconnect from the running program. */
    //    DSU_DEBUG_PRINT("Terminate\n");
    //    close(dsu_program_state.sockfd);
    //    if (dsu_program_state.sub_sockfd != 0) close(dsu_program_state.sub_sockfd);
    //    exit(EXIT_SUCCESS);
    //}
    
    /*  Sniff on DSU unix domain file descriptor for messages of new versions. */
    if (dsu_program_state.version == DSU_RUNNING_VERSION) {
        FD_SET(dsu_program_state.sockfd, readfds);
        if (dsu_program_state.sub_sockfd != 0) {
            FD_SET(dsu_program_state.sub_sockfd, readfds);
        }
    }
    printf("t:%d\n", FD_ISSET(5, readfds));
    int result = select(nfds, readfds, writefds, exceptfds, timeout);
    if (result < 0) return result;   
    printf("t:%d\n", FD_ISSET(5, readfds));
    /*  Handle message of the new version. */
    if (dsu_program_state.version == DSU_RUNNING_VERSION) {
        
        if (FD_ISSET(dsu_program_state.sockfd, readfds)) {
            DSU_DEBUG_PRINT(" - Accept new version request\n");
            /*  Accept the connection request of the new version. */
            int size = sizeof(dsu_program_state.sockfd_addr);
            dsu_program_state.sub_sockfd = accept(dsu_program_state.sockfd, (struct sockaddr *) &dsu_program_state.sockfd_addr, (socklen_t *) &size);
            FD_CLR(dsu_program_state.sockfd, readfds);
            if (dsu_program_state.sub_sockfd < 0)
		        exit(EXIT_FAILURE); // Set error in future. TO DO!!
        }
        
        if (FD_ISSET(dsu_program_state.sub_sockfd, readfds)) {
            int type = -1; int port = -1;
            dsu_read_message(dsu_program_state.sub_sockfd, &type, &port);
            
            /*  Transfer the file descriptor to the new version. */
            if (type == DSU_MSG_SELECT_REQ) {
                DSU_DEBUG_PRINT(" - Select port request\n");
                struct dsu_socket_struct *dsu_sockfd = dsu_sockets_search_port(dsu_program_state.binds, port);                
                if (dsu_sockfd == NULL) {
                    /*  If port is not present, send not accepted response. */
                    DSU_DEBUG_PRINT(" - Select port response\n");
                    dsu_write_fd(dsu_program_state.sub_sockfd, dsu_program_state.sub_sockfd, -1);
                } else {
                    /*  Send the socket to the new version. */
                    dsu_sockets_transfer_fd(&dsu_program_state.exchanged, &dsu_program_state.binds, dsu_sockfd);
                    DSU_DEBUG_PRINT(" - Select port response\n");
                    dsu_write_fd(dsu_program_state.sub_sockfd, dsu_sockfd->sockfd, port);
                }
            } 

            /*  Stop listening to the socket, it is successfully transfered. */
            else if (type == DSU_MSG_COMMIT_REQ) {
                DSU_DEBUG_PRINT(" - Commit port request\n");
                struct dsu_socket_struct *dsu_sockfd = dsu_sockets_search_port(dsu_program_state.exchanged, port);
                /*  Only accept commit if there are no pending requests on the socket. */
                if (dsu_sockfd != NULL && !FD_ISSET(dsu_sockfd->shadowfd, readfds) ) {
                    dsu_sockets_transfer_fd(&dsu_program_state.committed, &dsu_program_state.exchanged, dsu_sockfd);
                } else port = -1;
                DSU_DEBUG_PRINT(" - Commit port response\n");
                printf("port %d\n", port);
                dsu_write_message(dsu_program_state.sub_sockfd, DSU_MSG_COMMIT_RES, port);
            }
            
            FD_CLR(dsu_program_state.sub_sockfd, readfds);
        }

    }

    /* Convert shadow file descriptors back to user level file descriptors. */
    bind_socket = dsu_program_state.binds;
    while (bind_socket != NULL) {
        dsu_set_originalfd(readfds, &bind_socket->value);
        bind_socket = bind_socket->next;
    }
    if (dsu_program_state.version == DSU_RUNNING_VERSION) {
        bind_socket = dsu_program_state.exchanged;
        while (bind_socket != NULL) {
            dsu_set_originalfd(readfds, &bind_socket->value);
            bind_socket = bind_socket->next;
        }
    }
    

	return result;
}

int dsu_accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen) {
    /*  The accept() system call is used with connection-based socket types (SOCK_STREAM, SOCK_SEQPACKET).  It extracts the first
        connection request on the queue of pending connections for the listening socket, sockfd, creates a new connected socket, and
        returns a new file descriptor referring to that socket. The DSU library need to convert the file descriptor to the shadow
        file descriptor. */     
    
    int shadowfd = dsu_shadowfd(sockfd);  
    return accept(shadowfd, addr, addrlen);    
}

#define DSU_INIT dsu_init()
#define socket(domain, type, protocol) dsu_socket(domain, type, protocol)
#define bind(sockfd, addr, addrlen) dsu_bind(sockfd, addr, addrlen)
#define listen(sockfd, backlog) dsu_listen(sockfd, backlog)
#define select(nfds, readfds, writefds, exceptfds, timeout) dsu_select(nfds, readfds, writefds, exceptfds, timeout)
#define accept(sockfd, addr, addrlen) dsu_accept(sockfd, addr, addrlen);

/*********************************************************/

#endif
