#define _GNU_SOURCE //gettid

#include <stdio.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>          
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/file.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/msg.h>

#include "openDSU.h"

#if DSU_DEBUG == 1
#define DSU_DEBUG_PRINT(format, ...) {printf(format, ## __VA_ARGS__); fprintf(dsu_program_state.logfd, format, ## __VA_ARGS__); fflush(dsu_program_state.logfd);}
#else
#define DSU_DEBUG_PRINT(format, ...)
#endif


#undef socket
#undef bind
#undef listen
#undef accept
#undef accept4
#undef close

#undef select


/* Global state variable */
struct dsu_state_struct dsu_program_state;


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

int dsu_socket_add_comfd(struct dsu_socket_struct *socket, int comfd) {
       
    struct dsu_comfd_struct **comfds = &socket->comfds;

    struct dsu_comfd_struct *new_comfd = (struct dsu_comfd_struct *) malloc(sizeof(struct dsu_comfd_struct));
    memcpy(&new_comfd->value, &comfd, sizeof(int));
    new_comfd->next = NULL;     // Append at the end of the list.
    
    /*  First node of the list. */
    if (*comfds == NULL) {
        *comfds = new_comfd;
        return new_comfd->value;
    }        

    /*  Add to the last node of non-empty list. */
    while ((*comfds)->next != NULL) {
        *comfds = (*comfds)->next;
    }
    (*comfds)->next = new_comfd;
	
    return new_comfd->value;

}

void dsu_socket_remove_comfd(struct dsu_socket_struct *socket, int comfd) {

    struct dsu_comfd_struct **comfds = &socket->comfds;

    /*  Empty comfds. */
    if (*comfds == NULL) return;
    
    /*  List of size 1. */
    if ((*comfds)->next == NULL) {
        if ((*comfds)->value == comfd) {
            free(*comfds);
            *comfds = NULL;
            return;
        } else
            return;
    };    
    
    /*  List of size > 1. */
    struct dsu_comfd_struct *prev_comfds = *comfds;
    struct dsu_comfd_struct *cur_comfds = (*comfds)->next;
    
    while (cur_comfds != NULL) {
        if (cur_comfds->value == comfd) {
            prev_comfds->next = cur_comfds->next;
            free(cur_comfds);
            return;
        }
        prev_comfds = cur_comfds;
        cur_comfds = cur_comfds->next;
    }

}

#define dsu_forall_sockets(x, y, ...) { struct dsu_sockets_struct *dsu_socket_loop = x;\
                                        while (dsu_socket_loop != NULL) {\
                                            (*y)(&dsu_socket_loop->value, ## __VA_ARGS__);\
                                            dsu_socket_loop = dsu_socket_loop->next;\
                                        }\
                                      }


int dsu_write_fd(int fd, int sendfd, int port) {
    
    int32_t nport = htonl(port);
    char *ptr = (char*) &nport;
    int nbytes = sizeof(nport);

	struct msghdr msg;
	struct iovec iov[1];
    
    /* Check whether the socket is valid. TO HANDLE ERROR*/
    int error; socklen_t len; 
    if (getsockopt(sendfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        perror("DSU \"dsu_write_fd() not a fd not a socket\":");
        exit(EXIT_FAILURE);
    }
    
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



void dsu_sniff_conn(struct dsu_socket_struct *dsu_sockfd, fd_set *readfds) {
    /*  Listen under the hood to accepted connections on public socket. A new generation can request
        file descriptors. During DUAL listening, only one  */
    
    if (dsu_sockfd->internal[DSU_STATUS] == DSU_ACTIVE) {
        
        DSU_DEBUG_PRINT(" - Add %d (%ld-%ld)\n", dsu_sockfd->comfd, (long) getpid(), (long) gettid());
            
        FD_SET(dsu_sockfd->comfd, readfds);
        
    }

    
    /* Contains zero or more accepted connections. */
    struct dsu_comfd_struct *comfds = dsu_sockfd->comfds;   
    
    while (comfds != NULL) {

        DSU_DEBUG_PRINT(" - Add %d (%ld-%ld)\n", comfds->value, (long) getpid(), (long) gettid());

        FD_SET(comfds->value, readfds);

        comfds = comfds->next;        
    }

}


void dsu_acc_conn(struct dsu_socket_struct *dsu_sockfd, fd_set *readfds) {
    /*  Accept connection requests of new generation. Race conditions could occur because multiple
        processes or threads are listening on the same socket. */
    
    if (    FD_ISSET(dsu_sockfd->comfd, readfds)
        &&  dsu_sockfd->internal[DSU_STATUS] == DSU_ACTIVE 
        &&  dsu_sockfd->status[DSU_COUNTER] <= dsu_sockfd->internal[DSU_COUNTER] 
        &&  dsu_sockfd->locked == DSU_LOCKED) {
            
            int size = sizeof(dsu_sockfd->comfd_addr);
            int acc = accept(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, (socklen_t *) &size);
            
            if ( acc != -1) {
                DSU_DEBUG_PRINT(" - Accept %d on %d (%ld-%ld)\n", acc, dsu_sockfd->comfd, (long) getpid(), (long) gettid());
                dsu_socket_add_comfd(dsu_sockfd, acc);
            }
        
    }
    
    if (dsu_sockfd->internal[DSU_STATUS] == DSU_ACTIVE) {
        
        DSU_DEBUG_PRINT(" - remove: %d (%ld-%ld)\n", dsu_sockfd->comfd, (long) getpid(), (long) gettid());
        FD_CLR(dsu_sockfd->comfd, readfds);

    }

}


void dsu_handle_conn(struct dsu_socket_struct *dsu_sockfd, fd_set *readfds) {
    /*  Race conditions could happend when a "late" fork is performed. The fork happend after 
        accepting dsu communication connection. */
 
    struct dsu_comfd_struct *comfds =   dsu_sockfd->comfds;   
    
    while (comfds != NULL) {
    
        if (FD_ISSET(comfds->value, readfds)) {
            
            int buffer = 0;
            int port = dsu_sockfd->port;
            
            
            /*  Race condition on recv, hence do not wait, and continue on error. */
            int r = recv(comfds->value, &buffer, sizeof(buffer), MSG_DONTWAIT);
            
            
            /*  Connection is closed by client. */
            if (r == 0) {
                
                DSU_DEBUG_PRINT(" - Close on %d (%ld-%ld)\n", comfds->value, (long) getpid(), (long) gettid());
                                 
                close(comfds->value);
                FD_CLR(comfds->value, readfds);
                
                struct dsu_comfd_struct *_comfds = comfds;
                comfds = comfds->next;
                dsu_socket_remove_comfd(dsu_sockfd, _comfds->value);
                
                continue;
            } 
            
            
            /*  Other process already read message. */
            else if (r == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
                goto dsu_next_comfd;
            
            
            /*  Unkown error. */
            else if (r == -1)
                port = 1;
            
            
            DSU_DEBUG_PRINT(" - Message on %d (%ld-%ld)\n", comfds->value, (long) getpid(), (long) gettid());
            
            
            /*  Respond to request. */
            dsu_write_fd(comfds->value, dsu_sockfd->shadowfd, port);
        }
        
        dsu_next_comfd:
            DSU_DEBUG_PRINT(" - remove %d (%ld-%ld)\n", comfds->value, (long) getpid(), (long) gettid());
            FD_CLR(comfds->value, readfds);
            comfds = comfds->next;
                
    }

}
        

void dsu_settle_fd(struct dsu_socket_struct *dsu_sockfd, fd_set *readfds) {
    /*  The transfer of ownership: 
        1.  New generation set state from 0 to 1.
        2.  Old generation closes unix domain socket for internal communication and sets state from 1 to 2.
        3.  New generation initializes unix domain socket for internal communication and increases to version
            counter and sets state from 2 to 0. */


    DSU_DEBUG_PRINT("\n     State %d (%ld-%ld)\n    Internal state: %ld  External state: %ld\n\
    Internal counter: %ld  External counter: %ld\n\n", dsu_sockfd->port, (long) getpid(), (long) gettid(), dsu_sockfd->internal[DSU_STATUS], dsu_sockfd->status[DSU_STATUS], dsu_sockfd->internal[DSU_COUNTER], dsu_sockfd->status[DSU_COUNTER]);


    /*  Processes (>= 1) that are active for this port see that the port is now actively handled by new generation 
        and will stop internal communication and mark state to transfered. */
    if (dsu_sockfd->internal[DSU_STATUS] == DSU_ACTIVE
        && dsu_sockfd->status[DSU_COUNTER] == dsu_sockfd->internal[DSU_COUNTER] 
        && dsu_sockfd->status[DSU_STATUS] > DSU_LISTEN
    ) {
            
        DSU_DEBUG_PRINT(" - close %d and go to next gen (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
        close(dsu_sockfd->comfd);
        dsu_sockfd->internal[DSU_STATUS] = DSU_INACTIVE;
        dsu_sockfd->status[DSU_STATUS] = DSU_TRANSFERED;
        --dsu_program_state.binded;
   
    }

    
    /*  Old generation stop accepting connections. */
    if (dsu_sockfd->status[DSU_COUNTER] > dsu_sockfd->internal[DSU_COUNTER]) {
    
        DSU_DEBUG_PRINT(" - %d X (%ld-%ld)\n", dsu_sockfd->sockfd, (long) getpid(), (long) gettid());
        FD_CLR(dsu_sockfd->sockfd, readfds);
        
        /* No more connections to handle. */
        DSU_DEBUG_PRINT(" - Binded: %d & Accepted: %d (%ld-%ld)\n", dsu_program_state.binded, dsu_program_state.accepted, (long) getpid(), (long) gettid());
        if (dsu_program_state.binded == 0 && dsu_program_state.accepted == 0) {
            DSU_DEBUG_PRINT(" - Exit (%ld-%ld)\n", (long) getpid(), (long) gettid());
            exit(EXIT_SUCCESS);    
        }


    }

    /*  New generation set socket to dual mode. New generation is also accepting connections on this port. */
    if (dsu_sockfd->status[DSU_COUNTER] < dsu_sockfd->internal[DSU_COUNTER]
        && dsu_sockfd->status[DSU_STATUS] == DSU_LISTEN
    ) {
        
        DSU_DEBUG_PRINT(" - Mark ready %d (%ld-%ld)\n", dsu_sockfd->comfd, (long) getpid(), (long) gettid());
        dsu_sockfd->status[DSU_STATUS] = DSU_DUAL;

    }
    
    /*  New generation takes over internal communication. */ 
    if (dsu_sockfd->status[DSU_COUNTER] < dsu_sockfd->internal[DSU_COUNTER]
        && dsu_sockfd->status[DSU_STATUS] == DSU_TRANSFERED
    ) {
       
        /* Listen() cannot follow connect() on the same file descriptor. */
        close(dsu_sockfd->comfd);
        dsu_sockfd->comfd = socket(AF_UNIX, SOCK_STREAM, 0);
        
        /*  Bind() and listen() are thread safe. Only one process or thread will be able to reserve the port. */
        DSU_DEBUG_PRINT(" - Try initialize communication on  %d (%ld-%ld)\n", dsu_sockfd->comfd, (long) getpid(), (long) gettid());
        if (bind(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, (socklen_t) sizeof(dsu_sockfd->comfd_addr)) == 0) {
            
            if (listen(dsu_sockfd->comfd, MAXNUMOFPROC) == 0) {
                
                DSU_DEBUG_PRINT(" - Initialize communication on  %d (%ld-%ld)\n", dsu_sockfd->comfd, (long) getpid(), (long) gettid());
                
                dsu_sockfd->status[DSU_PGID] = dsu_sockfd->internal[DSU_PGID];
                dsu_sockfd->status[DSU_COUNTER] = dsu_sockfd->internal[DSU_COUNTER];
                dsu_sockfd->status[DSU_STATUS] = DSU_LISTEN;

                dsu_sockfd->internal[DSU_STATUS] = DSU_ACTIVE;

                ++dsu_program_state.binded;
            }
        
        }

    }
    
    /*  Due to race conditions, state might incorrectly be set on transfered. */ 
    if ( dsu_sockfd->internal[DSU_STATUS] == DSU_ACTIVE
        && dsu_sockfd->status[DSU_COUNTER] == dsu_sockfd->internal[DSU_COUNTER]
        && dsu_sockfd->status[DSU_STATUS] == DSU_TRANSFERED
    ) {
        dsu_sockfd->status[DSU_STATUS] = DSU_LISTEN;
    }
        
}


void dsu_set_shadowfd(struct dsu_socket_struct *dsu_sockfd, fd_set *readfds) {
    /*  Change socket file descripter to its shadow file descriptor. */   
   
    if (FD_ISSET(dsu_sockfd->sockfd, readfds)) {
        
        FD_CLR(dsu_sockfd->sockfd, readfds);
        DSU_DEBUG_PRINT(" - Try lock %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
        if (sem_trywait(dsu_sockfd->sem_id) == 0) {
            DSU_DEBUG_PRINT(" - Lock %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
            dsu_sockfd->locked = DSU_LOCKED;
            DSU_DEBUG_PRINT(" - Set %d => %d (%ld-%ld)\n", dsu_sockfd->sockfd, dsu_sockfd->shadowfd, (long) getpid(), (long) gettid());
            FD_SET(dsu_sockfd->shadowfd, readfds);
        }
        
    }
	
}


void dsu_set_originalfd(struct dsu_socket_struct *dsu_sockfd, fd_set *readfds) {
    /*  Change shadow file descripter to its original file descriptor. */
    
    if (dsu_sockfd->locked == 1) {
        DSU_DEBUG_PRINT(" - Unlock %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
        sem_post(dsu_sockfd->sem_id);
        dsu_sockfd->locked = 0;
    }

    
    if (FD_ISSET(dsu_sockfd->shadowfd, readfds)) {
		DSU_DEBUG_PRINT(" - Reset %d => %d (%ld-%ld)\n", dsu_sockfd->shadowfd, dsu_sockfd->sockfd, (long) getpid(), (long) gettid());
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


void dsu_socket_struct_init(struct dsu_socket_struct *dsu_socket) {
    dsu_socket->port        = 0;
    dsu_socket->sockfd      = 0;
    dsu_socket->shadowfd    = 0;
    dsu_socket->comfd       = 0;
    dsu_socket->comfds      = NULL;
    dsu_socket->status      = NULL;
    dsu_socket->internal[0] = DSU_LISTEN;
    dsu_socket->internal[1] = 0;
    dsu_socket->internal[2] = DSU_INACTIVE;
    dsu_socket->sem_id      = NULL;
    dsu_socket->locked      = 0;
}


void dsu_init() {

	#if DSU_DEBUG == 1
    int size = snprintf(NULL, 0, "%s-%ld.log", DSU_LOG, (long) getpid());
	char logfile[size+1];	
	sprintf(logfile, "%s-%ld.log", DSU_LOG, (long) getpid());
	dsu_program_state.logfd = fopen(logfile, "w");
	if (dsu_program_state.logfd == NULL) {
		perror("DSU \" Error opening debugging file\":");
		exit(EXIT_FAILURE);
	}
	#endif
	DSU_DEBUG_PRINT("INIT() (%ld-%ld)\n", (long) getpid(), (long) gettid());

    dsu_program_state.sockets = NULL;
    dsu_program_state.binds = NULL;

    dsu_program_state.binded = 0;
    dsu_program_state.accepted = 0;


    return;
}



int dsu_socket(int domain, int type, int protocol) {
    DSU_DEBUG_PRINT("Socket() (%ld-%ld)\n", (long) getpid(), (long) gettid());
    /*  With socket() an endpoint for communication is created and returns a file descriptor that refers to that 
        endpoint. The DSU library will connect the file descriptor to a shadow file descriptor. The shadow file 
        descriptor may be recieved from running version. */

    int sockfd = socket(domain, type, protocol);
    if (sockfd > 0) {
        /* After successfull creation, add socket to the DSU state. */
        struct dsu_socket_struct dsu_socket;
        dsu_socket_struct_init(&dsu_socket);
        dsu_socket.shadowfd = dsu_socket.sockfd = sockfd;
        dsu_sockets_add(&dsu_program_state.sockets, dsu_socket);
    }
    
    return sockfd;
}



int dsu_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    DSU_DEBUG_PRINT("Bind() (%ld-%ld)\n", (long) getpid(), (long) gettid());
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
    
    
    /*  Possibly communicate the socket from the older version. A bind can only be performed once on the same
        socket, therefore, processes will not communicate with each other. Abstract domain socket cannot be used 
        in portable programs. But has the advantage that it automatically disappear when all open references to 
        the socket are closed. */
    bzero(&dsu_socketfd->comfd_addr, sizeof(dsu_socketfd->comfd_addr));
    dsu_socketfd->comfd_addr.sun_family = AF_UNIX;
    sprintf(dsu_socketfd->comfd_addr.sun_path, "X%s_%d.unix", DSU_COMM, dsu_socketfd->port);    // On Linux, sun_path is 108 bytes in size.
    dsu_socketfd->comfd_addr.sun_path[0] = '\0';                                                // Abstract linux socket.
    dsu_socketfd->comfd = socket(AF_UNIX, SOCK_STREAM, 0);
    DSU_DEBUG_PRINT(" - Bind port %d on %d (%ld-%ld)\n", dsu_socketfd->port, sockfd, (long) getpid(), (long) gettid());
    
    
    /*  To communicate the status between the process, shared memory is used. */
    int pathname_size = snprintf(NULL, 0, "%s_%d", DSU_COMM, dsu_socketfd->port);
    char pathname[pathname_size+1];
    sprintf(pathname, "%s_%d", DSU_COMM, dsu_socketfd->port);
    dsu_socketfd->status_key = ftok(pathname, 1);
    int shmid = shmget(dsu_socketfd->status_key, 3*sizeof(long), 0666|IPC_CREAT);
    dsu_socketfd->status = (long *) shmat(shmid, NULL, 0);

    
    /*  During the transition a lock is needed. */
    int pathname_size_sem = snprintf(NULL, 0, "/%s_%d.semaphore", DSU_COMM, dsu_socketfd->port);
    char pathname_sem[pathname_size_sem+1];
    sprintf(pathname_sem, "%s_%d", DSU_COMM, dsu_socketfd->port);
    //pathname_sem[1] = '\0';
    
    
    /*  Bind socket, if it fails and already exists we know that another DSU application is 
        already running. These applications need to know each others listening ports. */

    if ( bind(dsu_socketfd->comfd, (struct sockaddr *) &dsu_socketfd->comfd_addr, (socklen_t) sizeof(dsu_socketfd->comfd_addr)) == -1) {

        if ( errno == EADDRINUSE ) {
            
            DSU_DEBUG_PRINT(" - Connect on %d (%ld-%ld)\n", dsu_socketfd->comfd, (long) getpid(), (long) gettid()); 
            /*  An older version of the program is running, connect to this application over Unix domain socket. */
            if ( connect(dsu_socketfd->comfd, (struct sockaddr *) &dsu_socketfd->comfd_addr, sizeof(dsu_socketfd->comfd_addr)) != -1) {
                
                DSU_DEBUG_PRINT(" - Send on %d (%ld-%ld)\n", dsu_socketfd->comfd, (long) getpid(), (long) gettid());           
                
                /*  Ask the running program for the file descriptor of corresponding port. */
                if ( send(dsu_socketfd->comfd, &dsu_socketfd->port, sizeof(dsu_socketfd->port), 0) > 0) {

                    DSU_DEBUG_PRINT(" - Recieve on %d (%ld-%ld)\n", dsu_socketfd->comfd, (long) getpid(), (long) gettid());
                    
                    /*  Recieve file descriptor from running version version. */
                    int port = 0;
                    dsu_read_fd(dsu_socketfd->comfd, &dsu_socketfd->shadowfd, &port);
                    if (port > 0) {
                        
                        dsu_socketfd->internal[DSU_PGID] = getpgid(getpid());
                        dsu_socketfd->internal[DSU_COUNTER] = dsu_socketfd->status[DSU_COUNTER];
                        dsu_socketfd->internal[DSU_STATUS] = DSU_INACTIVE;
                        
                        if (dsu_socketfd->status[DSU_PGID] != dsu_socketfd->internal[DSU_PGID])
                            ++dsu_socketfd->internal[DSU_COUNTER];
                        
                        dsu_socketfd->sem_id = sem_open(pathname, O_CREAT, S_IRWXO | S_IRWXG | S_IRWXU, 1);
                        
                        DSU_DEBUG_PRINT(" - Close on %d (%ld-%ld)\n", dsu_socketfd->comfd, (long) getpid(), (long) gettid()); 
                        close(dsu_socketfd->comfd);
	
                        dsu_sockets_transfer_fd(&dsu_program_state.binds, &dsu_program_state.sockets, dsu_socketfd);
                        
                        return 0;
                    }
                    
                }
            }
        }
    }
    

    DSU_DEBUG_PRINT(" - Initialize communication on %d (%ld-%ld)\n", dsu_socketfd->comfd, (long) getpid(), (long) gettid());
    /*  Running program does not have the file descriptor, then proceed with normal execution. */
    listen(dsu_socketfd->comfd, MAXNUMOFPROC);
    

    sem_unlink(pathname_sem);
    dsu_socketfd->sem_id = sem_open(pathname_sem, O_CREAT | O_EXCL, S_IRWXO | S_IRWXG | S_IRWXU, 1);
    sem_init(dsu_socketfd->sem_id, PTHREAD_PROCESS_SHARED, 1);
    

    int pid = getpid();
    dsu_socketfd->internal[DSU_PGID] = getpgid(pid);
    dsu_socketfd->internal[DSU_COUNTER] = 0;
    dsu_socketfd->internal[DSU_STATUS] = DSU_ACTIVE;
    memcpy(dsu_socketfd->status, dsu_socketfd->internal, 3*sizeof(long));
    

    dsu_sockets_transfer_fd(&dsu_program_state.binds, &dsu_program_state.sockets, dsu_socketfd);
    

    ++dsu_program_state.binded;
    
    
    return bind(sockfd, addr, addrlen);
}

int dsu_listen(int sockfd, int backlog) {
    DSU_DEBUG_PRINT("Listen() on fd %d (%ld-%ld)\n", sockfd, (long) getpid(), (long) gettid());
    /*  listen() marks the socket referred to by sockfd as a passive socket, that is, as a socket that will be
        used to accept incoming connection requests using accept(). */
    
    struct dsu_socket_struct *dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
    
    /*  The file descriptor is not inherited. */
    if (dsu_socketfd == NULL || dsu_socketfd->internal[DSU_COUNTER] == 0) {
        return listen(sockfd, sockfd);
    }
    
    /*  The socket is recieved from the older generation and listen() does not need to be called. */
	DSU_DEBUG_PRINT(" - Shadow fd: %d (%ld-%ld)\n", dsu_socketfd->shadowfd, (long) getpid(), (long) gettid());
    return 0;
}

int dsu_accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen) {
	DSU_DEBUG_PRINT("Accept() (%ld-%ld)\n", (long) getpid(), (long) gettid());   
    /*  The accept() system call is used with connection-based socket types (SOCK_STREAM, SOCK_SEQPACKET).  It extracts the first
        connection request on the queue of pending connections for the listening socket, sockfd, creates a new connected socket, and
        returns a new file descriptor referring to that socket. The DSU library need to convert the file descriptor to the shadow
        file descriptor. */     
    
    int shadowfd = dsu_shadowfd(sockfd);
    
    int sessionfd = accept(shadowfd, addr, addrlen);
	if (sessionfd == -1)
		return sessionfd;
	
    DSU_DEBUG_PRINT(" - accept %d (%ld-%ld)\n", sessionfd, (long) getpid(), (long) gettid());
    
	++dsu_program_state.accepted;
	
    return sessionfd;    
}

int dsu_accept4(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen, int flags) {
	DSU_DEBUG_PRINT("Accept4() (%ld-%ld)\n", (long) getpid(), (long) gettid());
    /*  For more information see dsu_accept(). */     
    
    int shadowfd = dsu_shadowfd(sockfd);
	
    int sessionfd = accept4(shadowfd, addr, addrlen, flags);
	if (sessionfd == -1)
		return sessionfd;
    
    DSU_DEBUG_PRINT(" - accept %d (%ld-%ld)\n", sessionfd, (long) getpid(), (long) gettid());
    
	++dsu_program_state.accepted;
    
    return sessionfd;   
}

int dsu_close(int sockfd) {
	DSU_DEBUG_PRINT("Close() (%ld-%ld)\n", (long) getpid(), (long) gettid());
	
    
	/* Return immediately for these file descriptors. */
    if (sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO) {
        return close(sockfd);
    }

	
	struct dsu_socket_struct * dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.sockets, sockfd);
    if (dsu_socketfd != NULL) {
		dsu_sockets_remove_fd(&dsu_program_state.sockets, sockfd);
		return close(sockfd);
	}
	

	dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
	if (dsu_socketfd != NULL) {
		dsu_sockets_remove_fd(&dsu_program_state.binds, sockfd);
        --dsu_program_state.binded;
		return close(sockfd);
	}


    /* Then it comes from accept. */
    --dsu_program_state.accepted;
	

	return close(sockfd);

}


int dsu_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
	DSU_DEBUG_PRINT("Select() (%ld-%ld)\n", (long) getpid(), (long) gettid());
    /*  Select() allows a program to monitor multiple file descriptors, waiting until one or more of the file descriptors 
        become "ready". The DSU library will modify this list by removing file descriptors that are transfered, and change
        file descriptors to their shadow file descriptors. */
    

    #if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - User listening: %d (%ld-%ld)\n", i, (long) getpid(), (long) gettid());
		}
	#endif

    
    /*  Settle between the different versions. */
    dsu_forall_sockets(dsu_program_state.binds, dsu_settle_fd, readfds);

    /*  Convert to shadow file descriptors, this must be done for binded sockets. */
    dsu_forall_sockets(dsu_program_state.binds, dsu_set_shadowfd, readfds);    
    
    /*  Sniff on internal communication.  */    
    dsu_forall_sockets(dsu_program_state.binds, dsu_sniff_conn, readfds);
    
    struct timeval tv = {2, 0};
    if (timeout != NULL) {
        tv = *timeout;
    }
    

	#if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - Listening: %d (%ld-%ld)\n", i, (long) getpid(), (long) gettid());
		}
	#endif


    int result = select(nfds, readfds, writefds, exceptfds, &tv);
    if (result < 0) return result;
	
    
	#if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - Incomming: %d (%ld-%ld)\n", i, (long) getpid(), (long) gettid());
		}
	#endif
    

    /*  Check for connections of new processes. */
    dsu_forall_sockets(dsu_program_state.binds, dsu_acc_conn, readfds);
    
    
    /*  Handle messages of new processes. */ 
    dsu_forall_sockets(dsu_program_state.binds, dsu_handle_conn, readfds);
    

    /*  Convert shadow file descriptors back to user level file descriptors to avoid changing the external behaviour. */
    dsu_forall_sockets(dsu_program_state.binds, dsu_set_originalfd, readfds);

    
    #if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - User incomming: %d (%ld-%ld)\n", i, (long) getpid(), (long) gettid());
		}
	#endif
    
	return result;
}

