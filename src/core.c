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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/msg.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <poll.h>

#include "core.h"
#include "state.h"
#include "communication.h"
#include "wrapper.h"
#include "file.h"
#include "log.h"

#include "event_handlers/select.h"
#include "event_handlers/poll.h"
#include "event_handlers/epoll.h"


/* 	Global variable containing pointers to the used data structures and state of the program. Every binded file descriptor
	will be connected to a shadow data structure including file descriptor for communication between versions, state
	management and shadow file descriptor if it is inherited from previous version. */
struct dsu_state_struct dsu_program_state;


//sighandler_t dsu_handler;
//struct sigaction dsu_sigaction;


/* 	For the functions socket(), bind(), listen(), accept(), accept4() and close() a wrapper is 
	created to maintain shadow data structures of the file descriptors. */
int (*dsu_bind)(int, const struct sockaddr *, socklen_t);
int (*dsu_listen)(int, int);
int (*dsu_accept)(int, struct sockaddr *restrict, socklen_t *restrict);
int (*dsu_accept4)(int, struct sockaddr *restrict, socklen_t *restrict, int);
int (*dsu_close)(int);
int (*dsu_dup)(int);
int (*dsu_dup2)(int, int);
int (*dsu_dup3)(int, int, int);
int (*dsu_fcntl)(int, int, char *);


int dsu_inherit_fd(struct dsu_socket_list *dsu_sockfd) {
	DSU_DEBUG_PRINT(" - Inherit fd %d (%d)\n", dsu_sockfd->comfd, (int) getpid());
	/*	Connect to the previous version, based on named unix domain socket, and receive the file descriptor that is 
		binded to the same port. Also, receive the file descriptor of the named unix domain socket so internal
		communication can be taken over when the update completes. */


	DSU_DEBUG_PRINT("  - Connect on %d (%d)\n", dsu_sockfd->comfd, (int) getpid());
    if ( connect(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, sizeof(dsu_sockfd->comfd_addr)) != -1) {
        
		
        DSU_DEBUG_PRINT("  - Send on %d (%d)\n", dsu_sockfd->comfd, (int) getpid());
        if ( dsu_send(dsu_sockfd->comfd, &dsu_sockfd->port, sizeof(dsu_sockfd->port), 0) > 0) {


            DSU_DEBUG_PRINT("  - Receive on %d (%d)\n", dsu_sockfd->comfd, (int) getpid());
            int port = 0; int _comfd = 0;
            dsu_read_fd(dsu_sockfd->comfd, &dsu_sockfd->shadowfd, &port);	// Handle return value;
            dsu_read_fd(dsu_sockfd->comfd, &_comfd, &port);
	    	DSU_DEBUG_PRINT("  - Received %d & %d (%d)\n", dsu_sockfd->shadowfd, _comfd, (int) getpid());

	
	    DSU_DEBUG_PRINT("  - Close %d (%d)\n", dsu_sockfd->comfd, (int) getpid());
	    dsu_close(dsu_sockfd->comfd);
	    dsu_sockfd->comfd = _comfd;


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
	
	DSU_DEBUG_PRINT(" - Termination detection (%d)\n", (int) getpid());
	
	struct dsu_socket_list *current = dsu_program_state.binds;

	while (current != NULL) {
		
		/* 	In case of error do not terminate. */
		int version = current->version;
		
		DSU_DEBUG_PRINT("  < Lock status %d (%d)\n", current->port, (int) getpid());
		if (sem_wait(current->status_sem) == 0) {
		
			version = current->status[DSU_VERSION];

			DSU_DEBUG_PRINT("  > Unlock status %d (%d)\n", current->port, (int) getpid());
			sem_post(current->status_sem);
		}

		
		if (	current->version >= version
			|| 	current->monitoring != 0
			||  current->comfds != NULL
			||	current->fds != NULL
			 )
			return 0;	

		current = current->next;

	}
	
	return 1;

}


void dsu_terminate() {
	DSU_DEBUG_PRINT(" - Termination (%d)\n", (int) getpid());
	/*	Different models, such as master-worker model, are used to horizontally scale the application. This
		can either be done with threads or processes. As threads are implemented as processes on linux, 
		there is not difference in termination. The number of active workers is tracked in the event handler. 
		The last active worker that terminates, terminates the group to ensure the full application stops. */
	
    
	/* 	In case of error do not terminate all processes. */
	int workers  = 1;
	
    
	DSU_DEBUG_PRINT(" < Lock program state (%d)\n", (int) getpid());
	if (sem_wait(dsu_program_state.lock) == 0) {
		
		workers = --dsu_program_state.workers[0];	
			
		DSU_DEBUG_PRINT(" > Unlock program state (%d)\n", (int) getpid());
		sem_post(dsu_program_state.lock);
	}


	if (workers == 0) {
		DSU_DEBUG_PRINT("  - All (%d)\n", (int) getpid());
		killpg(getpgid(getpid()), SIGKILL);
	}


	exit(EXIT_SUCCESS);

}


void dsu_activate_process(void) {
		
	DSU_DEBUG_PRINT(" < Lock program state (%d)\n", (int) getpid());
	if( sem_wait(dsu_program_state.lock) == 0) {
		
		++dsu_program_state.workers[0];

		DSU_DEBUG_PRINT(" > Unlock program state (%d)\n", (int) getpid());
		sem_post(dsu_program_state.lock);
	}
	
}


void dsu_configure_process(void) {
	dsu_forall_sockets(dsu_program_state.binds, dsu_configure_socket);
}


			

int dsu_monitor_init(struct dsu_socket_list *dsu_sockfd) {
	DSU_DEBUG_PRINT(" - (Try) initialize communication on  %d (%d)\n", dsu_sockfd->port, (int) getpid());
	/*  This function is called when the program calls bind(). If bind fails, in normal situation, a older version exists. If 
		bind succees, start listening on the named unix domain file descriptor for connection request of newer versions. It might 
		happen fork or pthreads are used to accept connections on multiple processes and multi threads respectively. Therefore set 
		the socket to non-blocking to be able to accept connection requests without risk of indefinetely blocking. */
    
    if (dsu_bind(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, (socklen_t) sizeof(dsu_sockfd->comfd_addr)) == 0) {
        
		
        if (dsu_listen(dsu_sockfd->comfd, DSU_MAXNUMOFPROC) == 0) {
            
			
			/* 	Set socket to non-blocking, several processes might be accepting connections. */
			int flags = dsu_fcntl(dsu_sockfd->comfd, F_GETFL, 0) | O_NONBLOCK;
			dsu_fcntl(dsu_sockfd->comfd, F_SETFL, (char *) &flags);
			
			
            DSU_DEBUG_PRINT(" - Initialized communication on  %d fd: %d (%d)\n", dsu_sockfd->port, dsu_sockfd->comfd, (int) getpid());

			
			return 0;
        }
    
    }
	

	return -1;     

}


void dsu_monitor_fd(struct dsu_socket_list *dsu_sockfd) {
	DSU_DEBUG_PRINT(" - Monitor on %d (%d)\n", dsu_sockfd->port, (int) getpid());
	/*	Act on the state, in shared memory, accourding to the flow. If processes noticed that the verison of a file descriptor
		increased, it could stop listening on this file descriptor. When its version is higher than the current version, update 
		the version (this is done after starting to listen on the socket). Because shared memory is updates, lock is required.  */

	
	DSU_DEBUG_PRINT("  < Lock status %d (%d)\n", dsu_sockfd->port, (int) getpid());
	if (sem_wait(dsu_sockfd->status_sem) == 0) {


		if (	dsu_sockfd->version < dsu_sockfd->status[DSU_VERSION] 
			&&	dsu_sockfd->monitoring
			&&	!dsu_sockfd->locked
		) {
			
			DSU_DEBUG_PRINT("  - Quit monitoring  %d (%d)\n", dsu_sockfd->port, (int) getpid());
			dsu_close(dsu_sockfd->comfd);
			dsu_sockfd->monitoring = 0;
			--dsu_sockfd->status[DSU_TRANSFER];
			dsu_sockfd->transfer = 0;
		
		}


		if (	dsu_sockfd->version > dsu_sockfd->status[DSU_VERSION]
			&&	dsu_sockfd->monitoring) {
			
			DSU_DEBUG_PRINT("  - Increase version  %d (%d)\n", dsu_sockfd->port, (int) getpid());				
			dsu_sockfd->status[DSU_PGID] = getpgid(getpid());
			++dsu_sockfd->status[DSU_VERSION];
			
		}
	
		
		DSU_DEBUG_PRINT("  > Unlock status %d (%d)\n", dsu_sockfd->port, (int) getpid());
		sem_post(dsu_sockfd->status_sem);
	
	}
}


int dsu_is_blocking(int fd) {
    
    int flags = dsu_fcntl(fd, F_GETFL, 0);
    if ( !(flags & O_NONBLOCK) ) return 0;
	
    return 1;

}


void dsu_configure_socket(struct dsu_socket_list *dsu_sockfd) {
    dsu_sockfd->blocking = dsu_is_blocking(dsu_sockfd->shadowfd);
}


#define DSU_INITIALIZE_EVENT dsu_initialize_event()
void dsu_initialize_event(void) {
	
	/* 	On first call event handler call, mark worker active and configure binded sockets. */
	if (!dsu_program_state.live) {	
		
		dsu_activate_process();
		dsu_configure_process();
		
		/*	Process is initialized. */
		dsu_program_state.live = 1;
	}
	
	
	/* 	State handler binded sockets. */
	dsu_forall_sockets(dsu_program_state.binds, dsu_monitor_fd);

	
	/* 	Termination detection & execution. */
	if (dsu_termination_detection()) {
		dsu_terminate();
	}
	
}


void dsu_accept_internal_connection(struct dsu_socket_list *dsu_sockfd) {
	
	int size = sizeof(dsu_sockfd->comfd_addr);
	int acc = dsu_accept4(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, (socklen_t *) &size, SOCK_NONBLOCK);
		
		
	if ( acc != -1) {
	    DSU_DEBUG_PRINT("  - Accept %d on %d (%d)\n", acc, dsu_sockfd->comfd, (int) getpid());
	    dsu_socket_add_fds(dsu_sockfd, acc, DSU_INTERNAL_FD);
	} else
		DSU_DEBUG_PRINT("  - Accept failed (%d)\n", (int) getpid());
	
}


struct dsu_fd_list *dsu_respond_internal_connection(struct dsu_socket_list *dsu_sockfd, struct dsu_fd_list *comfds) {

	
	int buffer = 0;
    int port = dsu_sockfd->port;
    
    
    /*  Race condition on recv, hence do not wait, and continue on error. */
    int r = dsu_recv(comfds->fd, &buffer, sizeof(buffer), MSG_DONTWAIT);
    
    
    /*  Connection is closed by client. */
    if (r == 0) {
        
        DSU_DEBUG_PRINT(" - Close on %d (%d)\n", comfds->fd, (int) getpid());            
        dsu_close(comfds->fd);
        
        struct dsu_fd_list *_comfds = comfds;
        comfds = comfds->next;
        dsu_socket_remove_fds(dsu_sockfd, _comfds->fd, DSU_INTERNAL_FD);
        
		return comfds;
    } 
    
    
    /*  Other process already read message. */
    else if (r == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return comfds = comfds->next;
    
    
    /*  Unkown error. */
    else if (r == -1) {
        port = -1;
    }
    

	if (port != -1) {
		

		DSU_DEBUG_PRINT(" < Lock status %d (%d)\n", dsu_sockfd->port, (int) getpid());
		sem_wait(dsu_sockfd->status_sem);
		

		DSU_DEBUG_PRINT(" - DSU_TRANSFER %d (%d)\n", dsu_sockfd->port, (int) getpid());
		/* Possible multiple processes respond to requests. */
		if (dsu_sockfd->transfer == 0) {
			++dsu_sockfd->status[DSU_TRANSFER]; 
			dsu_sockfd->transfer = 1;
		}
		

		DSU_DEBUG_PRINT(" > Unlock status %d (%d)\n", dsu_sockfd->port, (int) getpid());
		sem_post(dsu_sockfd->status_sem);
		
	}


	DSU_DEBUG_PRINT(" - Send file descriptors on %d (%d)\n", comfds->fd, (int) getpid());
    dsu_write_fd(comfds->fd, dsu_sockfd->shadowfd, port); 	// Listening socket.
	dsu_write_fd(comfds->fd, dsu_sockfd->comfd, port); 		// Internal socket.

	
	return comfds = comfds->next;
}



static __attribute__((constructor)) void dsu_init() {
	/*	LD_Preload constructor is called before the binary starts. Initialze the program state. */
	

	#ifdef DEBUG
    int size = snprintf(NULL, 0, "%s/dsu_%d.log", DEBUG, (int) getpid());
	char logfile[size+1];	
	sprintf(logfile, "%s/dsu_%d.log", DEBUG, (int) getpid());
	dsu_program_state.logfd = fopen(logfile, "w");
	if (dsu_program_state.logfd == NULL) {
		perror("DSU \"Error opening debugging file\"");
		exit(EXIT_FAILURE);
	}
	#endif
	DSU_DEBUG_PRINT("INIT() (%d)\n", (int) getpid());


    dsu_program_state.binds = NULL;

			
	dsu_program_state.live = 0;
	

	/* 	Create shared memory, to trace number of active worker processes. */
    int pathname_size = snprintf(NULL, 0, "/%s_%d.state", DSU_COMM, (int) getpgid(getpid()));
    char pathname[pathname_size+1];
    sprintf(pathname, "/%s_%d.state", DSU_COMM, (int) getpgid(getpid()));
	
	shm_unlink(pathname);
	
	int shfd = shm_open(pathname, O_CREAT | O_RDWR | O_TRUNC, O_RDWR);
	if (shfd == -1) {
		perror("DSU \"Error creating shared memory\"");
		exit(EXIT_FAILURE);
	}

  	if (ftruncate(shfd, sizeof(int)) == -1) {
	 	perror("DSU \"Error creating shared memory\"");
		exit(EXIT_FAILURE);
	}

	dsu_program_state.workers = (int *) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shfd, 0);
	if (dsu_program_state.workers == (void *) -1) {
		perror("DSU \"Error creating shared memory\"");
		exit(EXIT_FAILURE);
	}

	dsu_program_state.workers[0] = 0;


	/*	Create semaphore in shared memory to avoid race conditions during modifications. */
    int pathname_size_sem = snprintf(NULL, 0, "/%s_%d.lock", DSU_COMM, (int) getpgid(getpid()));
    char pathname_sem[pathname_size_sem+1];
    sprintf(pathname_sem, "/%s_%d.lock", DSU_COMM, (int) getpgid(getpid()));
	
	sem_unlink(pathname_sem); // Semaphore does not terminate after exit.
    
	dsu_program_state.lock = sem_open(pathname_sem, O_CREAT | O_EXCL, S_IRWXO | S_IRWXG | S_IRWXU, 1);
    
	sem_init(dsu_program_state.lock, PTHREAD_PROCESS_SHARED, 1);
	
	
	/*  Wrappers around system function. RTLD_NEXT will find the next occurrence of a function in the search 
		order after the current library*/

	#ifdef DEBUG
	dsu_socket = dlsym(RTLD_NEXT, "socket");
	#endif

	dsu_bind = dlsym(RTLD_NEXT, "bind");
	dsu_listen = dlsym(RTLD_NEXT, "listen");
	dsu_accept = dlsym(RTLD_NEXT, "accept");
	dsu_accept4 = dlsym(RTLD_NEXT, "accept4");
	dsu_shutdown = dlsym(RTLD_NEXT, "shutdown");
	dsu_close = dlsym(RTLD_NEXT, "close");
	dsu_fcntl = dlsym(RTLD_NEXT, "fcntl");
	dsu_ioctl = dlsym(RTLD_NEXT, "ioctl");
	dsu_dup = dlsym(RTLD_NEXT, "dup");
	dsu_dup2 = dlsym(RTLD_NEXT, "dup2");
	dsu_dup3 = dlsym(RTLD_NEXT, "dup3");
	dsu_getsockopt = dlsym(RTLD_NEXT, "getsockopt");
	dsu_setsockopt = dlsym(RTLD_NEXT, "setsockopt");
	dsu_getsockname = dlsym(RTLD_NEXT, "getsockname");
	dsu_getpeername = dlsym(RTLD_NEXT, "getpeername");
	dsu_read = dlsym(RTLD_NEXT, "read");
	dsu_recv = dlsym(RTLD_NEXT, "recv");
	dsu_recvfrom = dlsym(RTLD_NEXT, "recvfrom");
	dsu_recvmsg = dlsym(RTLD_NEXT, "recvmsg");
	dsu_write = dlsym(RTLD_NEXT, "write");
	dsu_send = dlsym(RTLD_NEXT, "send");
	dsu_sendto = dlsym(RTLD_NEXT, "sendto");
	dsu_sendmsg = dlsym(RTLD_NEXT, "sendmsg");


	/* 	Set default function for event-handler wrapper functions. */
	dsu_select = dlsym(RTLD_NEXT, "select");
	dsu_pselect = dlsym(RTLD_NEXT, "pselect");
	dsu_poll = dlsym(RTLD_NEXT, "poll");
	dsu_ppoll = dlsym(RTLD_NEXT, "ppoll");
	dsu_epoll_wait = dlsym(RTLD_NEXT, "epoll_wait");
	dsu_epoll_pwait = dlsym(RTLD_NEXT, "epoll_pwait");
	dsu_epoll_create1 = dlsym(RTLD_NEXT, "epoll_create1");
	dsu_epoll_create = dlsym(RTLD_NEXT, "epoll_create");
	dsu_epoll_ctl = dlsym(RTLD_NEXT, "epoll_ctl");

    return;
}


/*
void dsu_sigchild_suppressor(int signum) {	
	DSU_DEBUG_PRINT("Signal suppressor() (%d)\n", (int) getpid());
	
	if (*dsu_program_state.workers == 0) {
		DSU_DEBUG_PRINT(" - Suppress() (%d)\n", (int) getpid());
		return; 
	}
	
	dsu_handler(signum);
	
}


void dsu_sigchild_suppressor(int signo, siginfo_t *siginfo, void *ucontext) {
	DSU_DEBUG_PRINT("Signal suppressor() (%d)\n", (int) getpid());

	if (*dsu_program_state.workers == 0) {
		DSU_DEBUG_PRINT(" - Suppress() (%d)\n", (int) getpid());
		return; 
	}
	
	dsu_sigaction(signo, siginfo, ucontext);

}


sighandler_t signal(int signum, sighandler_t handler) {
	DSU_DEBUG_PRINT("Signal() (%d)\n", (int) getpid());
	

	if (signum == SIGCHLD) {
		dsu_handler = handler;
		return dsu_signal(SIGCHLD, sigchild_suppressor);
	}
	

	return dsu_signal(signum, handler);

}


int sigaction(int signum, const struct sigaction *restrict act, struct sigaction *restrict oldact) {
	DSU_DEBUG_PRINT("sigaction() (%d)\n", (int) getpid());

	
	if (signum == SIGCHLD) {
		dsu_sigaction = act;
		return dsu_sigaction(SIGCHLD, dsu_sigchild_suppressor, oldact);
	}

	
	return dsu_sigaction(signum, act, oldact);
	
}
*/


int dup(int oldfd) {
	DSU_DEBUG_PRINT("Dup(%d) (%d)\n", oldfd, (int) getpid());

	struct dsu_socket_list *dsu_sockoldfd = dsu_sockets_search_fd(dsu_program_state.binds, oldfd);

	
	/* 	Socket is not binded. */
	if (dsu_sockoldfd == NULL) return dsu_dup3(oldfd);


	/* Oldfd is binded, duplicate the shadow file descriptor and add the new file descriptor to the list. */
	int fd = dsu_dup3(dsu_sockoldfd->shadowfd, newfd, flags) > 0)
	if (fd < 0) return fd;

	
	/* 	Initialize the shadow data structure. */
	struct dsu_socket_list new_dsu_sockfd;
    memcpy(&new_dsu_sockfd, dsu_sockoldfd, sizeof(struct dsu_socket_list));
	new_dsu_sockfd.shadowfd = dsu_dup(dsu_sockoldfd->shadowfd);
	new_dsu_sockfd.fd = fd;
	
	
	dsu_sockets_add(dsu_program_state.binds, &new_dsu_sockfd);
	

	return fd;
}


int dup3(int oldfd, int newfd, int flags) {
	DSU_DEBUG_PRINT("Dup3(%d, %d, flags) (%d)\n", oldfd, newfd, (int) getpid());
	/* 	The caller can force the close-on-exec flag to be set for the new file descriptor by specifying O_CLOEXEC in flags. 
		Other settings are excluded, hence not needed to overwrite flags. */

	
	struct dsu_socket_list *dsu_sockoldfd = dsu_sockets_search_fd(dsu_program_state.binds, oldfd);
	

	/* 	Both sockets are not binded. */
	if (dsu_sockoldfd == NULL) return dsu_dup3(oldfd, dsu_shadowfd(newfd), flags);
	
	
	/* 	Initialize the shadow data structure. */
	struct dsu_socket_list new_dsu_sockfd;
    memcpy(&new_dsu_sockfd, dsu_sockoldfd, sizeof(struct dsu_socket_list));
	new_dsu_sockfd.shadowfd = dsu_dup(dsu_sockoldfd->shadowfd);

		
	struct dsu_socket_list *dsu_socknewfd = dsu_sockets_search_fd(dsu_program_state.binds, newfd);
	
		
	
	if (dsu_socknewfd == NULL) {
		/* 	Only oldfd is binded, duplicate the shadow file descriptor and add the new file descriptor to the list. */

		int fd = dsu_dup3(dsu_sockoldfd->shadowfd, newfd, flags) > 0)
		if (fd < 0) return fd;

	
	} else {
		/*	Both sockets are binded, remove the old binded newfd and insert the new one. */

		int fd = dsu_dup3(dsu_sockoldfd->shadowfd, dsu_socknewfd->shadowfd, flags) > 0);
		if (fd < 0) return fd;

		
		dsu_sockets_remove_fd(dsu_program_state.binds, newfd);


	}
	
	
	new_dsu_sockfd.fd = newfd; 
	

	dsu_sockets_add(dsu_program_state.binds, &new_dsu_sockfd);


	return newfd;
}


int dup2(int oldfd, int newfd) {
	DSU_DEBUG_PRINT("Dup2(%d,%d) (%d)\n", oldfd, newfd, (int) getpid());
	return dup3(oldfd, newfd, NULL);
}


int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    DSU_DEBUG_PRINT("Bind(%d, addr, len) (%d)\n", sockfd, (int) getpid());
    /*  Bind is used to accept a client connection on an socket, this means it is a "public" socket 
        that is ready to accept requests. */
    
	
	/* 	Initialize the shadow data structure. */
	struct dsu_socket_list dsu_socketfd;
    dsu_socket_list_init(&dsu_socketfd);
    dsu_socketfd.shadowfd = dsu_socketfd.fd = sockfd;
    
    
    /*  To be able to map a socket to the correct socket in the new version the port must be known. 
        therefore we assume it is of the form sockaddr_in. This assumption must be solved and is still
        TO DO. */
    struct sockaddr_in *addr_t; addr_t = (struct sockaddr_in *) addr;
    dsu_socketfd.port = ntohs(addr_t->sin_port);
	DSU_DEBUG_PRINT(" - port %d (%d)\n", dsu_socketfd.port, (int) getpid());    

    
    /*  Possibly communicate the socket from the older version. A bind can only be performed once on the same
        socket, therefore, processes will not communicate with each other. Abstract domain socket cannot be used 
        in portable programs and has the advantage that it automatically disappear when all open references to 
        the socket are closed. */
    bzero(&dsu_socketfd.comfd_addr, sizeof(dsu_socketfd.comfd_addr));
    dsu_socketfd.comfd_addr.sun_family = AF_UNIX;
    sprintf(dsu_socketfd.comfd_addr.sun_path, "X%s_%d.unix", DSU_COMM, dsu_socketfd.port);    // On Linux, sun_path is 108 bytes in size.
    dsu_socketfd.comfd_addr.sun_path[0] = '\0';                                                // Abstract linux socket.
    dsu_socketfd.comfd = dsu_socket(AF_UNIX, SOCK_STREAM, 0);
        
    
    /*  To communicate the status between the process, shared memory is used. */
    int pathname_size = snprintf(NULL, 0, "/%s_%d", DSU_COMM, dsu_socketfd.port);
    char pathname[pathname_size+1];
    sprintf(pathname, "/%s_%d", DSU_COMM, dsu_socketfd.port);
	
	int shfd = shm_open(pathname, O_CREAT | O_RDWR , O_RDWR);
	if (shfd == -1) {
		perror("DSU \"Error creating shared memory\"");
		exit(EXIT_FAILURE);
	}

  	if (ftruncate(shfd, 3*sizeof(int)) == -1) {
	 	perror("DSU \"Error creating shared memory\"");
		exit(EXIT_FAILURE);
	}
	
	dsu_socketfd.status = (int *) mmap(NULL, 3*sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shfd, 0);
	if (dsu_socketfd.status == (void *) -1) {
		perror("DSU \"Error creating shared memory\"");
		exit(EXIT_FAILURE);
	}

	
	/*  Multiple threads/processes write to the same memory, hence, protected by lock. */
    int pathname_size_status_sem = snprintf(NULL, 0, "/%s_%d_status.semaphore", DSU_COMM, dsu_socketfd.port);
    char pathname_status_sem[pathname_size_status_sem+1];
    sprintf(pathname_status_sem, "%s_%d", DSU_COMM, dsu_socketfd.port);


	/*  When the accepting file descriptor is blocking, locks are required to avoid deadlocks in read(), write() 
		and accept() function calls. */
    int pathname_size_fd_sem = snprintf(NULL, 0, "/%s_%d_key.semaphore", DSU_COMM, dsu_socketfd.port);
    char pathname_fd_sem[pathname_size_fd_sem+1];
    sprintf(pathname_fd_sem, "%s_%d", DSU_COMM, dsu_socketfd.port);

    
    /*  Bind socket, if it fails and already exists we know that another DSU application is 
        already running. These applications need to know each others listening ports. */
    if ( dsu_monitor_init(&dsu_socketfd) == -1) {
		
        if ( errno == EADDRINUSE ) {

            if (dsu_inherit_fd(&dsu_socketfd) > 0) {
                
				dsu_socketfd.monitoring = 1; // Start listening after inheriting the file descriptor.
				
				dsu_socketfd.status_sem = sem_open(pathname_status_sem, O_CREAT, S_IRWXO | S_IRWXG | S_IRWXU, 1);
				dsu_socketfd.fd_sem = sem_open(pathname_fd_sem, O_CREAT, S_IRWXO | S_IRWXG | S_IRWXU, 1);
				
				DSU_DEBUG_PRINT(" < Lock status %d (%d)\n", dsu_socketfd.port, (int) getpid());
				if (sem_wait(dsu_socketfd.status_sem) == 0) {
					
					dsu_socketfd.version = dsu_socketfd.status[DSU_VERSION];
					
					/* 	Could be a delayed process. */
					if(dsu_socketfd.status[DSU_PGID] != getpgid(getpid())) 
						++dsu_socketfd.version;
					DSU_DEBUG_PRINT(" - version %d (%d)\n", dsu_socketfd.version, (int) getpid());
					
					DSU_DEBUG_PRINT(" > Unlock status %d (%d)\n", dsu_socketfd.port, (int) getpid());
					sem_post(dsu_socketfd.status_sem);
				
				}
				
                dsu_sockets_add(&dsu_program_state.binds, &dsu_socketfd);
                				
                return 0;
            }
        }
    }
    
	
	/*	No other version running. */
		 	
	sem_unlink(pathname_status_sem); // Semaphore does not terminate after exit.
    dsu_socketfd.status_sem = sem_open(pathname_status_sem, O_CREAT | O_EXCL, S_IRWXO | S_IRWXG | S_IRWXU, 1);
    sem_init(dsu_socketfd.status_sem, PTHREAD_PROCESS_SHARED, 1);

	
	sem_unlink(pathname_fd_sem); // Semaphore does not terminate after exit.
    dsu_socketfd.fd_sem = sem_open(pathname_fd_sem, O_CREAT | O_EXCL, S_IRWXO | S_IRWXG | S_IRWXU, 1);
    sem_init(dsu_socketfd.fd_sem, PTHREAD_PROCESS_SHARED, 1);


	dsu_socketfd.monitoring = 1;
	

	DSU_DEBUG_PRINT(" < Lock status %d (%d)\n", dsu_socketfd.port, (int) getpid());
	if (sem_wait(dsu_socketfd.status_sem) == 0) {
		
		dsu_socketfd.status[DSU_PGID] = getpgid(getpid());
		dsu_socketfd.version = dsu_socketfd.status[DSU_VERSION] = 0;
		dsu_socketfd.status[DSU_TRANSFER] = 0;
		dsu_socketfd.transfer = 0;	
		
		DSU_DEBUG_PRINT(" > Unlock status %d (%d)\n", dsu_socketfd.port, (int) getpid());
		sem_post(dsu_socketfd.status_sem);
	}
	
	
	dsu_sockets_add(&dsu_program_state.binds, &dsu_socketfd);

  	
    
    return dsu_bind(sockfd, addr, addrlen);
}


int listen(int sockfd, int backlog) {
    DSU_DEBUG_PRINT("Listen() on fd %d -> %d (%d)\n", sockfd, dsu_shadowfd(sockfd), (int) getpid());
    /*  listen() marks the socket referred to by sockfd as a passive socket, that is, as a socket that will be
        used to accept incoming connection requests using accept(). */
    
	/*	Second and subsequent calls to listen() have no other effect. */
	return dsu_listen(dsu_shadowfd(sockfd), backlog);
	
}


int accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen) {
	DSU_DEBUG_PRINT("Accept(%d, addr, len, flags) (%d)\n", sockfd, (int) getpid());   
    /*  The accept() system call is used with connection-based socket types (SOCK_STREAM, SOCK_SEQPACKET).  It extracts the first
        connection request on the queue of pending connections for the listening socket, sockfd, creates a new connected socket, and
        returns a new file descriptor referring to that socket. The DSU library need to convert the file descriptor to the shadow
        file descriptor. */
    
    int shadowfd = dsu_shadowfd(sockfd);
    
    int sessionfd = dsu_accept(shadowfd, addr, addrlen);
	if (sessionfd == -1)
		return sessionfd;

	
    DSU_DEBUG_PRINT(" - accept %d (%d)\n", sessionfd, (int) getpid());
    

	struct dsu_socket_list *dsu_sockfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
	
	if (dsu_sockfd != NULL)
		dsu_socket_add_fds(dsu_sockfd, sessionfd, DSU_NON_INTERNAL_FD);
	
    return sessionfd;    
}


int accept4(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen, int flags) {
	DSU_DEBUG_PRINT("Accept4(%d, addr, len, flags) (%d)\n", sockfd, (int) getpid());
    /*  For more information see dsu_accept(). */     
    
    int shadowfd = dsu_shadowfd(sockfd);
	
    int sessionfd = dsu_accept4(shadowfd, addr, addrlen, flags);
	if (sessionfd == -1)
		return sessionfd;
    
    DSU_DEBUG_PRINT(" - accept %d (%d)\n", sessionfd, (int) getpid());
    
	struct dsu_socket_list *dsu_sockfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);


	if (dsu_sockfd != NULL)
		dsu_socket_add_fds(dsu_sockfd, sessionfd, DSU_NON_INTERNAL_FD);
    
    return sessionfd;   
}





int close(int sockfd) {
	DSU_DEBUG_PRINT("Close() fd: %d (%d)\n", sockfd, (int) getpid());
	/*	close() closes a file descriptor, so that it no longer refers to any file and may be reused. Therefore, the the shadow file descriptor
		should also be removed / closed. The file descriptor can exist in:
			2.	Binded sockets 		-> 	dsu_program_State.binds
			3.	Internal sockets	-> 	dsu_program_State.binds->comfd | dsu_program_State.binds->fds
			4.	Connected clients	-> 	dsu_program_state.accepted	*/
	
		
	/* 	Return immediately for these file descriptors. */
    if (sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO) {
        return dsu_close(sockfd);
    }


	struct dsu_socket_list *dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
	if (dsu_socketfd != NULL) {
		DSU_DEBUG_PRINT(" - Binded socket %d=%d (%d)\n",dsu_socketfd->fd, dsu_socketfd->shadowfd, (int) getpid());
		dsu_sockets_remove_fd(&dsu_program_state.binds, sockfd);
		return dsu_close(sockfd);
	}
	
	
	dsu_socketfd = dsu_sockets_search_fds(dsu_program_state.binds, sockfd, DSU_MONITOR_FD);
	if (dsu_socketfd != NULL) {
		DSU_DEBUG_PRINT(" - Internal master socket (%d)\n", (int) getpid());
		dsu_socketfd->monitoring = 0;
		return dsu_close(sockfd);
	}

	
	dsu_socketfd = dsu_sockets_search_fds(dsu_program_state.binds, sockfd, DSU_INTERNAL_FD);
	if (dsu_socketfd != NULL) {
		DSU_DEBUG_PRINT(" - Internal client socket (%d)\n", (int) getpid());
		dsu_socket_remove_fds(dsu_socketfd, sockfd, DSU_INTERNAL_FD);
		return dsu_close(sockfd);
	}


	dsu_socketfd = dsu_sockets_search_fds(dsu_program_state.binds, sockfd, DSU_NON_INTERNAL_FD);
	if (dsu_socketfd != NULL) {
		DSU_DEBUG_PRINT(" - Client socket (%d)\n", (int) getpid());
		dsu_socket_remove_fds(dsu_socketfd, sockfd, DSU_NON_INTERNAL_FD);
		return dsu_close(sockfd);
	}


	return dsu_close(sockfd);

}

/* The prototype stands out in the list of Unix system calls because of the dots, which usually mark the function as having a variable number of arguments. In a real system, however, a system call can't actually have a variable number of arguments. System calls must have a well-defined prototype, because user programs can access them only through hardware "gates." Therefore, the dots in the prototype represent not a variable number of arguments but a single optional argument, traditionally identified as char *argp. The dots are simply there to prevent type checking during compilation. */
int fcntl(int fd, int cmd, char *argp) {
	DSU_DEBUG_PRINT("fcntl(%d, cmd, argp) (%d)\n", fd, (int) getpid());

	
	if (cmd == F_DUPFD) {
		
		struct dsu_socket_list *dsu_sockoldfd = dsu_sockets_search_fd(dsu_program_state.binds, fd);


		/* 	Socket is not binded. */
		if (dsu_sockoldfd == NULL) return dsu_dup3(oldfd);


		/* Oldfd is binded, duplicate the shadow file descriptor and add the new file descriptor to the list. */
		int newfd = dsu_fcntl(dsu_sockoldfd->shadowfd, cmd, argp);
		if (newfd < 0) return fd;
		

		/* 	Initialize the shadow data structure. */
		struct dsu_socket_list new_dsu_sockfd;
		memcpy(&new_dsu_sockfd, dsu_sockoldfd, sizeof(struct dsu_socket_list));
		new_dsu_sockfd.shadowfd = dsu_dup(dsu_sockoldfd->shadowfd);
		new_dsu_sockfd.fd = newfd;


		dsu_sockets_add(dsu_program_state.binds, &new_dsu_sockfd);

	
		return newfd;
	
	} else if (cmd == F_SETFD) {

		struct dsu_socket_list *dsu_sockoldfd = dsu_sockets_search_fd(dsu_program_state.binds, fd);

	
		/* 	Socket is not binded. */
		if (dsu_sockoldfd == NULL) return dsu_fcntl(fd, cmd, argp);	


		if // TO DO!
	


	}

		


	return dsu_fcntl(dsu_sockoldfd->shadowfd, cmd, argp);
}
