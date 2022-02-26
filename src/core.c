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
//#include <limits.h>
#include <sys/wait.h>


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

#ifdef DEBUG
ssize_t (*dsu_read)(int, void *, size_t);
ssize_t (*dsu_recv)(int, void *, size_t, int); 
ssize_t (*dsu_recvfrom)(int, void *restrict, size_t, int, struct sockaddr *restrict, socklen_t *restrict);
ssize_t (*dsu_recvmsg)(int, struct msghdr *, int);
ssize_t (*dsu_write)(int, const void *, size_t);
ssize_t (*dsu_send)(int, const void *, size_t, int);
ssize_t (*dsu_sendto)(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
ssize_t (*dsu_sendmsg)(int, const struct msghdr *, int);
int (*dsu_listen)(int, int);
#endif

//pid_t (*dsu_waitpid)(pid_t pid, int *status, int options);

int dsu_inherit_fd(struct dsu_socket_list *dsu_sockfd) {
	DSU_DEBUG_PRINT(" - Inherit fd %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
	/*	Connect to the previous version, based on named unix domain socket, and receive the file descriptor that is 
		binded to the same port. Also, receive the file descriptor of the named unix domain socket so internal
		communication can be taken over when the update completes. */


	DSU_DEBUG_PRINT("  - Connect on %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
    if ( connect(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, sizeof(dsu_sockfd->comfd_addr)) != -1) {
        
		
        DSU_DEBUG_PRINT("  - Send on %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
        if ( send(dsu_sockfd->comfd, &dsu_sockfd->port, sizeof(dsu_sockfd->port), 0) > 0) {

			DSU_TEST_PRINT("  - Receive fd on %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
            DSU_DEBUG_PRINT("  - Receive on %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
            int port = 0; int _comfd = 0; int _sockfd;
            dsu_read_fd(dsu_sockfd->comfd, &_sockfd, &port);	// Handle return value;
			dsu_read_fd(dsu_sockfd->comfd, &_comfd, &port);
			DSU_TEST_PRINT("  - Received fd %d & %d (%d-%d)\n", _sockfd, _comfd, (int) getpid(), (int) gettid());
			DSU_DEBUG_PRINT("  - Received %d & %d (%d-%d)\n", _sockfd, _comfd, (int) getpid(), (int) gettid());

			if (port > 0) {
				
				/* Connect socket to the same v-node, but keep socket settings. */
				int flags = fcntl(dsu_sockfd->fd, F_GETFL);
				if (dup2(_sockfd, dsu_sockfd->fd) == -1) {
					fcntl(dsu_sockfd->fd, F_SETFL, flags);
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
	DSU_DEBUG_PRINT(" - Termination detection (%d-%d)\n", (int) getpid(), (int) gettid());

	
	int buf;
	int r = recv(dsu_program_state.close, &buf, 1, MSG_DONTWAIT | MSG_PEEK);
	//DSU_TEST_PRINT("  - Termination? open: %d (%d-%d)\n", r, (int) getpid(), (int) gettid());
	DSU_DEBUG_PRINT("  - Termination? open: %d (%d-%d)\n", r, (int) getpid(), (int) gettid());
	if (r > 0) {
		return 0;
	}

	//if (!(r == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))) {
	//	return 0;
	//}
	

	struct dsu_socket_list *current = dsu_program_state.binds;
	while (current != NULL) {
		DSU_DEBUG_PRINT("  - Termination? ready: %d (%d-%d)\n", !current->ready, (int) getpid(), (int) gettid());
        if (	!current->ready
				|| current->comfds != NULL
			 )
			return 0;        

		current = current->next;

	}
	
	
	return 1;

}


void ping() {
	DSU_TEST_PRINT(" - Send ping (%d, %d)\n", (int) getpid(), (int) gettid());
	const char buf = 'w';
	if (send(dsu_program_state.ping, &buf, 1, MSG_CONFIRM) < 0) {
		DSU_DEBUG_PRINT(" - Send to ping failed (%d, %d)\n", (int) getpid(), (int) gettid());
		DSU_TEST_PRINT(" - Send to ping failed (%d, %d)\n", (int) getpid(), (int) gettid());
	}
}


void dsu_terminate() {
	DSU_TEST_PRINT(" - Termination (%d-%d)\n", (int) getpid(), (int) gettid());
	DSU_DEBUG_PRINT(" - Termination (%d-%d)\n", (int) getpid(), (int) gettid());
	/*	Different models, such as master-worker model, are used to horizontally scale the application. This
		can either be done with threads or processes. As threads are implemented as processes on linux, 
		there is not difference in termination. The number of active workers is tracked in the event handler. 
		The last active worker that terminates, terminates the group to ensure the full application stops. */
	
    
	int workers = dsu_deactivate_process();

	
	if (workers == 0) {
		DSU_TEST_PRINT("  - pgkill all (pg:%d, pid:%d, tid:%d)\n", (int) getpgid(getpid()), (int) getpid(), (int) gettid());
		DSU_DEBUG_PRINT("  - pgkill all (pg:%d, pid:%d, tid:%d)\n", (int) getpgid(getpid()), (int) getpid(), (int) gettid());
		killpg(getpgid(getpid()), SIGKILL);
	}
	
	ping();
	
	DSU_DEBUG_PRINT("  - Sigwait() (pg:%d, pid:%d, tid:%d)\n",(int) getpgid(getpid()), (int) getpid(), (int) gettid());
	sigset_t sigset; int sig;
  	sigemptyset(&sigset);
  	sigaddset(&sigset, SIGKILL);
	
	sigwait(&sigset, &sig);
	
	//DSU_DEBUG_PRINT("  - kill() (pg:%d, pid:%d, tid:%d)\n",(int) getpgid(getpid()), (int) getpid(), (int) gettid());
    //kill(getpid(), SIGTERM);

}

void dsu_init_worker() {
	DSU_DEBUG_PRINT(" - Initialize worker (%d-%d)\n", (int) getpid(), (int) gettid());
	//dsu_program_state.processes = dup(dsu_program_state.processes);
}


int dsu_change_number_of_workers(int delta) {

	if (delta > 0) {
		
		const char buf = 'a';
		if (send(dsu_program_state.activate, &buf, 1, MSG_CONFIRM) < 0) {
			DSU_DEBUG_PRINT(" - Write to activate failed (%d, %d)\n", (int) getpid(), (int) gettid());
			DSU_TEST_PRINT(" - Write to activate failed (%d, %d)\n", (int) getpid(), (int) gettid());
		}
	
	} else {
		
		char buf;
		int r = recv(dsu_program_state.deactivate, &buf, 1, MSG_DONTWAIT);
		if (r < 0) {
			DSU_DEBUG_PRINT(" - Read from deactivate failed (%d, %d)\n", (int) getpid(), (int) gettid());
			DSU_TEST_PRINT(" - Read from deactivate failed (%d, %d)\n", (int) getpid(), (int) gettid());
		}

	}

	char buf;
	return recv(dsu_program_state.deactivate, &buf, 1, MSG_DONTWAIT | MSG_PEEK) > 0;

}
    //DSU_DEBUG_PRINT(" < Lock process file (%d-%d)\n", (int) getpid(), (int) gettid());
    //fcntl(dsu_program_state.processes, F_SETLKW, dsu_program_state.write_lock);
    
    //char buf[2] = {0};
    //lseek(dsu_program_state.processes, 0, SEEK_SET); 
    //if (read(dsu_program_state.processes, buf, 1) == -1) {
	//	DSU_DEBUG_PRINT(" - Error reading process file (%d-%d)\n", (int) getpid(), (int) gettid());
	//	DSU_DEBUG_PRINT(" > Unlock process file (%d-%d)\n", (int) getpid(), (int) gettid());
    //	fcntl(dsu_program_state.processes, F_SETLKW, dsu_program_state.unlock);
    //   return -1;
    //}
    
    //int _size = strtol(buf, NULL, 10);
    //int size = _size + delta;
    //DSU_DEBUG_PRINT(" - Number of processes %d to %d (%d-%d)\n", _size, size, (int) getpid(), (int) gettid());
    
    //buf[0] = size + '0';
    //lseek(dsu_program_state.processes, 0, SEEK_SET);
    //if (write(dsu_program_state.processes, buf, 1) == -1) {
	//	DSU_DEBUG_PRINT(" - Error writing process file (%d-%d)\n", (int) getpid(), (int) gettid());
	//	DSU_DEBUG_PRINT(" > Unlock process file (%d-%d)\n", (int) getpid(), (int) gettid());
    //	fcntl(dsu_program_state.processes, F_SETLKW, dsu_program_state.unlock);
    //    return -1;
    //}
    
    //DSU_DEBUG_PRINT(" > Unlock process file (%d-%d)\n", (int) getpid(), (int) gettid());
    //fcntl(dsu_program_state.processes, F_SETLKW, dsu_program_state.unlock);
    
    //return size;

//}


int dsu_deactivate_process(void) {
    return dsu_change_number_of_workers(-1);
}


int dsu_activate_process(void) {
	dsu_init_worker();
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
			int flags = fcntl(dsu_sockfd->comfd, F_GETFL) | O_NONBLOCK;
			fcntl(dsu_sockfd->comfd, F_SETFL, flags);
			
			
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
			DSU_TEST_PRINT("Error opening debugging file (%d-%d)\n", (int) getpid(), (int) gettid());
		    perror("DSU \"Error opening debugging file\"");
		    exit(EXIT_FAILURE);
	    }
	#endif


	#if defined(TEST)
		int size = snprintf(NULL, 0, "%s/dsu_%d.log", TEST, (int) getpid());
		char logfile[size+1];
		sprintf(logfile, "%s/dsu_%d.log", TEST, (int) getpid());
	    dsu_program_state.logfd = fopen(logfile, "w");
	    if (dsu_program_state.logfd == NULL) {
			DSU_TEST_PRINT("Error opening testing file (%d-%d)\n", (int) getpid(), (int) gettid());
		    perror("DSU \"Error opening testing file\"");
		    exit(EXIT_FAILURE);
	    }
	#endif
	
	
	DSU_DEBUG_PRINT("INIT() (%d-%d)\n", (int) getpid(), (int) gettid());
	DSU_TEST_PRINT("INIT() (%d-%d)\n", (int) getpid(), (int) gettid());
    

    //dsu_program_state.write_lock = (struct flock *) calloc(1, sizeof(struct flock));
    //dsu_program_state.write_lock->l_type = F_WRLCK;
    //dsu_program_state.write_lock->l_start = 0; 
    //dsu_program_state.write_lock->l_whence = SEEK_SET; 
    //dsu_program_state.write_lock->l_len = 0; 

    //dsu_program_state.unlock = (struct flock *) calloc(1, sizeof(struct flock));
    //dsu_program_state.unlock->l_type = F_UNLCK;
    //dsu_program_state.unlock->l_start = 0; 
    //dsu_program_state.unlock->l_whence = SEEK_SET; 
    //dsu_program_state.unlock->l_len = 0; 
    
    
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
	//dsu_waitpid = dlsym(RTLD_NEXT, "waitpid");
	
	
	/* 	Set default function for event-handler wrapper functions. */
	dsu_select = dlsym(RTLD_NEXT, "select");
	//dsu_poll = dlsym(RTLD_NEXT, "poll");
	//dsu_ppoll = dlsym(RTLD_NEXT, "ppoll");
	dsu_epoll_wait = dlsym(RTLD_NEXT, "epoll_wait");
	dsu_epoll_create1 = dlsym(RTLD_NEXT, "epoll_create1");
	dsu_epoll_create = dlsym(RTLD_NEXT, "epoll_create");

	#ifdef DEBUG
	dsu_read = dlsym(RTLD_NEXT, "read");
	dsu_recv = dlsym(RTLD_NEXT, "recv");
	dsu_recvfrom = dlsym(RTLD_NEXT, "recvfrom");
	dsu_recvmsg = dlsym(RTLD_NEXT, "recvmsg");
	dsu_write = dlsym(RTLD_NEXT, "write");
	dsu_send = dlsym(RTLD_NEXT, "send");
	dsu_sendto = dlsym(RTLD_NEXT, "sendto");
	dsu_sendmsg = dlsym(RTLD_NEXT, "sendmsg");
	dsu_listen = dlsym(RTLD_NEXT, "listen");
	#endif

    
    //int len = snprintf(NULL, 0, "/tmp/dsu_processes_%d.pid", (int) getpid());
	//char temp_path[len+1];
	//sprintf(temp_path, "/tmp/dsu_processes_%d.pid", (int) getpid());
    //dsu_program_state.processes = open(temp_path, O_RDWR | O_CREAT, 0600);
    //if (dsu_program_state.processes <= 0) {
    //    perror("DSU \"Error opening process file\"");
	//	exit(EXIT_FAILURE);
    //}
    //unlink(temp_path);
    //char buf = 0 + '0';
    //if (write(dsu_program_state.processes, &buf, 1) == -1) {
    //    perror("DSU \"Error initiating process file\"");
	//	exit(EXIT_FAILURE);
    //}

	
	int pair[2];
	if (socketpair(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0, pair) < 0) {
		perror("DSU \"Error generating socket pair constructor\"");
		exit(EXIT_FAILURE);
	}   
	dsu_program_state.close = pair[0];
	dsu_program_state.accept = pair[1];

	
	if (socketpair(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0, pair) < 0) {
		perror("DSU \"Error generating socket pair constructor\"");
		exit(EXIT_FAILURE);
	}   
	dsu_program_state.ping = pair[0];
	dsu_program_state.wakeup = pair[1];


	if (socketpair(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0, pair) < 0) {
		perror("DSU \"Error generating socket pair constructor\"");
		exit(EXIT_FAILURE);
	}   
	dsu_program_state.activate = pair[0];
	dsu_program_state.deactivate = pair[1];


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
		DSU_DEBUG_PRINT(" - Supressed (%d-%d)\n", (int) getpid(), (int) gettid());
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
    
	DSU_DEBUG_PRINT(" - blocking? %d (%d-%d)\n", !(fcntl(sockfd, F_GETFL, 0) & O_NONBLOCK), (int) getpid(), (int) gettid()); 
	
	
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
	DSU_DEBUG_PRINT("Accept4() %d (%d-%d)\n", sockfd, (int) getpid(), (int) gettid());
    
	int acc = dsu_accept4(sockfd, addr, addrlen, flags);
	if (acc > 0) {
		//DSU_TEST_PRINT(" - Client socket %d (%d-%d)\n", acc, (int) getpid(), (int) gettid()); 
		DSU_DEBUG_PRINT(" - New client socket %d (%d-%d)\n", acc, (int) getpid(), (int) gettid());     
		const char buf = 'a';
		if (send(dsu_program_state.accept, &buf, 1, MSG_CONFIRM) < 0) {
			DSU_DEBUG_PRINT(" - Send to accept failed (%d, %d)\n", (int) getpid(), (int) gettid());
			DSU_TEST_PRINT(" - Send to accept failed (%d, %d)\n", (int) getpid(), (int) gettid());
		}
	}
	

	//DSU_DEBUG_PRINT(" - blocking? %d (%d-%d)\n", !(fcntl(sockfd, F_GETFL, 0) & O_NONBLOCK), (int) getpid(), (int) gettid());
    
    //int sessionfd = dsu_accept4(sockfd, addr, addrlen, flags);
	//if (sessionfd == -1)
	//	return sessionfd;

    
    //DSU_DEBUG_PRINT(" - accept %d (%d-%d)\n", sessionfd, (int) getpid(), (int) gettid());
	//struct dsu_socket_list *dsu_sockfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
	//if (dsu_sockfd != NULL)
	//	dsu_socket_add_fds(dsu_sockfd, sessionfd, DSU_NON_INTERNAL_FD);
    

    return acc;
}


int close(int sockfd) {
	DSU_DEBUG_PRINT("Close() fd: %d (%d-%d)\n", sockfd, (int) getpid(), (int) gettid());
	
	int type = -1; int listen = -1; int domain = -1; socklen_t len = sizeof(int);
	int r = getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &type, &len);
	r += getsockopt(sockfd, SOL_SOCKET, SO_ACCEPTCONN, &listen, &len);
	r += getsockopt(sockfd, SOL_SOCKET, SO_DOMAIN, &domain, &len);
	//DSU_TEST_PRINT(" - Socket %d result %d Listen %d type %d domain %d (%d-%d)\n", sockfd, r, listen, type, domain, (int) getpid(), (int) gettid());
	DSU_DEBUG_PRINT(" - Socket %d result %d Listen %d type %d domain %d (%d-%d)\n", sockfd, r, listen, type, domain, (int) getpid(), (int) gettid());
	
	
	int result = dsu_close(sockfd);

	
	if (result == 0 && r == 0 && type == SOCK_STREAM && domain == AF_INET && !listen) {
		DSU_DEBUG_PRINT(" - Close client socket %d (%d-%d)\n", sockfd, (int) getpid(), (int) gettid());
		//DSU_TEST_PRINT(" - Close client socket %d (%d-%d)\n", sockfd, (int) getpid(), (int) gettid());
		char buf;
		int r = recv(dsu_program_state.close, &buf, 1, MSG_DONTWAIT);
		if (r <= 0) {
			DSU_DEBUG_PRINT(" - Read from close failed (%d, %d)\n", (int) getpid(), (int) gettid());
			DSU_TEST_PRINT(" - Read from close failed (%d, %d)\n", (int) getpid(), (int) gettid());
		}

	}
	
	return result;

}
	
	/*	close() closes a file descriptor, so that it no longer refers to any file and may be reused. Therefore, the the shadow file descriptor
		should also be removed / closed. The file descriptor can exist in:
			1.	Unbinded sockets 	-> 	dsu_program_State.sockets
			2.	Binded sockets 		-> 	dsu_program_State.binds
			3.	Internal sockets	-> 	dsu_program_State.binds->comfd | dsu_program_State.binds->fds
			4.	Connected clients	-> 	dsu_program_state.accepted	*/
	
		
	/* 	Return immediately for these file descriptors. */
    //if (sockfd == STDIN_FILENO || sockfd == STDOUT_FILENO || sockfd == STDERR_FILENO) {
    //    return dsu_close(sockfd);
    //}
	
	
	//struct dsu_socket_list * dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.sockets, sockfd);
    //if (dsu_socketfd != NULL) {
	//	DSU_DEBUG_PRINT(" - Unbinded socket (%d-%d)\n", (int) getpid(), (int) gettid());
	//	dsu_sockets_remove_fd(&dsu_program_state.sockets, sockfd);
	//	return dsu_close(sockfd);
	//}


	//dsu_socketfd = dsu_sockets_search_fd(dsu_program_state.binds, sockfd);
	//if (dsu_socketfd != NULL) {
	//	DSU_DEBUG_PRINT(" - Binded socket %d (%d-%d)\n",dsu_socketfd->fd, (int) getpid(), (int) gettid());
	//	if (dsu_socketfd->readyfd > 0) dsu_close(dsu_socketfd->readyfd);
	//	if (dsu_socketfd->comfd >0) dsu_close(dsu_socketfd->comfd);
	//	dsu_sockets_remove_fd(&dsu_program_state.binds, sockfd);
	//	return dsu_close(sockfd);
	//}
	
	
	//dsu_socketfd = dsu_sockets_search_fds(dsu_program_state.binds, sockfd, DSU_MONITOR_FD);
	//if (dsu_socketfd != NULL) {
	//	DSU_DEBUG_PRINT(" - Internal master socket (%d-%d)\n", (int) getpid(), (int) gettid());
	//	return dsu_close(sockfd);
	//}

	
	//dsu_socketfd = dsu_sockets_search_fds(dsu_program_state.binds, sockfd, DSU_INTERNAL_FD);
	//if (dsu_socketfd != NULL) {
	//	DSU_DEBUG_PRINT(" - Internal client socket (%d-%d)\n", (int) getpid(), (int) gettid());
	//	dsu_socket_remove_fds(dsu_socketfd, sockfd, DSU_INTERNAL_FD);
	//	return dsu_close(sockfd);
	//}


	//dsu_socketfd = dsu_sockets_search_fds(dsu_program_state.binds, sockfd, DSU_NON_INTERNAL_FD);
	//if (dsu_socketfd != NULL) {
	//	DSU_DEBUG_PRINT(" - Client socket (%d-%d)\n", (int) getpid(), (int) gettid());
	//	dsu_socket_remove_fds(dsu_socketfd, sockfd, DSU_NON_INTERNAL_FD);
	//	return dsu_close(sockfd);
	//}


	//return dsu_close(sockfd);

//}




#ifdef DEBUG
ssize_t read(int fd, void *buf, size_t count) {
	DSU_DEBUG_PRINT("read() %d (%d-%d)\n", fd, (int) getpid(), (int) gettid());
	return dsu_read(fd, buf, count);
}


ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
	DSU_DEBUG_PRINT("recv() %d (%d-%d)\n", sockfd, (int) getpid(), (int) gettid());
	return dsu_recv(sockfd, buf, len, flags);
}


ssize_t recvfrom(int sockfd, void *restrict buf, size_t len, int flags, struct sockaddr *restrict src_addr, socklen_t *restrict addrlen) {
	DSU_DEBUG_PRINT("recvfrom() %d (%d-%d)\n", sockfd, (int) getpid(), (int) gettid());
	return dsu_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
}


ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
	DSU_DEBUG_PRINT("recvmsg() %d (%d-%d)\n", sockfd, (int) getpid(), (int) gettid());
	return dsu_recvmsg(sockfd, msg, flags);
}


ssize_t write(int fd, const void *buf, size_t count) {
	DSU_DEBUG_PRINT("write() %d (%d-%d)\n", fd, (int) getpid(), (int) gettid());
	return dsu_write(fd, buf, count);
}


ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
	DSU_DEBUG_PRINT("send() %d (%d-%d)\n", sockfd, (int) getpid(), (int) gettid());
	return dsu_send(sockfd, buf, len, flags);
}


ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
	DSU_DEBUG_PRINT("sendto() %d (%d-%d)\n", sockfd, (int) getpid(), (int) gettid());
	return dsu_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}


ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
	DSU_DEBUG_PRINT("sendmsg() %d (%d-%d)\n", sockfd, (int) getpid(), (int) gettid());
	return dsu_sendmsg(sockfd, msg, flags);
}

int listen(int sockfd, int backlog) {
    DSU_DEBUG_PRINT("Listen() on fd %d (%d-%d)\n", sockfd, (int) getpid(), (int) gettid());
	return dsu_listen(sockfd, backlog);
	
}
#endif

//pid_t waitpid(pid_t pid, int *status, int options) {
//	DSU_DEBUG_PRINT("waitpid() on pid %d and status %d (%d-%d)\n", pid, *status, (int) getpid(), (int) gettid());
//	pid_t cpid = dsu_waitpid(pid, status, options);
//	DSU_DEBUG_PRINT(" - pid %d (%d-%d)\n", cpid, (int) getpid(), (int) gettid());
//	//errno = EINTR;
//	return 0 ? cpid > 0 : cpid;
//}








