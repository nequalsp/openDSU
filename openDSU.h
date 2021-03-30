#ifndef openDSU
#define openDSU

/******************* State ******************************/

/* Ordered linked list for unix ports that are bind by 
   the application. The address.sin_port (port) must be unique. 
   Because they can only be bind once. */
struct dsu_sockets_struct {
    int sockfd;
    int shadowfd;
    int port;
    struct dsu_sockets_struct *next;
}
#define dsu_sockets struct dsu_sockets_struct *

void dsu_sockets_add(dsu_sockets *socket, int sockfd, int port) {
    dsu_sockets new_socket;
    new_socket->port = port;
    new_socket->sockfd = sockfd;
    new_socket->shadowfd = sockfd;

    dsu_sockets prev_socket;
    while (socket->next != null && socket->port < port) {
        prev_socket = sockets;
        socket = socket->next;
    }
    assert(socket->port != port) // The port must be unique.
    
    if (socket->next == null && socket->port < port) {
        socket->next = new_socket;
    } else {
        prev_socket->next = new_socket;
        new_socket->next = socket;
    }
    
}

void dsu_sockets_remove(dsu_sockets *socket, int sockfd) {
    
    dsu_sockets prev_socket;
    while (socket->next != null && socket->sockfd != sockfd) {
        prev_socket = socket;
        socket = socket->next;
    }
    
    if (socket->next == null && socket->sockfd != sockfd)
        return;
    else
        prev_socket->next = socket->next;
}

dsu_sockets dsu_sockets_search_fd(dsu_sockets *socket, int sockfd) {
    
    while (socket->next != null) {
        if (*socket->sockfd == sockfd) return socket;
        socket = socket->next;
    }
    return null;

}

#define DSU_RUNNING_VERSION 0;
#define DSU_NEW_VERSION 1;
struct dsu_state_struct {
    /* State of application. */
    int update;
    
    /* Binded ports of the application. */
	dsu_sockets binds;
    dsu_sockets exchange;
    
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
            dsu_program_state.update = DSU_NEW_VERSION;
            return;
        } else {
            exit(EXIT_FAILURE);
        }
    }
    
    /* Normal execution */
    dsu_program_state.update = DSU_RUNNING_VERSION;
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
			*recvfd = -1; return;   /* descriptor was not passed */
        }
		if (cmptr->cmsg_type != SCM_RIGHTS) {
			*recvfd = -1; return;   /* descriptor was not passed */
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
void dsu_init(int argc, char **argv) {
    
    dsu_open_communication();
    
    return;
}
#define DSU_INIT(argc, argv) dsu_init(argc, argv)

int dsu_socket(int domain, int type, int protocol) {
    /* Socket is used for ... */
    
    /* Add socket with unkown port to the DSU state. */
    int sockfd = socket(domain,type,protocol);
    if (sockfd > 0)
        dsu_sockets_add(dsu_state_struct.binds, sockfd, NULL);
    return sockfd;
}
#define socket(domain, type, protocol) dsu_socket(domain, type, protocol)

int dsu_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    /*  Bind is used to accept a client connection on an socket, this means
        it is a "public" socket that is ready to accept requests. */
    
    /* Find corresponding socket metadata and update it. */
    dsu_sockets dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.binds, int sockfd); 
    if (dsu_socketfd == null) return -1; // Set error in future
    dsu_socketfd.port = ntohs(addr->sin_port);
    
    if (dsu_program_state.update == DSU_NEW_VERSION) {

        /* Connect to the old version and request for file descriptor. */
        connect(dsu_program_state.socketfd, &dsu_program_state.sockfd_addr, sizeof(dsu_program_state.sockfd_addr));
        send(dsu_program_state.socketfd, &dsu_socketfd.port, sizeof(dsu_socketfd.port), MSG_CONFIRM);
        
        /* Recieve file descriptor from old version */
        int buf = 0;
        read_fd(dsu_program_state.socketfd, &buf, 1, &dsu_socketfd.shadowfd);
        if (buf >= 0)
            return 0; // On success zero is returned.
         
    } 
    
    /* Continue normal execution. */
    return bind(sockfd, addr, addrlen);
}
#define bind(sockfd, addr, addrlen) dsu_bind(sockfd, addr, addrlen)

int dsu_listen(int sockfd, int backlog) {
    /* If it is not the new version, run normal listen function.*/
    if (dsu_program_state.update == DSU_RUNNING_VERSION)
        return listen(sockfd, sockfd);
    
    /* During update */
    dsu_sockets dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.binds, int sockfd);
    if (dsu_socketfd == null) 
        return -1; // Set error in future
    else if (dsu_socketfd.sockfd == dsu_socketfd.shadowfd)
        /* The socket is not transfered from old version. */
        return listen(sockfd, sockfd);
    
    return 0;
}
#define listen(sockfd, backlog) dsu_listen(sockfd, backlog)

int dsu_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    /* Sniff on DSU unix domain socket. */
    
    /* If port is transfered, do not accept new connections. */

    /* If all ports are transfered and no more connections are alive terminate. */
	return select(nfds, readfds, writefds, exceptfds, timeout);
}
#define select(nfds, readfds, writefds, exceptfds, timeout) dsu_select(nfds, readfds, writefds, exceptfds, timeout)

/*********************************************************/

#endif
