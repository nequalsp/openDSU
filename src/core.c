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
#include <fcntl.h>


#include "core.h"
#include "state.h"
#include "communication.h"
#include "log.h"


#include "event_handlers/select.h"
//#include "event_handlers/poll.h"
#include "event_handlers/epoll.h"


/* 	Global variable containing pointers to the used data structures and state of the program. Every binded file descriptor
	will be connected to a shadow data structure including file descriptor for communication between versions, state
	management and shadow file descriptor if it is inherited from previous version. */
struct dsu_state_struct dsu_program_state;


/* 	For the functions socket(), bind(), listen(), accept(), accept4() and close() a wrapper is 
	created to maintain shadow data structures of the file descriptors. */
int (*dsu_bind)(int, const struct sockaddr *, socklen_t);
int (*dsu_close)(int);



int dsu_recieve_fd(struct dsu_socket_list *dsu_sockfd) {
	DSU_DEBUG_PRINT(" - Inherit fd %d (%d)\n", dsu_sockfd->fd, (int) getpid());
	/*	Connect to the previous version, based on named unix domain socket, and receive the file descriptor that is 
		binded to the same port. Also, receive the file descriptor of the named unix domain socket so internal
		communication can be taken over when the update completes. */


	DSU_DEBUG_PRINT("  - Connect on %d (%d)\n", dsu_sockfd->comfd, (int) getpid());
    if ( connect(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, sizeof(dsu_sockfd->comfd_addr)) == 0) {
        
		
        DSU_DEBUG_PRINT("  - Read on %d (%d)\n", dsu_sockfd->comfd, (int) getpid());
		int _sock = 0; int _comfd = 0;
        if (dsu_read_fd(dsu_sockfd->comfd, &_sock) < 0) goto error;				// Listening socket.
		if (dsu_read_fd(dsu_sockfd->comfd, &_comfd) < 0) goto error;			// Internal socket.
		
		
    	DSU_DEBUG_PRINT("  - Received %d & %d (%d)\n", _sock, _comfd, (int) getpid());
		close(dsu_sockfd->comfd);

		
	    dsu_sockfd->comfd = _comfd;
		return dup2(_sock, dsu_sockfd->fd);
		

    }

	error:
		close(dsu_sockfd->comfd);

	return -1;
}


void dsu_send_fd(struct dsu_socket_list *dsu_sockfd) {
	DSU_DEBUG_PRINT("  - Accept internal connection (%d)\n", (int) getpid());

	
	int size = sizeof(dsu_sockfd->comfd_addr);
	DSU_DEBUG_PRINT("  - Accept on %d (%d)\n", dsu_sockfd->comfd, (int) getpid());
	int comfd = accept4(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, (socklen_t *) &size, 0);	
	if ( comfd != -1) {
		
		
		++dsu_sockfd->status[DSU_TRANSFER];


		DSU_DEBUG_PRINT("  - Write on %d (%d)\n", dsu_sockfd->comfd, (int) getpid());
    	if(dsu_write_fd(comfd, dsu_sockfd->fd) <= 0) goto end;		// Listening socket.
		if(dsu_write_fd(comfd, dsu_sockfd->comfd) <= 0) goto end;	// Internal socket.
		recv(comfd, NULL, 0, 0);									// Wait for the close.

		DSU_DEBUG_PRINT("  - Close (%d)\n", (int) getpid());
		dsu_close(comfd);

	}
	
	end:
		dsu_close(comfd);
}


void dsu_termination() {
	DSU_DEBUG_PRINT(" - Termination (%d)\n", (int) getpid());
	DSU_ALERT_PRINT(" - Termination (%d)\n", (int) getpid());
	/*	Different models, such as master-worker model, are used to horizontally scale the application. This
		can either be done with threads or processes. As threads are implemented as processes on linux, 
		there is not difference in termination. The number of active workers is tracked in the event handler. 
		The last active worker that terminates, terminates the group to ensure the full application stops. */
    
	
	DSU_DEBUG_PRINT(" < Lock program state (%d)\n", (int) getpid());
	if (sem_wait(dsu_program_state.lock) == 0) {
		
		
		if (dsu_program_state.workers[0] == 0) {
			DSU_DEBUG_PRINT("  - All (%d)\n", (int) getpid());
			DSU_ALERT_PRINT("  - All (%d)\n", (int) getpid());

			
			DSU_DEBUG_PRINT(" > Unlock program state (%d)\n", (int) getpid());
			sem_post(dsu_program_state.lock);

			
			killpg(getpgid(getpid()), SIGKILL);
		}	
		

		DSU_DEBUG_PRINT(" > Unlock program state (%d)\n", (int) getpid());
		sem_post(dsu_program_state.lock);
	}

	
	return;

}


void dsu_activate_process(void) {
		
	DSU_DEBUG_PRINT(" < Lock program state (%d)\n", (int) getpid());
	if( sem_wait(dsu_program_state.lock) == 0) {
		
		++dsu_program_state.workers[0];

		DSU_DEBUG_PRINT(" > Unlock program state (%d)\n", (int) getpid());
		sem_post(dsu_program_state.lock);
	}
	
}


void dsu_deactivate_process(void) {
		
	DSU_DEBUG_PRINT(" < Lock program state (%d)\n", (int) getpid());
	if( sem_wait(dsu_program_state.lock) == 0) {
		
		--dsu_program_state.workers[0];

		DSU_DEBUG_PRINT(" > Unlock program state (%d)\n", (int) getpid());
		sem_post(dsu_program_state.lock);
	}
	
}


void dsu_handle_state(struct dsu_socket_list *dsu_sockfd) {
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
			
			DSU_DEBUG_PRINT("  - Quit monitoring %d (%d)\n", dsu_sockfd->port, (int) getpid());
			DSU_ALERT_PRINT("  - Quit monitoring %d (%d)\n", dsu_sockfd->port, (int) getpid());
			dsu_close(dsu_sockfd->comfd);
			dsu_sockfd->monitoring = 0;
			--dsu_sockfd->status[DSU_TRANSFER];
			dsu_sockfd->transfer = 0;
		
		}


		if (	dsu_sockfd->version > dsu_sockfd->status[DSU_VERSION]
			&&	dsu_sockfd->monitoring) {
			
			DSU_DEBUG_PRINT("  - Increase version %d (%d)\n", dsu_sockfd->port, (int) getpid());
			DSU_ALERT_PRINT("  - Increase version %d (%d)\n", dsu_sockfd->port, (int) getpid());				
			dsu_sockfd->status[DSU_PGID] = getpgid(getpid());
			++dsu_sockfd->status[DSU_VERSION];
			
		}
	
		
		DSU_DEBUG_PRINT("  > Unlock status %d (%d)\n", dsu_sockfd->port, (int) getpid());
		sem_post(dsu_sockfd->status_sem);
	
	}
}


void dsu_pre_event(void) {
	
	/* 	On first call event handler call, mark worker active and configure binded sockets. */
	if (!dsu_program_state.live) {	
		
		dsu_activate_process();
		
		/*	Process is initialized. */
		dsu_program_state.live = 1;
	}
	
	
	/* 	State handler binded sockets. */
	dsu_forall_sockets(dsu_program_state.binds, dsu_handle_state);
	
}


static __attribute__((constructor)) void dsu_init() {
	/*	LD_Preload constructor is called before the binary starts. Initialze the program state. */
	

	#if defined(DEBUG) || defined(ALERT)
	#if defined(DEBUG)
		int size = snprintf(NULL, 0, "%s/dsu_%d.log", DEBUG, (int) getpid());
		char logfile[size+1];
		sprintf(logfile, "%s/dsu_%d.log", DEBUG, (int) getpid());
	#endif
	#if defined(ALERT)
		int size = snprintf(NULL, 0, "%s/dsu_%d.log", ALERT, (int) getpid());
		char logfile[size+1];
		sprintf(logfile, "%s/dsu_%d.log", ALERT, (int) getpid());
	#endif
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


	/*	Create semaphore in shared memory to avoid race conditions between multiple processes during modifications. */
    int pathname_size_sem = snprintf(NULL, 0, "/%s_%d.lock", DSU_COMM, (int) getpgid(getpid()));
    char pathname_sem[pathname_size_sem+1];
    sprintf(pathname_sem, "/%s_%d.lock", DSU_COMM, (int) getpgid(getpid()));
	sem_unlink(pathname_sem);
	dsu_program_state.lock = sem_open(pathname_sem, O_CREAT | O_EXCL, S_IRWXO | S_IRWXG | S_IRWXU, 1);
	if (dsu_program_state.lock < 0) {
		perror("DSU \"Error creating program state lock memory\"");
		exit(EXIT_FAILURE);
	}
	sem_init(dsu_program_state.lock, PTHREAD_PROCESS_SHARED, 1);
	
	
	/*  Wrappers around system function. RTLD_NEXT will find the next occurrence of a function in the search 
		order after the current library*/
	dsu_bind = dlsym(RTLD_NEXT, "bind");
	dsu_close = dlsym(RTLD_NEXT, "close");
	

	/* 	Set default function for event-handler wrapper functions. */
	dsu_select = dlsym(RTLD_NEXT, "select");
	dsu_pselect = dlsym(RTLD_NEXT, "pselect");
	//dsu_poll = dlsym(RTLD_NEXT, "poll");
	//dsu_ppoll = dlsym(RTLD_NEXT, "ppoll");
	dsu_epoll_wait = dlsym(RTLD_NEXT, "epoll_wait");
	dsu_epoll_pwait = dlsym(RTLD_NEXT, "epoll_pwait");
	dsu_epoll_pwait2 = dlsym(RTLD_NEXT, "epoll_pwait2");
	dsu_epoll_ctl = dlsym(RTLD_NEXT, "epoll_ctl");


    return;
}


void dsu_init_sem_status(struct dsu_socket_list *dsu_socketfd, int first) {


	int pathname_size_status_sem = snprintf(NULL, 0, "/%s_%d_status.semaphore", DSU_COMM, dsu_socketfd->port);
    char pathname_status_sem[pathname_size_status_sem+1];
    sprintf(pathname_status_sem, "%s_%d", DSU_COMM, dsu_socketfd->port);
	if (first) sem_unlink(pathname_status_sem); // Semaphore does not terminate after exit.
    if (first) {
		sem_unlink(pathname_status_sem);
		dsu_socketfd->status_sem = sem_open(pathname_status_sem, O_CREAT | O_EXCL, S_IRWXO | S_IRWXG | S_IRWXU, 0);
	} else {
		dsu_socketfd->status_sem = sem_open(pathname_status_sem, 0);
	}

	if (dsu_socketfd->status_sem == SEM_FAILED) {
		perror("DSU \"Error initializing semaphore for status\"");
		exit(EXIT_FAILURE);
	}

}


void dsu_init_sem_fd(struct dsu_socket_list *dsu_socketfd, int first) {

	
	int pathname_size_fd_sem = snprintf(NULL, 0, "/%s_%d_key.semaphore", DSU_COMM, dsu_socketfd->port);
    char pathname_fd_sem[pathname_size_fd_sem+1];
    sprintf(pathname_fd_sem, "%s_%d", DSU_COMM, dsu_socketfd->port);
	if (first) {
		sem_unlink(pathname_fd_sem); // Semaphore does not terminate after exit.
    	dsu_socketfd->fd_sem = sem_open(pathname_fd_sem, O_CREAT | O_EXCL, S_IRWXO | S_IRWXG | S_IRWXU, 0);
	} else {
		dsu_socketfd->fd_sem = sem_open(pathname_fd_sem, 0);
	}
		

	if (dsu_socketfd->fd_sem == SEM_FAILED) {
		perror("DSU \"Error initializing semaphore for file descriptor\"");
		exit(EXIT_FAILURE);
	}

}

void dsu_init_fd(struct dsu_socket_list *dsu_socketfd, const struct sockaddr *addr) {


	/*	Set default values. */
	dsu_socket_list_init(dsu_socketfd);


	/*  To be able to map a socket to the correct socket in the new version the port must be known. 
        therefore we assume it is of the form sockaddr_in. This assumption must be solved and is still
        TO DO. */
    struct sockaddr_in *addr_t; addr_t = (struct sockaddr_in *) addr;
    dsu_socketfd->port = ntohs(addr_t->sin_port);
	DSU_DEBUG_PRINT(" - port %d (%d)\n", dsu_socketfd->port, (int) getpid());


	/*  Possibly communicate the socket from the older version. A bind can only be performed once on the same
        socket, therefore, processes will not communicate with each other. Abstract domain socket cannot be used 
        in portable programs and has the advantage that it automatically disappear when all open references to 
        the socket are closed. */
    dsu_socketfd->comfd_addr.sun_family = AF_UNIX;
    sprintf(dsu_socketfd->comfd_addr.sun_path, "X%s_%d.unix", DSU_COMM, dsu_socketfd->port);    	// On Linux, sun_path is 108 bytes in size.
    dsu_socketfd->comfd_addr.sun_path[0] = '\0';                                                	// Abstract linux socket.
    dsu_socketfd->comfd = socket(AF_UNIX, SOCK_STREAM, 0);
		//if (fchmod(dsu_socketfd.comfd, S_IROTH | S_IWOTH) < 0)
	//	perror("mod");			// protect


	dsu_socketfd->lock = open("/tmp/dsu_lock", O_RDONLY | O_CREAT, 0600);


	/*  To communicate the status between the process, shared memory is used. */
    int pathname_size = snprintf(NULL, 0, "/%s_%d", DSU_COMM, dsu_socketfd->port);
    char pathname[pathname_size+1];
    sprintf(pathname, "/%s_%d", DSU_COMM, dsu_socketfd->port);
	int shfd = shm_open(pathname, O_CREAT | O_RDWR , O_RDWR);
	if (shfd == -1) {
		perror("DSU \"Error creating shared memory\"");
		exit(EXIT_FAILURE);
	}
  	if (ftruncate(shfd, 3*sizeof(int)) == -1) {
	 	perror("DSU \"Error creating shared memory\"");
		exit(EXIT_FAILURE);
	}
	dsu_socketfd->status = (int *) mmap(NULL, 3*sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shfd, 0);
	
	
	if (dsu_socketfd->status == (void *) -1) {
		perror("DSU \"Error creating shared memory\"");
		exit(EXIT_FAILURE);
	}


	dsu_socketfd->status[DSU_PGID] = 0;
	dsu_socketfd->status[DSU_VERSION] = 0;
	dsu_socketfd->status[DSU_TRANSFER] = 0;

}


int dsu_version_fd(struct dsu_socket_list *dsu_sockfd) {
	
	DSU_DEBUG_PRINT(" - (Try) initialize communication on  %d (%d)\n", dsu_sockfd->port, (int) getpid());
	/*  This function is called when the program calls bind(). If bind fails, in normal situation, a older version exists. If 
		bind succees it is the first version while on the other hand it is a new version. */
    
    if ( dsu_bind(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, (socklen_t) sizeof(dsu_sockfd->comfd_addr)) == -1)   
		if ( errno == EADDRINUSE )
			return -1;

	return 0;

}


void dsu_init_first_version(struct dsu_socket_list *dsu_socketfd) {
	   

	/*	It might happen fork or pthreads are used to accept connections on multiple processes and multi threads respectively. Therefore set 
		the socket to non-blocking to be able to accept connection requests without risk of indefinetely blocking. */
	int flags = fcntl(dsu_socketfd->comfd, F_GETFL, 0) | O_NONBLOCK;
	fcntl(dsu_socketfd->comfd, F_SETFL, (char *) &flags);
	if (listen(dsu_socketfd->comfd, DSU_MAXNUMOFPROC) == -1) {
		perror("DSU \"Error listen on communication fd\"");
		exit(EXIT_FAILURE);
	}
           
			
    dsu_init_sem_status(dsu_socketfd, 1);


    dsu_init_sem_fd(dsu_socketfd, 1);


	DSU_ALERT_PRINT(" - Group process id %d (%d)\n", getpgid(getpid()), (int) getpid());
	dsu_socketfd->status[DSU_PGID] = getpgid(getpid());
	dsu_socketfd->version = dsu_socketfd->status[DSU_VERSION] = 0;
	dsu_socketfd->status[DSU_TRANSFER] = 0;
	dsu_socketfd->transfer = 0;

	
	dsu_socketfd->monitoring = 1;
	dsu_sockets_add(&dsu_program_state.binds, dsu_socketfd);


	/* 	Activate semaphore. */
	if (sem_init(dsu_socketfd->fd_sem, PTHREAD_PROCESS_SHARED, 1) == -1) {
		perror("DSU \"Error initializing semaphore\"");
		exit(EXIT_FAILURE);
	}

	
	/* 	Activate semaphore, allow new processes to continue. */
    if (sem_init(dsu_socketfd->status_sem, PTHREAD_PROCESS_SHARED, 1) == -1) {
		perror("DSU \"Error initializing semaphore\"");
		exit(EXIT_FAILURE);
	}

}


void dsu_init_new_version(struct dsu_socket_list *dsu_socketfd) {

	dsu_socketfd->monitoring = 1; // Start listening after inheriting the file descriptor.
			
	
	dsu_init_sem_status(dsu_socketfd, 0);


    dsu_init_sem_fd(dsu_socketfd, 0);


	DSU_DEBUG_PRINT(" < Lock status %d (%d)\n", dsu_socketfd->port, (int) getpid());
	if (sem_wait(dsu_socketfd->status_sem) == 0) {
		
		dsu_socketfd->version = dsu_socketfd->status[DSU_VERSION];
		
		/* 	Could be a delayed process. */
		DSU_ALERT_PRINT(" - Group process id %d (%d)\n", getpgid(getpid()), (int) getpid());
		if(dsu_socketfd->status[DSU_PGID] != getpgid(getpid())) 
			++dsu_socketfd->version;
		DSU_DEBUG_PRINT(" - version %d (%d)\n", dsu_socketfd->version, (int) getpid());
		
		DSU_DEBUG_PRINT(" > Unlock status %d (%d)\n", dsu_socketfd->port, (int) getpid());
		sem_post(dsu_socketfd->status_sem);
	
	}
	
    dsu_sockets_add(&dsu_program_state.binds, dsu_socketfd);

}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    DSU_DEBUG_PRINT("Bind(%d, addr, len) (%d)\n", sockfd, (int) getpid());
    /*  Bind is used to accept a client connection on an socket, this means it is a "public" socket 
        that is ready to accept requests. */
    
	
	/* 	Initialize the shadow data structure. */
	struct dsu_socket_list dsu_socketfd;
	dsu_init_fd(&dsu_socketfd, addr);
    dsu_socketfd.fd = sockfd;

    
    /*  Bind socket, if it fails and already exists we know that another DSU application is 
        already running. These applications need to know each others listening ports. */
    if ( dsu_version_fd(&dsu_socketfd) == -1) {


		if (dsu_recieve_fd(&dsu_socketfd) > 0) {
                

			dsu_init_new_version(&dsu_socketfd);
				

            /*	Inherited file descriptor no need to bind. */
            return 0;
        }
    }
    

	dsu_init_first_version(&dsu_socketfd);
	
	
	
    return dsu_bind(sockfd, addr, addrlen);
}


int close(int sockfd) {
	DSU_DEBUG_PRINT("Close() fd: %d (%d)\n", sockfd, (int) getpid());
	/*	close() closes a file descriptor, so that it no longer refers to any file and may be reused. Therefore, the the shadow file descriptor
		should also be removed / closed. The file descriptor can exist in:
			2.	Binded sockets 		-> 	dsu_program_State.binds
			3.	Internal sockets	-> 	dsu_program_State.binds->comfd | dsu_program_State.binds->fds
			4.	Connected clients	-> 	dsu_program_state.accepted	*/

	
	/*	Epoll is active, close also deletes from the epoll list. */
	if (dsu_epoll_fds != NULL) {
		dsu_socket_remove_fds(&dsu_epoll_fds, sockfd);
	}
		

	struct dsu_socket_list *dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
	if (dsu_socketfd != NULL) {
		DSU_DEBUG_PRINT(" - Binded socket %d=%d (%d)\n",dsu_socketfd->fd, dsu_socketfd->fd, (int) getpid());
		dsu_sockets_remove_fd(&dsu_program_state.binds, sockfd);
		return dsu_close(sockfd);
	}

	
	return dsu_close(sockfd);

}

