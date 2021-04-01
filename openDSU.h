#ifndef openDSU
#define openDSU

#include <assert.h>

/******************* State ******************************/

/* Ordered linked list for unix ports that are bind by 
   the application. The address.sin_port (port) must be unique. 
   Because they can only be bind once. */
struct dsu_socket_struct {
    int sockfd;
    int shadowfd;
    int port;
};

struct dsu_sockets_struct {
    struct dsu_socket_struct value;
    struct dsu_sockets_struct *next;
};

void dsu_sockets_add(struct dsu_sockets_struct *socket, struct dsu_socket_struct value) {
    /* implement check on port. */        

    struct dsu_sockets_struct *new_socket = (struct dsu_sockets_struct *) malloc(sizeof(struct dsu_sockets_struct));
    memcpy(&new_socket->value, &value, sizeof(struct dsu_socket_struct));

    while (socket->next != NULL) {
        socket = socket->next;
    }
    
    socket->next = new_socket;

}

void dsu_sockets_remove(struct dsu_sockets_struct *socket, int sockfd) {
    
    struct dsu_sockets_struct *prev_socket = NULL;
    while (socket->next != NULL && socket->value.sockfd != sockfd) {
        prev_socket = socket;
        socket = socket->next;
    }
    
    if (socket->next == NULL && socket->value.sockfd != sockfd)
        return;
    else
        prev_socket->next = socket->next;

    free(socket);
}

struct dsu_socket_struct *dsu_sockets_search_fd(struct dsu_sockets_struct *socket, int sockfd) {
    
    while (socket->next != NULL) {
        printf("test\n");
        if (socket->value.sockfd == sockfd) return &socket->value;
        socket = socket->next;
    }
    return NULL;

}

struct dsu_socket_struct *dsu_sockets_search_port(struct dsu_sockets_struct *socket, int port) {

    while (socket->next != NULL) {
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
	struct dsu_sockets_struct *binds;
    struct dsu_sockets_struct *exchange;
    
    /* Internal communication. */    
    int sockfd;
    struct sockaddr_un sockfd_addr;

};
#define dsu_state struct dsu_state_struct 

/* Global state variable */
dsu_state dsu_program_state;


/*********************************************************/

/********************* Communication *********************/
#define HAVE_MSGHDR_MSG_CONTROL 1
#define DSU_COMM "/tmp/dsu_comm.unix"
#define DSU_COMM_LEN 13
void dsu_open_communication(void) {
        
    /* Create Unix domain socket. */
    dsu_program_state.sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    bzero(&dsu_program_state.sockfd_addr, sizeof(dsu_program_state.sockfd_addr));
    dsu_program_state.sockfd_addr.sun_family = AF_UNIX;
    strncpy(dsu_program_state.sockfd_addr.sun_path, DSU_COMM, DSU_COMM_LEN);
    
    /* Bind socket, if it fails and already exists we know that another DSU application is 
       already running. These applications need to know each others listening ports. */
    if ( bind(dsu_program_state.sockfd, (struct sockaddr *) &dsu_program_state.sockfd_addr, (socklen_t) sizeof(dsu_program_state.sockfd_addr)) != 0) {
        if ( errno == EINVAL ) {
            /* Other DSU application is running, ask for binded ports. */
            dsu_program_state.version = DSU_NEW_VERSION;
            return;
        } else {
            perror("bind");
            exit(EXIT_FAILURE);
        }
    }
    
    /* Normal execution */
    dsu_program_state.version = DSU_RUNNING_VERSION;
    listen(dsu_program_state.sockfd, 1);
    return;
}

ssize_t write_fd(int fd, void *ptr, int nbytes, int sendfd) {
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

ssize_t read_fd(int fd, void *ptr, int nbytes, int *recvfd) {
	struct msghdr msg;
	struct iovec iov[1];
	ssize_t n;

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
			*recvfd = -1; return -1;   /* descriptor was not passed */
        }
		if (cmptr->cmsg_type != SCM_RIGHTS) {
			*recvfd = -1; return -1;   /* descriptor was not passed */
        }
		*recvfd = *((int *) CMSG_DATA(cmptr));
	} else
		*recvfd = -1;   /* descriptor was not passed */
	#else
	if (msg.msg_accrightslen == sizeof(int))
		*recvfd = newfd;
	else
		*recvfd = -1;   /* descriptor was not passed */
	#endif

	return (n);
}

/*********************************************************/

/********************* POSIX *****************************/
void dsu_init() {
    
    dsu_program_state.version = DSU_RUNNING_VERSION;
    
    /* Initialize the linked lists. */
    dsu_program_state.binds = (struct dsu_sockets_struct *) malloc(sizeof(struct dsu_sockets_struct));
    dsu_program_state.binds->next = NULL;
    dsu_program_state.exchange = (struct dsu_sockets_struct *) malloc(sizeof(struct dsu_sockets_struct));
    dsu_program_state.exchange->next = NULL;
    
    dsu_open_communication();
    
    return;
}
#define DSU_INIT dsu_init()

int dsu_socket(int domain, int type, int protocol) {
    /* Socket is used for ... */
    
    /* Add socket with unkown port to the DSU state. */
    int sockfd = socket(domain, type, protocol);
    if (sockfd > 0) {
        struct dsu_socket_struct node;
        node.port = 0;
        node.sockfd = sockfd;
        node.shadowfd = sockfd;
        dsu_sockets_add(dsu_program_state.binds, node);
    }
    return sockfd;
}
#define socket(domain, type, protocol) dsu_socket(domain, type, protocol)

int dsu_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    /*  Bind is used to accept a client connection on an socket, this means
        it is a "public" socket that is ready to accept requests. */
    
    /* Find corresponding socket metadata and update it. */
    struct dsu_socket_struct *dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd); 
    if (dsu_socketfd == NULL) return -1; // Set error in future

    struct sockaddr_in *addr_t; addr_t = addr;
    dsu_socketfd->port = ntohs(addr_t->sin_port); // !Explain the assumption here! 
    
    if (dsu_program_state.version == DSU_NEW_VERSION) {

        /* Connect to the old version and request for file descriptor. */
        connect(dsu_program_state.sockfd, &dsu_program_state.sockfd_addr, sizeof(dsu_program_state.sockfd_addr));
        send(dsu_program_state.sockfd, &dsu_socketfd->port, 1, MSG_CONFIRM);
        
        /* Recieve file descriptor from old version */
        int buf = 0;
        read_fd(dsu_program_state.sockfd, &buf, 1, &dsu_socketfd->shadowfd);
        if (buf >= 0)
            return 0; // On success zero is returned.
         
    } 
    
    /* Continue normal execution. */
    return bind(sockfd, addr, addrlen);
}
#define bind(sockfd, addr, addrlen) dsu_bind(sockfd, addr, addrlen)

int dsu_listen(int sockfd, int backlog) {
    /* If it is not the new version, run normal listen function. */
    if (dsu_program_state.version == DSU_RUNNING_VERSION)
        return listen(sockfd, sockfd);
    
    /* During update */
    struct dsu_socket_struct *dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
    if (dsu_socketfd == NULL) 
        return -1; // Set error in future (assert)
    else if (dsu_socketfd->sockfd == dsu_socketfd->shadowfd)
        /* The socket is not transfered from old version. */
        return listen(sockfd, sockfd);
    
    return 0;
}
#define listen(sockfd, backlog) dsu_listen(sockfd, backlog)

int dsu_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    
    /* Sniff on DSU unix domain file descriptor for updates. */
    FD_SET(dsu_program_state.sockfd, readfds);
    
    /* Remove exchanged file descriptor. */
    struct dsu_sockets_struct *exchange_socket = dsu_program_state.exchange;
    while (exchange_socket->next != NULL) {
        /* Remove user level file descriptor. */
        FD_CLR(exchange_socket->value.sockfd, readfds);
        exchange_socket = exchange_socket->next;
    }
    
    int result = select(nfds, readfds, writefds, exceptfds, timeout);
    
    if (FD_ISSET(dsu_program_state.sockfd, readfds) && dsu_program_state.version == DSU_RUNNING_VERSION) {      
        /* New version of the application asks for file descriptor. */
        
        int port; int size = sizeof(dsu_program_state.sockfd_addr);
        int internal_com = accept(dsu_program_state.sockfd, (struct sockaddr *) &dsu_program_state.sockfd_addr, (socklen_t *) &size);        
        if (internal_com < 0)
		    exit(EXIT_FAILURE); // Set error in future.
        recv(internal_com, &port, 1, 0);
        
        struct dsu_socket_struct *socket = dsu_sockets_search_port(dsu_program_state.binds, port);
        if (socket == NULL) {
            /* If port is not used in a bind, return error. Send internal fd as place
               holder. Fix this in the future!! */
            port = -1;
            write_fd(internal_com, &port, 1, internal_com);
        } else {
            write_fd(internal_com, &port, 1, socket->shadowfd);
        }
        
        /* Remove the DSU unix socket from the list. */
        FD_CLR(dsu_program_state.sockfd, readfds);
        return result-1;
    }
    
	return result;
}
#define select(nfds, readfds, writefds, exceptfds, timeout) dsu_select(nfds, readfds, writefds, exceptfds, timeout)


/*********************************************************/

#endif
