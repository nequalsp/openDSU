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
#include <sys/mman.h>


#include "core.h"
#include "state.h"
#include "communication.h"


#include "event_handlers/select.h"


/* 	Global variable containing pointers to the used data structures and state of the program. Every binded file descriptor
	will be connected to a shadow data structure including file descriptor for communication between versions, state
	management and shadow file descriptor if it is inherited from previous version. */
struct dsu_state_struct dsu_program_state;


/* 	For the functions socket(), bind(), listen(), accept(), accept4() and close() a wrapper is 
	created to maintain shadow data structures of the file descriptors. */
int (*dsu_socket)(int, int, int);
int (*dsu_bind)(int, const struct sockaddr *, socklen_t);
int (*dsu_listen)(int, int);
int (*dsu_accept)(int, struct sockaddr *restrict, socklen_t *restrict);
int (*dsu_accept4)(int, struct sockaddr *restrict, socklen_t *restrict, int);
int (*dsu_close)(int);


int dsu_inherit_fd(struct dsu_socket_list *dsu_sockfd) {
	DSU_DEBUG_PRINT(" - Inherit fd %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
	/*	Connect to the previous version, based on named unix domain socket, and receive the file descriptor that is 
		binded to the same port. Also, receive the file descriptor of the named unix domain socket so internal
		communication can be taken over when the update completes. */


	DSU_DEBUG_PRINT("  - Connect on %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
    if ( connect(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, sizeof(dsu_sockfd->comfd_addr)) != -1) {
        
		
        DSU_DEBUG_PRINT("  - Send on %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
        if ( send(dsu_sockfd->comfd, &dsu_sockfd->port, sizeof(dsu_sockfd->port), 0) > 0) {


            DSU_DEBUG_PRINT("  - Recieve on %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
            int port = 0; int _comfd = 0;
            dsu_read_fd(dsu_sockfd->comfd, &dsu_sockfd->shadowfd, &port);
			dsu_read_fd(dsu_sockfd->comfd, &_comfd, &port);
			

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
		3.	The singlely linked list comfds still contains open connections between different versions.
		4.	Existes of open connections with one or more clients. */
	
	DSU_DEBUG_PRINT(" - Termination detection (%d-%d)\n", (int) getpid(), (int) gettid());
	
	struct dsu_socket_list *current = dsu_program_state.binds;

	while (current != NULL) {

		if (	current->version >= current->status[DSU_VERSION]
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
	DSU_DEBUG_PRINT(" - Termination (%d-%d)\n", (int) getpid(), (int) gettid());
	/*	Different models, such as master-worker model, are used to horizontally scale the application. This
		can either be done with threads or processes. As threads are implemented as processes on linux, 
		there is not difference in termination. The number of active workers is tracked in the event handler. 
		The last active worker that terminates, terminates the group to ensure the full application stops. */
	

	int final = 0; 	//	Ensure program lock is released first, because it is shared with the new version. 
	
	
	DSU_DEBUG_PRINT(" - Lock program state (%d-%d)\n", (int) getpid(), (int) gettid());
	if (sem_wait(dsu_program_state.lock) == -1)
		perror("program_state.lock");
	
	if (--dsu_program_state.workers[0] == 0) {
		DSU_DEBUG_PRINT("  - All (%d-%d)\n", (int) getpid(), (int) gettid());
		final = 1;
	}
	
	DSU_DEBUG_PRINT(" - Unlock program state (%d-%d)\n", (int) getpid(), (int) gettid());
	sem_post(dsu_program_state.lock);


	if (final) killpg(getpgid(getpid()), SIGKILL);
	
	
    exit(EXIT_SUCCESS);     

}    
			

int dsu_monitor_init(struct dsu_socket_list *dsu_sockfd) {
	DSU_DEBUG_PRINT(" - (Try) initialize communication on  %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
	/*  This function is called when the program calls bind(). If bind fails, in normal situation, a older version exists. If 
		bind succees, start listening on the named unix domain file descriptor for connection request of newer versions. It might 
		happen fork or pthreads are used to accept connections on multiple processes and multi threads respectively. Therefore set 
		the socket to non-blocking to be able to accept connection requests without risk of indefinetely blocking. */
    
    if (dsu_bind(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, (socklen_t) sizeof(dsu_sockfd->comfd_addr)) == 0) {
        
        if (dsu_listen(dsu_sockfd->comfd, MAXNUMOFPROC) == 0) {
            
			fcntl(dsu_sockfd->comfd, F_SETFL, fcntl(dsu_sockfd->comfd, F_GETFL, 0) | O_NONBLOCK); // Set non-blocking, several might be accepting connections.
			
            DSU_DEBUG_PRINT(" - Initialized communication on  %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());

			return 0;
        }
    
    }
	

	return -1;     

}


void dsu_monitor_fd(struct dsu_socket_list *dsu_sockfd) {
	
	DSU_DEBUG_PRINT(" - Monitor on %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());

	
	DSU_DEBUG_PRINT("  - Lock status %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
	sem_wait(dsu_sockfd->status_sem);

			
	/*	Quit monitoring by older generation. */
	if (	dsu_sockfd->version < dsu_sockfd->status[DSU_VERSION] 
		&&	dsu_sockfd->monitoring 
	) {
		
		DSU_DEBUG_PRINT("  - Quit monitoring  %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
		dsu_close(dsu_sockfd->comfd);
		dsu_sockfd->monitoring = 0;
		dsu_sockfd->status[DSU_TRANSFER] = 0;
	
	}


	/*	Takeover monitoring from older generation. */
	if (dsu_sockfd->version > dsu_sockfd->status[DSU_VERSION]) {
		
		DSU_DEBUG_PRINT("  - Increase version  %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());				
		dsu_sockfd->status[DSU_PGID] = getpgid(getpid());
		++dsu_sockfd->status[DSU_VERSION];
		
	}
	
		
	DSU_DEBUG_PRINT("  - Unlock status %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
	sem_post(dsu_sockfd->status_sem);
	
}


static __attribute__((constructor)) void dsu_init() {

	
	#if DSU_DEBUG == 1
    int size = snprintf(NULL, 0, "%s_%d.log", DSU_LOG, (int) getpid());
	char logfile[size+1];	
	sprintf(logfile, "%s_%d.log", DSU_LOG, (int) getpid());
	dsu_program_state.logfd = fopen(logfile, "w");
	if (dsu_program_state.logfd == NULL) {
		perror("DSU \" Error opening debugging file\":");
		exit(EXIT_FAILURE);
	}
	#endif
	DSU_DEBUG_PRINT("INIT() (%d-%d)\n", (int) getpid(), (int) gettid());


    dsu_program_state.sockets = NULL;
    dsu_program_state.binds = NULL;

	
	dsu_program_state.live = 0;
	
	
	/*  */
    int pathname_size = snprintf(NULL, 0, "/%s_%d.state", DSU_COMM, (int) getpgid(getpid()));
    char pathname[pathname_size+1];
    sprintf(pathname, "/%s_%d.state", DSU_COMM, (int) getpgid(getpid()));
	shm_unlink(pathname);
	int shfd = shm_open(pathname, O_CREAT | O_RDWR | O_TRUNC, O_RDWR);
	if (shfd == -1)
		perror("DSU");
  	if (ftruncate(shfd, sizeof(int)) == -1)
	 	perror("DSU");
	dsu_program_state.workers = (int *) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shfd, 0);
	if (dsu_program_state.workers == (void *) -1)
		perror("DSU");
	dsu_program_state.workers[0] = 0;	// Initialize.


	/*   */
    int pathname_size_sem = snprintf(NULL, 0, "/%s_%d.lock", DSU_COMM, (int) getpgid(getpid()));
    char pathname_sem[pathname_size_sem+1];
    sprintf(pathname_sem, "/%s_%d.lock", DSU_COMM, (int) getpgid(getpid()));
	sem_unlink(pathname_sem); // Semaphore does not terminate after exit.
    dsu_program_state.lock = sem_open(pathname_sem, O_CREAT | O_EXCL, S_IRWXO | S_IRWXG | S_IRWXU, 1);
    sem_init(dsu_program_state.lock, PTHREAD_PROCESS_SHARED, 1);
	
	
	/*  Wrappers around system function. RTLD_NEXT will find the next occurrence of a function in the search 
		order after the current library*/
	dsu_socket = dlsym(RTLD_NEXT, "socket");
	dsu_bind = dlsym(RTLD_NEXT, "bind");
	dsu_listen = dlsym(RTLD_NEXT, "listen");
	dsu_accept = dlsym(RTLD_NEXT, "accept");
	dsu_accept4 = dlsym(RTLD_NEXT, "accept4");
	dsu_close = dlsym(RTLD_NEXT, "close");

	
	/* 	Set default function for event-handler wrapper functions. */
	dsu_select = dlsym(RTLD_NEXT, "select");

	
    return;
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
        dsu_socket.shadowfd = dsu_socket.fd = sockfd;
        dsu_sockets_add(&dsu_program_state.sockets, &dsu_socket);
    }
	
    
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
    
    
    /*  Possibly communicate the socket from the older version. A bind can only be performed once on the same
        socket, therefore, processes will not communicate with each other. Abstract domain socket cannot be used 
        in portable programs. But has the advantage that it automatically disappear when all open references to 
        the socket are closed. */
    bzero(&dsu_socketfd->comfd_addr, sizeof(dsu_socketfd->comfd_addr));
    dsu_socketfd->comfd_addr.sun_family = AF_UNIX;
    sprintf(dsu_socketfd->comfd_addr.sun_path, "X%s_%d.unix", DSU_COMM, dsu_socketfd->port);    // On Linux, sun_path is 108 bytes in size.
    dsu_socketfd->comfd_addr.sun_path[0] = '\0';                                                // Abstract linux socket.
    dsu_socketfd->comfd = dsu_socket(AF_UNIX, SOCK_STREAM, 0);
    DSU_DEBUG_PRINT(" - Bind port %d on %d (%d-%d)\n", dsu_socketfd->port, sockfd, (int) getpid(), (int) gettid());
    
    
    /*  To communicate the status between the process, shared memory is used. */
    int pathname_size = snprintf(NULL, 0, "/%s_%d", DSU_COMM, dsu_socketfd->port);
    char pathname[pathname_size+1];
    sprintf(pathname, "/%s_%d", DSU_COMM, dsu_socketfd->port);
	int shfd = shm_open(pathname, O_CREAT | O_RDWR , O_RDWR);
	if (shfd == -1)			// TO DO cleanup here.
		perror("DSU");
  	if (ftruncate(shfd, 3*sizeof(int)) == -1)
	 	perror("DSU");
	dsu_socketfd->status = (int *) mmap(NULL, 3*sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shfd, 0);
	if (dsu_socketfd->status == (void *) -1)
		perror("DSU");

	
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

            if (dsu_inherit_fd(dsu_socketfd) > 0) {
                
				dsu_socketfd->monitoring = 1; // Communication file descriptor is inherited.
				
				dsu_socketfd->status_sem = sem_open(pathname_status_sem, O_CREAT, S_IRWXO | S_IRWXG | S_IRWXU, 1);
				dsu_socketfd->fd_sem = sem_open(pathname_fd_sem, O_CREAT, S_IRWXO | S_IRWXG | S_IRWXU, 1);
				
				DSU_DEBUG_PRINT(" - Lock status %d (%d-%d)\n", dsu_socketfd->port, (int) getpid(), (int) gettid());
				if (sem_wait(dsu_socketfd->status_sem) == 0) {
					dsu_socketfd->version = dsu_socketfd->status[DSU_VERSION];
					if(dsu_socketfd->status[DSU_PGID] != getpgid(getpid())) // Could be a delayed process.
						++dsu_socketfd->version;
					DSU_DEBUG_PRINT(" - version %d (%d-%d)\n", dsu_socketfd->version, (int) getpid(), (int) gettid());
					DSU_DEBUG_PRINT(" - Unlock status %d (%d-%d)\n", dsu_socketfd->port, (int) getpid(), (int) gettid());
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
	

	DSU_DEBUG_PRINT(" - Lock status %d (%d-%d)\n", dsu_socketfd->port, (int) getpid(), (int) gettid());
	if (sem_wait(dsu_socketfd->status_sem) == 0) {
		dsu_socketfd->status[DSU_PGID] = getpgid(getpid());
		dsu_socketfd->version = dsu_socketfd->status[DSU_VERSION] = 0;
		dsu_socketfd->status[DSU_TRANSFER] = 0;
		DSU_DEBUG_PRINT(" - Unlock status %d (%d-%d)\n", dsu_socketfd->port, (int) getpid(), (int) gettid());
		sem_post(dsu_socketfd->status_sem);
	}
	
    dsu_sockets_transfer_fd(&dsu_program_state.binds, &dsu_program_state.sockets, dsu_socketfd);
  	
    
    return dsu_bind(sockfd, addr, addrlen);
}


int listen(int sockfd, int backlog) {
    DSU_DEBUG_PRINT("Listen() on fd %d (%d-%d)\n", sockfd, (int) getpid(), (int) gettid());
    /*  listen() marks the socket referred to by sockfd as a passive socket, that is, as a socket that will be
        used to accept incoming connection requests using accept(). */
    
    struct dsu_socket_list *dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
    
    /*  The file descriptor is not inherited. */
    if (dsu_socketfd == NULL || dsu_socketfd->monitoring == 1) {
        return dsu_listen(sockfd, sockfd);
    }
    
    /*  The socket is recieved from the older generation and listen() does not need to be called. */
	DSU_DEBUG_PRINT(" - Shadow fd: %d (%d-%d)\n", dsu_socketfd->shadowfd, (int) getpid(), (int) gettid());
    return 0;
}


int accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen) {
	DSU_DEBUG_PRINT("Accept() (%d-%d)\n", (int) getpid(), (int) gettid());   
    /*  The accept() system call is used with connection-based socket types (SOCK_STREAM, SOCK_SEQPACKET).  It extracts the first
        connection request on the queue of pending connections for the listening socket, sockfd, creates a new connected socket, and
        returns a new file descriptor referring to that socket. The DSU library need to convert the file descriptor to the shadow
        file descriptor. */
    
    int shadowfd = dsu_shadowfd(sockfd);
    
    int sessionfd = dsu_accept(shadowfd, addr, addrlen);
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
    
    int shadowfd = dsu_shadowfd(sockfd);
	
    int sessionfd = dsu_accept4(shadowfd, addr, addrlen, flags);
	if (sessionfd == -1)
		return sessionfd;
    
    DSU_DEBUG_PRINT(" - accept %d (%d-%d)\n", sessionfd, (int) getpid(), (int) gettid());
    
	struct dsu_socket_list *dsu_sockfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
	
	if (dsu_sockfd != NULL)
		dsu_socket_add_fds(dsu_sockfd, sessionfd, DSU_NON_INTERNAL_FD);
    
    return sessionfd;   
}


int close(int sockfd) {
	DSU_DEBUG_PRINT("Close() (%d-%d)\n", (int) getpid(), (int) gettid());
	
	
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
		DSU_DEBUG_PRINT(" - Binded socket (%d-%d)\n", (int) getpid(), (int) gettid());
		dsu_sockets_remove_fd(&dsu_program_state.binds, sockfd);
		return dsu_close(sockfd);
	}
	
	
	dsu_socketfd = dsu_sockets_search_fds(dsu_program_state.binds, sockfd, DSU_MONITOR_FD);
	if (dsu_socketfd != NULL) {
		DSU_DEBUG_PRINT(" - Internal master socket (%d-%d)\n", (int) getpid(), (int) gettid());
		dsu_socketfd->monitoring = 0;
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

