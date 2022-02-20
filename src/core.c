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
//#include <sys/stat.h>
#include <sys/types.h>
//#include <sys/ipc.h>
//#include <sys/shm.h>
//#include <semaphore.h>
//#include <pthread.h>
//#include <sys/msg.h>
#include <dlfcn.h>
//#include <sys/mman.h>
//#include <stdarg.h>
#include <fcntl.h>
//#include <poll.h>


#include "core.h"
#include "state.h"
#include "communication.h"


#include "event_handlers/select.h"
//#include "event_handlers/poll.h"
#include "event_handlers/epoll.h"


/* 	Global variable containing pointers to the used data structures and state of the program. Every binded file descriptor
	will be connected to a shadow data structure including file descriptor for communication between versions, state
	management and shadow file descriptor if it is inherited from previous version. */
struct dsu_state_struct dsu_program_state;


/* 	For the functions socket(), bind(), listen(), accept(), accept4() and close() a wrapper is 
	created to maintain shadow data structures of the file descriptors. */
int (*dsu_socket)(int, int, int);
int (*dsu_bind)(int, const struct sockaddr *, socklen_t);
int (*dsu_accept)(int, struct sockaddr *restrict, socklen_t *restrict);
int (*dsu_accept4)(int, struct sockaddr *restrict, socklen_t *restrict, int);
int (*dsu_close)(int);
sighandler_t (*dsu_signal)(int signum, sighandler_t handler);
int (*dsu_sigaction)(int signum, const struct sigaction *restrict act, struct sigaction *restrict oldact);


int dsu_inherit_fd(struct dsu_socket_list *dsu_sockfd) {
	DSU_DEBUG_PRINT(" - Inherit fd %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
	/*	Connect to the previous version, based on named unix domain socket, and receive the file descriptor that is 
		binded to the same port. Also, receive the file descriptor of the named unix domain socket so internal
		communication can be taken over when the update completes. */


	DSU_DEBUG_PRINT("  - Connect on %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
    if ( connect(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, sizeof(dsu_sockfd->comfd_addr)) != -1) {
        
		
        DSU_DEBUG_PRINT("  - Send on %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
        if ( send(dsu_sockfd->comfd, &dsu_sockfd->port, sizeof(dsu_sockfd->port), 0) > 0) {


            DSU_DEBUG_PRINT("  - Receive on %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
            int port = 0; int _comfd = 0; int _sockfd;
            dsu_read_fd(dsu_sockfd->comfd, &_sockfd, &port);	// Handle return value;
			dsu_read_fd(dsu_sockfd->comfd, &_comfd, &port);
			DSU_DEBUG_PRINT("  - Received %d & %d (%d-%d)\n", _sockfd, _comfd, (int) getpid(), (int) gettid());

			if (port > 0) {
				
				/* Connect socket to the same v-node. */
				if (dup2(_sockfd, dsu_sockfd->fd) == -1) {
					return -1;
				}

				
				/* preserve comfd socket to signal ready. */
				dsu_sockfd->comfd_close = dsu_sockfd->comfd;
				dsu_sockfd->comfd = _comfd;
				
			}

			return port;
		}
	}


	return -1;
}


int dsu_termination_detection() {
	/*	Determine based on the global programming state whether it is possible to terminate. A version cannot terminate if:
		1.	One socket did not increase version.
		2.	A socket is still actively monitored.
		3.	The singlely linked list comdfd still contains open connections between different versions.
		4.	The singlely linked list fds still contains open connections with clients. */
	
	DSU_DEBUG_PRINT(" - Termination detection (%d-%d)\n", (int) getpid(), (int) gettid());
	
	struct dsu_socket_list *current = dsu_program_state.binds;

	while (current != NULL) {
		
        if (	!current->ready
			||  current->comfds != NULL
			||	current->fds != NULL
			 )
			return 0;        

		current = current->next;

	}
	
	return 1;

}


void dsu_terminate() {
	DSU_DEBUG_PRINT(" - Termination (%d-%d)\n", (int) getpid(), (int) gettid());
	/*	Different models, such as master-worker model, are used to horizontally scale the application. This
		can either be done with threads or processes. As threads are implemented as processes on linux, 
		there is not difference in termination. The number of active workers is tracked in the event handler. 
		The last active worker that terminates, terminates the group to ensure the full application stops. */
	
    
	int workers = dsu_deactivate_process();

    DSU_DEBUG_PRINT("  - Workers: %d (pg:%d, pid:%d, tid:%d)\n", workers, (int) getpgid(getpid()), (int) getpid(), (int) gettid());
	if (workers == 0) {
		DSU_DEBUG_PRINT("  - Kill all (pg:%d, pid:%d, tid:%d)\n", (int) getpgid(getpid()), (int) getpid(), (int) gettid());
		killpg(getpgid(getpid()), SIGKILL);
	}

    killpg(getpid(), SIGTERM);

}


int dsu_change_number_of_workers(int delta) {

    DSU_DEBUG_PRINT(" < Lock process file (%d-%d)\n", (int) getpid(), (int) gettid());
    fcntl(dsu_program_state.processes, F_SETLKW, dsu_program_state.write_lock);
    
    char buf[2] = {0};
    lseek(dsu_program_state.processes, 0, SEEK_SET); 
    if (read(dsu_program_state.processes, buf, 1) == -1) {
        return -1;
    }
    
    int _size = strtol(buf, NULL, 10);
    int size = _size + delta;
    DSU_DEBUG_PRINT(" - Number of processes %d to %d (%d-%d)\n", _size, size, (int) getpid(), (int) gettid());
    
    buf[0] = size + '0';
    lseek(dsu_program_state.processes, 0, SEEK_SET);
    if (write(dsu_program_state.processes, buf, 1) == -1) {
        return -1;
    }
    
    DSU_DEBUG_PRINT(" > Unlock process file (%d-%d)\n", (int) getpid(), (int) gettid());
    fcntl(dsu_program_state.processes, F_SETLKW, dsu_program_state.write_lock);
    
    return size;

}


int dsu_deactivate_process(void) {
    return dsu_change_number_of_workers(-1);
}


int dsu_activate_process(void) {
	return dsu_change_number_of_workers(1);
}
			

int dsu_monitor_init(struct dsu_socket_list *dsu_sockfd) {
	DSU_DEBUG_PRINT(" - (Try) initialize communication on  %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
	/*  This function is called when the program calls bind(). If bind fails, in normal situation, a older version exists. If 
		bind succees, start listening on the named unix domain file descriptor for connection request of newer versions. It might 
		happen fork or pthreads are used to accept connections on multiple processes and multi threads respectively. Therefore set 
		the socket to non-blocking to be able to accept connection requests without risk of indefinetely blocking. */
    
    if (dsu_bind(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, (socklen_t) sizeof(dsu_sockfd->comfd_addr)) == 0) {
        
		
        if (listen(dsu_sockfd->comfd, DSU_MAXNUMOFPROC) == 0) {
            
			
			/* 	Set socket to non-blocking, several processes might be accepting connections. */
			int flags = fcntl(dsu_sockfd->comfd, F_GETFL, 0) | O_NONBLOCK;
			fcntl(dsu_sockfd->comfd, F_SETFL, (char *) &flags);
			
			
            DSU_DEBUG_PRINT(" - Initialized communication on  %d fd: %d (%d-%d)\n", dsu_sockfd->port, dsu_sockfd->comfd, (int) getpid(), (int) gettid());

			
			return 0;
        }
    
    }
	

	return -1;     

}


static __attribute__((constructor)) void dsu_init() {
	/*	LD_Preload constructor is called before the binary starts. Initialze the program state. */

	#if defined(DEBUG)
		int size = snprintf(NULL, 0, "%s/dsu_%d.log", DEBUG, (int) getpid());
		char logfile[size+1];
		sprintf(logfile, "%s/dsu_%d.log", DEBUG, (int) getpid());
	    dsu_program_state.logfd = fopen(logfile, "w");
	    if (dsu_program_state.logfd == NULL) {
		    perror("DSU \"Error opening debugging file\"");
		    exit(EXIT_FAILURE);
	    }
	#endif
	DSU_DEBUG_PRINT("INIT() (%d-%d)\n", (int) getpid(), (int) gettid());
    

    dsu_program_state.write_lock = (struct flock *) calloc(1, sizeof(struct flock));
    dsu_program_state.write_lock->l_type = F_WRLCK;
    dsu_program_state.write_lock->l_start = 0; 
    dsu_program_state.write_lock->l_whence = SEEK_SET; 
    dsu_program_state.write_lock->l_len = 0; 

    dsu_program_state.unlock = (struct flock *) calloc(1, sizeof(struct flock));
    dsu_program_state.unlock->l_type = F_UNLCK;
    dsu_program_state.unlock->l_start = 0; 
    dsu_program_state.unlock->l_whence = SEEK_SET; 
    dsu_program_state.unlock->l_len = 0; 
    
    
    dsu_program_state.sockets = NULL;
    dsu_program_state.binds = NULL;
	
	
	/*  Wrappers around system function. RTLD_NEXT will find the next occurrence of a function in the search 
		order after the current library*/
	dsu_socket = dlsym(RTLD_NEXT, "socket");
	dsu_bind = dlsym(RTLD_NEXT, "bind");
	dsu_accept = dlsym(RTLD_NEXT, "accept");
	dsu_accept4 = dlsym(RTLD_NEXT, "accept4");
	dsu_close = dlsym(RTLD_NEXT, "close");
    dsu_sigaction = dlsym(RTLD_NEXT, "sigaction");
    dsu_signal = dlsym(RTLD_NEXT, "signal");
	
	
	/* 	Set default function for event-handler wrapper functions. */
	dsu_select = dlsym(RTLD_NEXT, "select");
	//dsu_poll = dlsym(RTLD_NEXT, "poll");
	//dsu_ppoll = dlsym(RTLD_NEXT, "ppoll");
	dsu_epoll_wait = dlsym(RTLD_NEXT, "epoll_wait");
	dsu_epoll_create1 = dlsym(RTLD_NEXT, "epoll_create1");
	dsu_epoll_create = dlsym(RTLD_NEXT, "epoll_create");
	//dsu_epoll_ctl = dlsym(RTLD_NEXT, "epoll_ctl");

    
    int len = snprintf(NULL, 0, "/tmp/dsu_processes_%d.pid", (int) getpid());
	char temp_path[len+1];
	sprintf(temp_path, "/tmp/dsu_processes_%d.pid", (int) getpid());
    dsu_program_state.processes = open(temp_path, O_RDWR | O_CREAT, 0600);
    if (dsu_program_state.processes <= 0) {
        perror("DSU \"Error opening process file\"");
		exit(EXIT_FAILURE);
    }
    unlink(temp_path);
    char buf = 0 + '0';
    if (write(dsu_program_state.processes, &buf, 1) == -1) {
        perror("DSU \"Error initiating process file\"");
		exit(EXIT_FAILURE);
    }
    
    return;
}


sighandler_t signal(int signum, sighandler_t handler) {
	DSU_DEBUG_PRINT("Signal() %d (%d-%d)\n", signum, (int) getpid(), (int) gettid());	

	
	//if (signum == SIGTERM) {
	//	return 0;
	//}
	
	
	return dsu_signal(signum, handler);

}


int sigaction(int signum, const struct sigaction *restrict act, struct sigaction *restrict oldact) {
	DSU_DEBUG_PRINT("sigaction() %d (%d-%d)\n", signum, (int) getpid(), (int) gettid());
	
	
	if (signum == SIGTERM) {
		return 0;
	}

	
	return dsu_sigaction(signum, act, oldact);
	
}


int socket(int domain, int type, int protocol) {
    DSU_DEBUG_PRINT("Socket() (%d-%d)\n", (int) getpid(), (int) gettid());
    /*  With socket() an endpoint for communication is created and returns a file descriptor that refers to that 
        endpoint. The DSU library will connect the file descriptor to a shadow file descriptor. The shadow file 
        descriptor may be recieved from running version. */

	
    int sockfd = dsu_socket(domain, type, protocol);
    if (sockfd > 0) {
        /* After successfull creation, add socket to the DSU state. */
        struct dsu_socket_list dsu_socket;
        dsu_socket_list_init(&dsu_socket);
        dsu_socket.fd = sockfd;
        dsu_sockets_add(&dsu_program_state.sockets, &dsu_socket);
    }
	
	DSU_DEBUG_PRINT(" - fd: %d(%d-%d)\n", sockfd, (int) getpid(), (int) gettid());
    
    return sockfd;
}


int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    DSU_DEBUG_PRINT("Bind() (%d-%d)\n", (int) getpid(), (int) gettid());

    /*  Bind is used to accept a client connection on an socket, this means it is a "public" socket 
        that is ready to accept requests. */
    
	
    /* Find the metadata of sockfd, and transfer the socket to the state binds. */
    struct dsu_socket_list *dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.sockets, sockfd); 
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
	DSU_DEBUG_PRINT(" - Bind port %d on %d (%d-%d)\n", dsu_socketfd->port, sockfd, (int) getpid(), (int) gettid());
    
    
    /*  Possibly communicate the socket from the older version. A bind can only be performed once on the same
        socket, therefore, processes will not communicate with each other. Abstract domain socket cannot be used 
        in portable programs. But has the advantage that it automatically disappear when all open references to 
        the socket are closed. */
    bzero(&dsu_socketfd->comfd_addr, sizeof(dsu_socketfd->comfd_addr));
    dsu_socketfd->comfd_addr.sun_family = AF_UNIX;
    sprintf(dsu_socketfd->comfd_addr.sun_path, "X%s_%d.unix", DSU_COMM, dsu_socketfd->port);    // On Linux, sun_path is 108 bytes in size.
    dsu_socketfd->comfd_addr.sun_path[0] = '\0';                                                // Abstract linux socket.
    dsu_socketfd->comfd = dsu_socket(AF_UNIX, SOCK_STREAM, 0);
    
	
	/*	Notify threads that file descriptor is successfully transfered to the new version. */
	int pair[2];
	if (socketpair(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0, pair) < 0) {
		perror("DSU \"Error generating socket pair\"");
		exit(EXIT_FAILURE);
	}   
	dsu_socketfd->readyfd = pair[0];
	dsu_socketfd->markreadyfd = pair[1];
    
    
    /*  Bind socket, if it fails and already exists we know that another DSU application is 
        already running. These applications need to know each others listening ports. */
    if ( dsu_monitor_init(dsu_socketfd) == -1) {
		
        if ( errno == EADDRINUSE ) {

            if (dsu_inherit_fd(dsu_socketfd) > 0) {
				
                dsu_sockets_transfer_fd(&dsu_program_state.binds, &dsu_program_state.sockets, dsu_socketfd);
                				
                return 0;
            }
        }
    }
    
	
	/*	No other version running. */
    dsu_sockets_transfer_fd(&dsu_program_state.binds, &dsu_program_state.sockets, dsu_socketfd);
  	
    
    return dsu_bind(sockfd, addr, addrlen);
}


int accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen) {
	DSU_DEBUG_PRINT("Accept() (%d-%d)\n", (int) getpid(), (int) gettid());   
    /*  The accept() system call is used with connection-based socket types (SOCK_STREAM, SOCK_SEQPACKET).  It extracts the first
        connection request on the queue of pending connections for the listening socket, sockfd, creates a new connected socket, and
        returns a new file descriptor referring to that socket. The DSU library need to convert the file descriptor to the shadow
        file descriptor. */
    
	
    int sessionfd = dsu_accept(sockfd, addr, addrlen);
	if (sessionfd == -1)
		return sessionfd;
	

    DSU_DEBUG_PRINT(" - accept %d (%d-%d)\n", sessionfd, (int) getpid(), (int) gettid());
	struct dsu_socket_list *dsu_sockfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
	if (dsu_sockfd != NULL)
		dsu_socket_add_fds(dsu_sockfd, sessionfd, DSU_NON_INTERNAL_FD);
	
	
    return sessionfd;    
}


int accept4(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen, int flags) {
	DSU_DEBUG_PRINT("Accept4() (%d-%d)\n", (int) getpid(), (int) gettid());
    /*  For more information see dsu_accept(). */     

    
    int sessionfd = dsu_accept4(sockfd, addr, addrlen, flags);
	if (sessionfd == -1)
		return sessionfd;

    
    DSU_DEBUG_PRINT(" - accept %d (%d-%d)\n", sessionfd, (int) getpid(), (int) gettid());
	struct dsu_socket_list *dsu_sockfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
	if (dsu_sockfd != NULL)
		dsu_socket_add_fds(dsu_sockfd, sessionfd, DSU_NON_INTERNAL_FD);
    

    return sessionfd;   
}


int close(int sockfd) {
	DSU_DEBUG_PRINT("Close() fd: %d (%d-%d)\n", sockfd, (int) getpid(), (int) gettid());
	/*	close() closes a file descriptor, so that it no longer refers to any file and may be reused. Therefore, the the shadow file descriptor
		should also be removed / closed. The file descriptor can exist in:
			1.	Unbinded sockets 	-> 	dsu_program_State.sockets
			2.	Binded sockets 		-> 	dsu_program_State.binds
			3.	Internal sockets	-> 	dsu_program_State.binds->comfd | dsu_program_State.binds->fds
			4.	Connected clients	-> 	dsu_program_state.accepted	*/
	
		
	/* 	Return immediately for these file descriptors. */
    if (sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO) {
        return dsu_close(sockfd);
    }
	
	
	struct dsu_socket_list * dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.sockets, sockfd);
    if (dsu_socketfd != NULL) {
		DSU_DEBUG_PRINT(" - Unbinded socket (%d-%d)\n", (int) getpid(), (int) gettid());
		dsu_sockets_remove_fd(&dsu_program_state.sockets, sockfd);
		return dsu_close(sockfd);
	}


	dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
	if (dsu_socketfd != NULL) {
		DSU_DEBUG_PRINT(" - Binded socket %d (%d-%d)\n",dsu_socketfd->fd, (int) getpid(), (int) gettid());
		if (dsu_socketfd->readyfd > 0) dsu_close(dsu_socketfd->readyfd);
		if (dsu_socketfd->comfd >0) dsu_close(dsu_socketfd->comfd);
		dsu_sockets_remove_fd(&dsu_program_state.binds, sockfd);
		return dsu_close(sockfd);
	}
	
	
	dsu_socketfd = dsu_sockets_search_fds(dsu_program_state.binds, sockfd, DSU_MONITOR_FD);
	if (dsu_socketfd != NULL) {
		DSU_DEBUG_PRINT(" - Internal master socket (%d-%d)\n", (int) getpid(), (int) gettid());
		return dsu_close(sockfd);
	}

	
	dsu_socketfd = dsu_sockets_search_fds(dsu_program_state.binds, sockfd, DSU_INTERNAL_FD);
	if (dsu_socketfd != NULL) {
		DSU_DEBUG_PRINT(" - Internal client socket (%d-%d)\n", (int) getpid(), (int) gettid());
		dsu_socket_remove_fds(dsu_socketfd, sockfd, DSU_INTERNAL_FD);
		return dsu_close(sockfd);
	}


	dsu_socketfd = dsu_sockets_search_fds(dsu_program_state.binds, sockfd, DSU_NON_INTERNAL_FD);
	if (dsu_socketfd != NULL) {
		DSU_DEBUG_PRINT(" - Client socket (%d-%d)\n", (int) getpid(), (int) gettid());
		dsu_socket_remove_fds(dsu_socketfd, sockfd, DSU_NON_INTERNAL_FD);
		return dsu_close(sockfd);
	}


	return dsu_close(sockfd);

}

