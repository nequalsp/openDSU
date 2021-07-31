#define _GNU_SOURCE
       
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "../core.h"
#include "../state.h"
#include "../communication.h"


int (*dsu_select)(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int (*dsu_pselect)(int, fd_set *, fd_set *, fd_set *, const struct timespec *, const sigset_t *);


int dsu_correction_select = 0;
int dsu_max_fds = 0;
int dsu_select_transfer = 0;
fd_set dsu_zero;


void dsu_sniff_conn(struct dsu_socket_list *dsu_sockfd, fd_set *readfds) {
    /*  Listen under the hood to accepted connections on public socket. A new generation can request
        file descriptors. During DUAL listening, only one  */
    

	if (readfds == NULL) return;
	

	if (dsu_sockfd->monitoring) {
		DSU_DEBUG_PRINT(" - Add %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
		
		FD_SET(dsu_sockfd->comfd, readfds);
		if (dsu_max_fds < dsu_sockfd->comfd + 1) dsu_max_fds = dsu_sockfd->comfd + 1;
	}

	
}


void dsu_handle_conn(struct dsu_socket_list *dsu_sockfd, fd_set *readfds) {
    DSU_DEBUG_PRINT(" - Handle (%d-%d)\n", (int) getpid(), (int) gettid());
	
	/*  Race conditions could happend when a "late" fork is performed. The fork happend after 
        accepting dsu communication connection. */
	if (readfds != NULL && FD_ISSET(dsu_sockfd->comfd, readfds)) {
		DSU_DEBUG_PRINT(" - test (%d-%d)\n", (int) getpid(), (int) gettid());

		++dsu_correction_select; // Reduction of connections in readfds.
	

		dsu_send_fd(dsu_sockfd);
		

		DSU_DEBUG_PRINT(" - remove: %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
		FD_CLR(dsu_sockfd->comfd, readfds);
	}
	
}


void dsu_pre_select(struct dsu_socket_list *dsu_sockfd, fd_set *readfds) {
    /*  Change socket file descripter to its shadow file descriptor. */  
	
    if (readfds != NULL && FD_ISSET(dsu_sockfd->fd, readfds)) {


		if (dsu_sockfd->monitoring) { 
			

			DSU_DEBUG_PRINT(" - Lock status %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
			if (sem_wait(dsu_sockfd->status_sem) == 0) {
			

                if (dsu_sockfd->status[DSU_TRANSFER] > 0) dsu_select_transfer = 1;

                
                /*  Only one process can monitor a blocking socket. During transfer use lock. */
				if (dsu_sockfd->status[DSU_TRANSFER] > 0 && !(fcntl(dsu_sockfd->fd, F_GETFL, 0) & O_NONBLOCK) ) {
					
                    
					DSU_DEBUG_PRINT(" - Try lock fd %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
					if (sem_trywait(dsu_sockfd->fd_sem) == 0) {
						

						DSU_DEBUG_PRINT(" - Lock fd %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
						dsu_sockfd->locked = DSU_LOCKED;

						
						

					} else {


						DSU_DEBUG_PRINT(" - Remove fd %d (%d-%d)\n", dsu_sockfd->fd, (int) getpid(), (int) gettid());
						FD_CLR(dsu_sockfd->fd, readfds);


					}

				
				}
			

				DSU_DEBUG_PRINT(" - Unlock status %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
				sem_post(dsu_sockfd->status_sem);
			}
		} else {


			DSU_DEBUG_PRINT(" - Remove fd %d (%d-%d)\n", dsu_sockfd->fd, (int) getpid(), (int) gettid());
			FD_CLR(dsu_sockfd->fd, readfds);


		}
    } 
}


void dsu_unlock_select(struct dsu_socket_list *dsu_sockfd) {
	
	if (dsu_sockfd->locked == DSU_LOCKED) {
        DSU_DEBUG_PRINT(" - Unlock fd %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
        sem_post(dsu_sockfd->fd_sem);
        dsu_sockfd->locked = DSU_UNLOCKED;
    }
	
}


int fpselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask) {


    /*  It might be necessary to increase the maximum file descriptors that need to be monitored. */
	dsu_max_fds = nfds;


    /*  This will be marked to 1 if one of the sockets is transfering a file descriptor, this is used to activate locking
        and decrease the timeout. */
	dsu_select_transfer = 0;
	

    #ifdef DEBUG
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( readfds != NULL && FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - User listening: %d (%d-%d)\n", i, (int) getpid(), (int) gettid());
		}
	#endif
	

	DSU_PRE_EVENT;
	

    dsu_forall_sockets(dsu_program_state.binds, dsu_pre_select, readfds);


    dsu_forall_sockets(dsu_program_state.binds, dsu_sniff_conn, readfds);

	
	FD_ZERO(&dsu_zero);
	if (!memcmp(&dsu_zero, readfds, sizeof(dsu_zero)) && dsu_select_transfer == 0) DSU_DEACTIVATE;


	DSU_TERMINATION;
	

	#ifdef DEBUG
	for(int i = 0; i < FD_SETSIZE; i++)
		if (readfds != NULL && FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - Listening: %d (%d-%d)\n", i, (int) getpid(), (int) gettid());
		}
	DSU_DEBUG_PRINT(" - Size: %d (%d-%d)\n", dsu_max_fds, (int) getpid(), (int) gettid());
	#endif


    int result = dsu_pselect(dsu_max_fds, readfds, writefds, exceptfds, timeout, sigmask);
    if (result == -1) {
		DSU_DEBUG_PRINT(" - error: (%d-%d)\n", (int) getpid(), (int) gettid());	

		/*	Unlock binded file descriptors. */
		dsu_forall_sockets(dsu_program_state.binds, dsu_unlock_select);
		
		return result;
	}
	
    
	#ifdef DEBUG
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( readfds != NULL && FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - Incomming: %d (%d-%d)\n", i, (int) getpid(), (int) gettid());
		}
	#endif
    

    /*  Handle messages of new processes. */ 
    dsu_forall_sockets(dsu_program_state.binds, dsu_handle_conn, readfds);
    

	/*	Unlock binded file descriptors. */
	dsu_forall_sockets(dsu_program_state.binds, dsu_unlock_select);

   	
    /*  Used respond the correct number of file descriptors that triggered the select function. */			
	int _correction = dsu_correction_select;
	dsu_correction_select = 0;


	#ifdef DEBUG
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( readfds != NULL && FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - User incomming: %d (%d-%d)\n", i, (int) getpid(), (int) gettid());
		}
	DSU_DEBUG_PRINT(" - Return size: %d (%d-%d)\n", result - _correction, (int) getpid(), (int) gettid());
	#endif
	
	
	return result - _correction;
}


int rpselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask) {
	

	const struct timespec ts = {.tv_sec = 0, .tv_nsec = 500000 /* 500 microseconds (usec). */ };
	const struct timespec *ps = timeout;
	if (dsu_select_transfer == 1) {
		ps = &ts;
	}


	fd_set original_readfds; 	if (readfds != NULL) original_readfds = *readfds;
	fd_set original_writefds; 	if (writefds != NULL) original_writefds = *writefds;
    fd_set original_exceptfds; 	if (exceptfds != NULL) original_exceptfds = *exceptfds;
	

	int v = 0;	
	while ( v == 0 ) {
		

		if (readfds != NULL) *readfds = original_readfds;
		if (writefds != NULL) *writefds = original_writefds;
		if (exceptfds != NULL) *exceptfds = original_exceptfds;		


		v = fpselect(nfds, readfds, writefds, exceptfds, ps, sigmask);
	}
	

	return v;

}


int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask) {
	DSU_DEBUG_PRINT("pselect(%d, set, set, set, timeout) (%d-%d)\n", nfds, (int) getpid(), (int) gettid());
    /*  Select() allows a program to monitor multiple file descriptors, waiting until one or more of the file descriptors 
        become "ready". The DSU library will modify this list by removing file descriptors that are transfered, and change
        file descriptors to their shadow file descriptors. */
	
	
	/* 	During transfer, rotate faster to have short file descriptor locks. */
	if (timeout == NULL) {
		return rpselect(nfds, readfds, writefds, exceptfds, timeout, sigmask);
	}

	
	return fpselect(nfds, readfds, writefds, exceptfds, timeout, sigmask);
}


int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
	DSU_DEBUG_PRINT("Select() (%d)\n", (int) getpid());


	struct timespec ts; struct timespec *pts = NULL;
	if (timeout != NULL) {
		TIMEVAL_TO_TIMESPEC(timeout, &ts);
		pts = &ts;
	}


	return pselect(nfds, readfds, writefds, exceptfds, pts, NULL);
	
}

