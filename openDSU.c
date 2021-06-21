#define _GNU_SOURCE

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
#include <dlfcn.h>

#include "openDSU.h"

#if DSU_DEBUG == 1
#define DSU_DEBUG_PRINT(format, ...) {printf(format, ## __VA_ARGS__); fprintf(dsu_program_state.logfd, format, ## __VA_ARGS__); fflush(dsu_program_state.logfd);}
#else
#define DSU_DEBUG_PRINT(format, ...)
#endif


/* Global state variable */
struct dsu_state_struct dsu_program_state;


int (*dsu_socket)(int, int, int);
int (*dsu_bind)(int, const struct sockaddr *, socklen_t);
int (*dsu_listen)(int, int);
int (*dsu_accept)(int, struct sockaddr *restrict, socklen_t *restrict);
int (*dsu_accept4)(int, struct sockaddr *restrict, socklen_t *restrict, int);
int (*dsu_close)(int);
int (*dsu_select)(int, fd_set *, fd_set *, fd_set *, struct timeval *);


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
    struct dsu_comfd_struct **prev_comfds = comfds;
    struct dsu_comfd_struct **cur_comfds = &(*comfds)->next;
    
    while (*cur_comfds != NULL) {
        if ((*cur_comfds)->value == comfd) {
            (*prev_comfds)->next = (*cur_comfds)->next;
            free(*cur_comfds);
            return;
        }
        prev_comfds = cur_comfds;
        cur_comfds = &(*cur_comfds)->next;
    }

}


struct dsu_socket_struct *dsu_sockets_search_comfd(struct dsu_sockets_struct *socket, int sockfd, int flag) {
	/*	List in list. */    

    while (socket != NULL) {
		
		if (flag == DSU_MONITOR)
			if (socket->value.comfd == sockfd && socket->value.monitoring) return &socket->value;
		
		if (flag == DSU_NONMONITOR) {
			struct dsu_comfd_struct *comfds = socket->value.comfds;

			while(comfds != NULL) {
			
				if (comfds->value == sockfd) return &socket->value;
				
				comfds = comfds->next;
			}
			
		}

		socket = socket->next;
    }
    
    return NULL;
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


int dsu_request_fd(struct dsu_socket_struct *dsu_sockfd) {
	DSU_DEBUG_PRINT(" - Request fd %d (%ld-%ld)\n", dsu_sockfd->comfd, (long) getpid(), (long) gettid()); 


	DSU_DEBUG_PRINT("  - Connect on %d (%ld-%ld)\n", dsu_sockfd->comfd, (long) getpid(), (long) gettid()); 
    /*  An older version of the program is running, connect to this application over Unix domain socket. */
    if ( connect(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, sizeof(dsu_sockfd->comfd_addr)) != -1) {
        
		
        DSU_DEBUG_PRINT("  - Send on %d (%ld-%ld)\n", dsu_sockfd->comfd, (long) getpid(), (long) gettid());           
        /*  Ask the running program for the file descriptor of corresponding port. */
        if ( send(dsu_sockfd->comfd, &dsu_sockfd->port, sizeof(dsu_sockfd->port), 0) > 0) {


            DSU_DEBUG_PRINT("  - Recieve on %d (%ld-%ld)\n", dsu_sockfd->comfd, (long) getpid(), (long) gettid());
            /*  Recieve file descriptor from running version version. */
            int port = 0; int _comfd = 0;
            dsu_read_fd(dsu_sockfd->comfd, &dsu_sockfd->shadowfd, &port);
			dsu_read_fd(dsu_sockfd->comfd, &_comfd, &port);
			

			close(dsu_sockfd->comfd);
			dsu_sockfd->comfd = _comfd;


			return port;
		}
	}


	return -1;
}


int dsu_termination_detection() {
	
	DSU_DEBUG_PRINT(" - Termination detection (%ld-%ld)\n", (long) getpid(), (long) gettid());
	
	struct dsu_sockets_struct *current = dsu_program_state.binds;

	while (current != NULL) {

		if (	current->value.version >= current->value.status[DSU_VERSION]
			|| 	current->value.monitoring != 0 
			|| 	current->value.status[DSU_PGID] == getpgid(getpid())
			||  current->value.comfds != NULL
			||	dsu_program_state.accepted > 0
			 )
			return 0;

		current = current->next;

	}
	
	return 1;     

}


void dsu_terminate() {
	
	DSU_DEBUG_PRINT(" - Exit (%ld-%ld)\n", (long) getpid(), (long) gettid());
    exit(EXIT_SUCCESS);     

}    
			

int dsu_monitor_init(struct dsu_socket_struct *dsu_sockfd) {	
	
	/*  Bind() and listen() are thread safe. Only one process or thread will be able to reserve the port. */
    DSU_DEBUG_PRINT(" - Try initialize communication on  %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
    if (dsu_bind(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, (socklen_t) sizeof(dsu_sockfd->comfd_addr)) == 0) {
        
        if (dsu_listen(dsu_sockfd->comfd, MAXNUMOFPROC) == 0) {
            
			fcntl(dsu_sockfd->comfd, F_SETFL, fcntl(dsu_sockfd->comfd, F_GETFL, 0) | O_NONBLOCK); // Set non-blocking, several might be accepting connections.
			
            DSU_DEBUG_PRINT(" - Initialized communication on  %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());

			return 0;
        }
    
    }
	

	return -1;     

}


void dsu_monitor_fd(struct dsu_socket_struct *dsu_sockfd) {
	
	DSU_DEBUG_PRINT(" - Monitor on %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());

	
	DSU_DEBUG_PRINT("  - Lock status %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
	sem_wait(dsu_sockfd->status_sem);

			
	/*	Quit monitoring by older generation. */
	if (	dsu_sockfd->version < dsu_sockfd->status[DSU_VERSION] 
		&&	dsu_sockfd->monitoring 
	) {
		
		DSU_DEBUG_PRINT("  - Quit monitoring  %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
		dsu_close(dsu_sockfd->comfd);
		dsu_sockfd->monitoring = 0;
		dsu_sockfd->status[DSU_TRANSFER] = 0;
	
	}


	/*	Takeover monitoring from older generation. */
	if (dsu_sockfd->version > dsu_sockfd->status[DSU_VERSION]) {
		
		DSU_DEBUG_PRINT("  - Increase version  %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());				
		dsu_sockfd->status[DSU_PGID] = getpgid(getpid());
		++dsu_sockfd->status[DSU_VERSION];
		
	}
	
		
	DSU_DEBUG_PRINT("  - Unlock status %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
	sem_post(dsu_sockfd->status_sem);
	
}


void dsu_sniff_conn(struct dsu_socket_struct *dsu_sockfd, fd_set *readfds) {
    /*  Listen under the hood to accepted connections on public socket. A new generation can request
        file descriptors. During DUAL listening, only one  */
    

	
	if (dsu_sockfd->monitoring) {
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


void dsu_handle_conn(struct dsu_socket_struct *dsu_sockfd, fd_set *readfds) {
	
    DSU_DEBUG_PRINT(" - Handle on %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
	
	/*  Race conditions could happend when a "late" fork is performed. The fork happend after 
        accepting dsu communication connection. */
	if (FD_ISSET(dsu_sockfd->comfd, readfds)) {
		
		if (dsu_sockfd->monitoring) {
				    
			int size = sizeof(dsu_sockfd->comfd_addr);
			int acc = dsu_accept4(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, (socklen_t *) &size, SOCK_NONBLOCK);
			
			if ( acc != -1) {
			    DSU_DEBUG_PRINT("  - Accept %d on %d (%ld-%ld)\n", acc, dsu_sockfd->comfd, (long) getpid(), (long) gettid());
			    dsu_socket_add_comfd(dsu_sockfd, acc);
			} else
				DSU_DEBUG_PRINT("  - Accept failed (%ld-%ld)\n", (long) getpid(), (long) gettid());
			
		}
		
		DSU_DEBUG_PRINT("  - remove: %d (%ld-%ld)\n", dsu_sockfd->comfd, (long) getpid(), (long) gettid());
		FD_CLR(dsu_sockfd->comfd, readfds);
	}
	

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
                                 
                dsu_close(comfds->value);
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
                port = -1;
            
            
			if (port != -1) {
				
				DSU_DEBUG_PRINT(" - Lock status %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
				sem_wait(dsu_sockfd->status_sem);
				dsu_sockfd->status[DSU_TRANSFER] = 1;
				DSU_DEBUG_PRINT(" - Unlock status %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
				sem_post(dsu_sockfd->status_sem);
				
			}

			
            DSU_DEBUG_PRINT(" - Message on %d (%ld-%ld)\n", comfds->value, (long) getpid(), (long) gettid());
            dsu_write_fd(comfds->value, dsu_sockfd->shadowfd, port);
			dsu_write_fd(comfds->value, dsu_sockfd->comfd, port);
			
			
        }
        
        dsu_next_comfd:
            DSU_DEBUG_PRINT(" - remove %d (%ld-%ld)\n", comfds->value, (long) getpid(), (long) gettid());
            FD_CLR(comfds->value, readfds);
            comfds = comfds->next;
                
    }

}


void dsu_set_shadowfd(struct dsu_socket_struct *dsu_sockfd, fd_set *readfds) {
    /*  Change socket file descripter to its shadow file descriptor. */   
   
    if (FD_ISSET(dsu_sockfd->sockfd, readfds) && dsu_sockfd->monitoring) {

        
        FD_CLR(dsu_sockfd->sockfd, readfds);
		
		DSU_DEBUG_PRINT(" - Lock status %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
		sem_wait(dsu_sockfd->status_sem);

		if (dsu_sockfd->status[DSU_TRANSFER]) {
        	
			DSU_DEBUG_PRINT(" - Try lock fd %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
		    if (sem_trywait(dsu_sockfd->fd_sem) == 0) {
		        
				DSU_DEBUG_PRINT(" - Lock fd %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
		        dsu_sockfd->locked = DSU_LOCKED;

		        DSU_DEBUG_PRINT(" - Set %d => %d (%ld-%ld)\n", dsu_sockfd->sockfd, dsu_sockfd->shadowfd, (long) getpid(), (long) gettid());
		        FD_SET(dsu_sockfd->shadowfd, readfds);
			}
        
		} else {
			
			DSU_DEBUG_PRINT(" - Set %d => %d (%ld-%ld)\n", dsu_sockfd->sockfd, dsu_sockfd->shadowfd, (long) getpid(), (long) gettid());
            FD_SET(dsu_sockfd->shadowfd, readfds);

		}
		
		DSU_DEBUG_PRINT(" - Unlock status %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
		sem_post(dsu_sockfd->status_sem);
		
		
        
    }
	
}


void dsu_set_originalfd(struct dsu_socket_struct *dsu_sockfd, fd_set *readfds) {
    /*  Change shadow file descripter to its original file descriptor. */
    
    if (dsu_sockfd->locked == DSU_LOCKED) {
        DSU_DEBUG_PRINT(" - Unlock fd %d (%ld-%ld)\n", dsu_sockfd->port, (long) getpid(), (long) gettid());
        sem_post(dsu_sockfd->fd_sem);
        dsu_sockfd->locked = DSU_UNLOCKED;
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
	dsu_socket->monitoring	= 0;
	dsu_socket->status_sem	= 0;
	dsu_socket->fd_sem		= 0;	
	dsu_socket->locked		= 0;

}


static __attribute__((constructor)) void dsu_init() {

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

    dsu_program_state.accepted = 0;

	/*  Wrappers around system function. RTLD_NEXT will find the next occurrence of a function in the search 
		order after the current library*/
	dsu_socket = dlsym(RTLD_NEXT, "socket");
	dsu_bind = dlsym(RTLD_NEXT, "bind");
	dsu_listen = dlsym(RTLD_NEXT, "listen");
	dsu_accept = dlsym(RTLD_NEXT, "accept");
	dsu_accept4 = dlsym(RTLD_NEXT, "accept4");
	dsu_close = dlsym(RTLD_NEXT, "close");
	dsu_select = dlsym(RTLD_NEXT, "select");

    return;
}



int socket(int domain, int type, int protocol) {
    DSU_DEBUG_PRINT("Socket() (%ld-%ld)\n", (long) getpid(), (long) gettid());
    /*  With socket() an endpoint for communication is created and returns a file descriptor that refers to that 
        endpoint. The DSU library will connect the file descriptor to a shadow file descriptor. The shadow file 
        descriptor may be recieved from running version. */

    int sockfd = dsu_socket(domain, type, protocol);
    if (sockfd > 0) {
        /* After successfull creation, add socket to the DSU state. */
        struct dsu_socket_struct dsu_socket;
        dsu_socket_struct_init(&dsu_socket);
        dsu_socket.shadowfd = dsu_socket.sockfd = sockfd;
        dsu_sockets_add(&dsu_program_state.sockets, dsu_socket);
    }
    
    return sockfd;
}



int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
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
    key_t status_key = ftok(pathname, 1);
    int shmid = shmget(status_key, 3*sizeof(long), 0666|IPC_CREAT);
    dsu_socketfd->status = (long *) shmat(shmid, NULL, 0);


	/*  During the transition a lock is needed when the accepting file descriptor is blocking. */
    int pathname_size_status_sem = snprintf(NULL, 0, "/%s_%d_status.semaphore", DSU_COMM, dsu_socketfd->port);
    char pathname_status_sem[pathname_size_status_sem+1];
    sprintf(pathname_status_sem, "%s_%d", DSU_COMM, dsu_socketfd->port);


	/*  During the transition a lock is needed. */
    int pathname_size_fd_sem = snprintf(NULL, 0, "/%s_%d_key.semaphore", DSU_COMM, dsu_socketfd->port);
    char pathname_fd_sem[pathname_size_fd_sem+1];
    sprintf(pathname_fd_sem, "%s_%d", DSU_COMM, dsu_socketfd->port);
    
    
    /*  Bind socket, if it fails and already exists we know that another DSU application is 
        already running. These applications need to know each others listening ports. */
    if ( dsu_monitor_init(dsu_socketfd) == -1) {
		
        if ( errno == EADDRINUSE ) {

            if (dsu_request_fd(dsu_socketfd) > 0) {
                
				dsu_socketfd->monitoring = 1; // Communication file descriptor is inherited.
				
				dsu_socketfd->status_sem = sem_open(pathname_status_sem, O_CREAT, S_IRWXO | S_IRWXG | S_IRWXU, 1);
				dsu_socketfd->fd_sem = sem_open(pathname_fd_sem, O_CREAT, S_IRWXO | S_IRWXG | S_IRWXU, 1);
				
				DSU_DEBUG_PRINT(" - Lock status %d (%ld-%ld)\n", dsu_socketfd->port, (long) getpid(), (long) gettid());
				if (sem_wait(dsu_socketfd->status_sem) == 0) {
					dsu_socketfd->version = dsu_socketfd->status[DSU_VERSION];
					if(dsu_socketfd->status[DSU_PGID] != getpgid(getpid())) // Could be a delayed process.
						++dsu_socketfd->version;
					DSU_DEBUG_PRINT(" - version %ld (%ld-%ld)\n", dsu_socketfd->version, (long) getpid(), (long) gettid());
					DSU_DEBUG_PRINT(" - Unlock status %d (%ld-%ld)\n", dsu_socketfd->port, (long) getpid(), (long) gettid());
					sem_post(dsu_socketfd->status_sem);
				}
				
                dsu_sockets_transfer_fd(&dsu_program_state.binds, &dsu_program_state.sockets, dsu_socketfd);
                				
                return 0;
            }
        }
    }
    
		 	
	sem_unlink(pathname_status_sem); // Semaphore does not terminate after exit.
    dsu_socketfd->status_sem = sem_open(pathname_status_sem, O_CREAT | O_EXCL, S_IRWXO | S_IRWXG | S_IRWXU, 1);
    sem_init(dsu_socketfd->status_sem, PTHREAD_PROCESS_SHARED, 1);

	
	sem_unlink(pathname_fd_sem); // Semaphore does not terminate after exit.
    dsu_socketfd->fd_sem = sem_open(pathname_fd_sem, O_CREAT | O_EXCL, S_IRWXO | S_IRWXG | S_IRWXU, 1);
    sem_init(dsu_socketfd->fd_sem, PTHREAD_PROCESS_SHARED, 1);

	dsu_socketfd->monitoring = 1;
	
	DSU_DEBUG_PRINT(" - Lock status %d (%ld-%ld)\n", dsu_socketfd->port, (long) getpid(), (long) gettid());
	if (sem_wait(dsu_socketfd->status_sem) == 0) {
		dsu_socketfd->status[DSU_PGID] = getpgid(getpid());
		dsu_socketfd->version = dsu_socketfd->status[DSU_VERSION] = 0;
		dsu_socketfd->status[DSU_TRANSFER] = 0;
		DSU_DEBUG_PRINT(" - Unlock status %d (%ld-%ld)\n", dsu_socketfd->port, (long) getpid(), (long) gettid());
		sem_post(dsu_socketfd->status_sem);
	}

    dsu_sockets_transfer_fd(&dsu_program_state.binds, &dsu_program_state.sockets, dsu_socketfd);
   	
    
    return dsu_bind(sockfd, addr, addrlen);
}

int listen(int sockfd, int backlog) {
    DSU_DEBUG_PRINT("Listen() on fd %d (%ld-%ld)\n", sockfd, (long) getpid(), (long) gettid());
    /*  listen() marks the socket referred to by sockfd as a passive socket, that is, as a socket that will be
        used to accept incoming connection requests using accept(). */
    
    struct dsu_socket_struct *dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
    
    /*  The file descriptor is not inherited. */
    if (dsu_socketfd == NULL || dsu_socketfd->monitoring == 1) {
        return dsu_listen(sockfd, sockfd);
    }
    
    /*  The socket is recieved from the older generation and listen() does not need to be called. */
	DSU_DEBUG_PRINT(" - Shadow fd: %d (%ld-%ld)\n", dsu_socketfd->shadowfd, (long) getpid(), (long) gettid());
    return 0;
}

int accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen) {
	DSU_DEBUG_PRINT("Accept() (%ld-%ld)\n", (long) getpid(), (long) gettid());   
    /*  The accept() system call is used with connection-based socket types (SOCK_STREAM, SOCK_SEQPACKET).  It extracts the first
        connection request on the queue of pending connections for the listening socket, sockfd, creates a new connected socket, and
        returns a new file descriptor referring to that socket. The DSU library need to convert the file descriptor to the shadow
        file descriptor. */
    
    int shadowfd = dsu_shadowfd(sockfd);
    
    int sessionfd = dsu_accept(shadowfd, addr, addrlen);
	if (sessionfd == -1)
		return sessionfd;
	
    DSU_DEBUG_PRINT(" - accept %d (%ld-%ld)\n", sessionfd, (long) getpid(), (long) gettid());
    
	++dsu_program_state.accepted;
	
    return sessionfd;    
}

int accept4(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen, int flags) {
	DSU_DEBUG_PRINT("Accept4() (%ld-%ld)\n", (long) getpid(), (long) gettid());
    /*  For more information see dsu_accept(). */     
    
    int shadowfd = dsu_shadowfd(sockfd);
	
    int sessionfd = dsu_accept4(shadowfd, addr, addrlen, flags);
	if (sessionfd == -1)
		return sessionfd;
    
    DSU_DEBUG_PRINT(" - accept %d (%ld-%ld)\n", sessionfd, (long) getpid(), (long) gettid());
    
	++dsu_program_state.accepted;
    
    return sessionfd;   
}

int close(int sockfd) {
	DSU_DEBUG_PRINT("Close() (%ld-%ld)\n", (long) getpid(), (long) gettid());
	
	
	/* 	Return immediately for these file descriptors. */
    if (sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO) {
        return dsu_close(sockfd);
    }
	
	
	/* 	Keep consistent state of the sockets. */
	struct dsu_socket_struct * dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.sockets, sockfd);
    if (dsu_socketfd != NULL) {
		dsu_sockets_remove_fd(&dsu_program_state.sockets, sockfd);
		return dsu_close(sockfd);
	}
	dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
	if (dsu_socketfd != NULL) {
		dsu_sockets_remove_fd(&dsu_program_state.binds, sockfd);
		return dsu_close(sockfd);
	}
	
	
	/*	Handle close on internal file descriptors. */
	dsu_socketfd = dsu_sockets_search_comfd(dsu_program_state.binds, sockfd, DSU_MONITOR);
	if (dsu_socketfd != NULL) {
		dsu_socketfd->monitoring = 0;
		return dsu_close(sockfd);
	}
	
	dsu_socketfd = dsu_sockets_search_comfd(dsu_program_state.binds, sockfd, DSU_NONMONITOR);
	if (dsu_socketfd != NULL) {
		dsu_socket_remove_comfd(dsu_socketfd, sockfd);
		return dsu_close(sockfd);
	}
	

	/* Normal connection is closed. */
	--dsu_program_state.accepted;


	return dsu_close(sockfd);

}


int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
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
	

	/* 	Mark version of the file descriptors. */
	dsu_forall_sockets(dsu_program_state.binds, dsu_monitor_fd);

	if (dsu_termination_detection()) {
		dsu_terminate();
	}
	
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


    int result = dsu_select(nfds, readfds, writefds, exceptfds, &tv);
    if (result < 0) return result;
	
    
	#if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - Incomming: %d (%ld-%ld)\n", i, (long) getpid(), (long) gettid());
		}
	#endif
    
    
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

