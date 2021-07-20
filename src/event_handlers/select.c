#define _GNU_SOURCE
       
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#include "../core.h"
#include "../state.h"
#include "../communication.h"


int (*dsu_select)(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int (*dsu_pselect)(int, fd_set *, fd_set *, fd_set *, const struct timespec *, const sigset_t *);


int correction = 0;
int max_fds = 0;
int transfer = 0;


void dsu_sniff_conn(struct dsu_socket_list *dsu_sockfd, fd_set *readfds) {
    /*  Listen under the hood to accepted connections on public socket. A new generation can request
        file descriptors. During DUAL listening, only one  */
    

	/*	HACK if readfds is NULL TO DO set in program state so that malloc can be called and it will be removed. */
	if (readfds == NULL) return;
	
	if (dsu_sockfd->monitoring) {
		DSU_DEBUG_PRINT(" - Add %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
		
		FD_SET(dsu_sockfd->comfd, readfds);
		if (max_fds < dsu_sockfd->comfd + 1) max_fds = dsu_sockfd->comfd + 1;
	}
	
    
	/* Contains zero or more accepted connections. */
	struct dsu_fd_list *comfds = dsu_sockfd->comfds;   
	
	while (comfds != NULL) {

	    DSU_DEBUG_PRINT(" - Add %d (%d-%d)\n", comfds->fd, (int) getpid(), (int) gettid());
	    
		FD_SET(comfds->fd, readfds);
		if (max_fds < comfds->fd + 1) max_fds = comfds->fd + 1;
		
	    comfds = comfds->next;        
	}
	
}


void dsu_handle_conn(struct dsu_socket_list *dsu_sockfd, fd_set *readfds) {
    DSU_DEBUG_PRINT(" - Handle (%d-%d)\n", (int) getpid(), (int) gettid());
	
	/*  Race conditions could happend when a "late" fork is performed. The fork happend after 
        accepting dsu communication connection. */
	if (readfds != NULL && FD_ISSET(dsu_sockfd->comfd, readfds)) {
		

		++correction; // Reduction of connections in readfds.
	

		dsu_accept_internal_connection(dsu_sockfd);
		

		DSU_DEBUG_PRINT(" - remove: %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
		FD_CLR(dsu_sockfd->comfd, readfds);
	}
	

    struct dsu_fd_list *comfds =   dsu_sockfd->comfds;   
   	

    while (comfds != NULL) {
    

        if (readfds != NULL && FD_ISSET(comfds->fd, readfds)) {

			
			DSU_DEBUG_PRINT(" - remove %d (%d-%d)\n", comfds->fd, (int) getpid(), (int) gettid());
            FD_CLR(comfds->fd, readfds);

			
			++correction;
            

			comfds = dsu_respond_internal_connection(dsu_sockfd, comfds);

			
			
		
			
        } else {

        
        	comfds = comfds->next;


		}       
    }
}


void dsu_pre_select(struct dsu_socket_list *dsu_sockfd, fd_set *readfds) {
    /*  Change socket file descripter to its shadow file descriptor. */  
	
    if (readfds != NULL && FD_ISSET(dsu_sockfd->fd, readfds)) {

		/*  Deactivate original file descriptor. */
		FD_CLR(dsu_sockfd->fd, readfds);


		if (dsu_sockfd->monitoring) { 
			

			DSU_DEBUG_PRINT(" - Lock status %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
			if (sem_wait(dsu_sockfd->status_sem) == 0) {
			
                if (dsu_sockfd->status[DSU_TRANSFER] > 0) transfer = 1;
                
                /*  Only one process can monitor a blocking socket. During transfer use lock. */
				if (transfer && dsu_sockfd->blocking) {
					
                    
					DSU_DEBUG_PRINT(" - Try lock fd %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
					if (sem_trywait(dsu_sockfd->fd_sem) == 0) {
						

						DSU_DEBUG_PRINT(" - Lock fd %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
						dsu_sockfd->locked = DSU_LOCKED;

						
						/*  Set shadow file descriptor. */
						DSU_DEBUG_PRINT(" - Set %d => %d (%d-%d)\n", dsu_sockfd->fd, dsu_sockfd->shadowfd, (int) getpid(), (int) gettid());
						FD_SET(dsu_sockfd->shadowfd, readfds);
						if (max_fds < dsu_sockfd->shadowfd + 1) max_fds = dsu_sockfd->shadowfd + 1;

					}

				
				} else {
				
				
					/*  Set shadow file descriptor. */
					DSU_DEBUG_PRINT(" - Set %d => %d (%d-%d)\n", dsu_sockfd->fd, dsu_sockfd->shadowfd, (int) getpid(), (int) gettid());
					FD_SET(dsu_sockfd->shadowfd, readfds);
					if (max_fds < dsu_sockfd->shadowfd + 1) max_fds = dsu_sockfd->shadowfd + 1;

				}
			

				DSU_DEBUG_PRINT(" - Unlock status %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
				sem_post(dsu_sockfd->status_sem);
			}
		}
    }
}


void dsu_post_select(struct dsu_socket_list *dsu_sockfd, fd_set *readfds) {
    /*  Change shadow file descripter to its original file descriptor. */


    if (dsu_sockfd->locked == DSU_LOCKED) {
        DSU_DEBUG_PRINT(" - Unlock fd %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
        sem_post(dsu_sockfd->fd_sem);
        dsu_sockfd->locked = DSU_UNLOCKED;
    }

    
    if (readfds != NULL && FD_ISSET(dsu_sockfd->shadowfd, readfds)) {
		DSU_DEBUG_PRINT(" - Reset %d => %d (%d-%d)\n", dsu_sockfd->shadowfd, dsu_sockfd->fd, (int) getpid(), (int) gettid());
        FD_CLR(dsu_sockfd->shadowfd, readfds);
        FD_SET(dsu_sockfd->fd, readfds);
    }
        
}


int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask) {
	DSU_DEBUG_PRINT("pselect(%d, set, set, set, timeout) (%d-%d)\n", nfds, (int) getpid(), (int) gettid());
    /*  Select() allows a program to monitor multiple file descriptors, waiting until one or more of the file descriptors 
        become "ready". The DSU library will modify this list by removing file descriptors that are transfered, and change
        file descriptors to their shadow file descriptors. */
	
	
	/*  Store original file descriptor sets so select can be called recursively when only internal traffic triggers the 
        select() function. */
	fd_set original_readfds; if (readfds != NULL) original_readfds = *readfds;
	fd_set original_writefds; if (writefds != NULL) original_writefds = *writefds;
    fd_set original_exceptfds; if (exceptfds != NULL) original_exceptfds = *exceptfds;
	

    /*  It might be necessary to increase the maximum file descriptors that need to be monitored. */
	max_fds = nfds;


    /*  This will be marked to 1 if one of the sockets is transfering a file descriptor, this is used to activate locking
        and decrease the timeout. */
	transfer = 0;
	

    #if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( readfds != NULL && FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - User listening: %d (%d-%d)\n", i, (int) getpid(), (int) gettid());
		}
	#endif
	

	DSU_INITIALIZE_EVENT;
	

    /*  Convert to shadow file descriptors, this must be done for binded sockets. */
    dsu_forall_sockets(dsu_program_state.binds, dsu_pre_select, readfds);


    /*  Sniff on internal communication.  */    
    dsu_forall_sockets(dsu_program_state.binds, dsu_sniff_conn, readfds);


	/* To support non-blocking sockets, narrow down the time between locks. */
	const struct timespec ts = {.tv_sec = 0, .tv_nsec = 100000 /* 100 microseconds (usec). */ };
	const struct timespec *pts = timeout;
	if (transfer == 1 && timeout == NULL) {
		pts = &ts;
	}


	#if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if (readfds != NULL && FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - Listening: %d (%d-%d)\n", i, (int) getpid(), (int) gettid());
		}
	DSU_DEBUG_PRINT(" - Size: %d (%d-%d)\n", max_fds, (int) getpid(), (int) gettid());
	#endif


    int result = dsu_pselect(max_fds, readfds, writefds, exceptfds, pts, sigmask);
    if (result == -1) {
		DSU_DEBUG_PRINT(" - error: (%d-%d)\n", (int) getpid(), (int) gettid());
		return result;
	}
	
    
	#if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( readfds != NULL && FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - Incomming: %d (%d-%d)\n", i, (int) getpid(), (int) gettid());
		}
	#endif
    

    /*  Handle messages of new processes. */ 
    dsu_forall_sockets(dsu_program_state.binds, dsu_handle_conn, readfds);
    

    /*  Convert shadow file descriptors back to user level file descriptors to avoid changing the external behaviour. */
    dsu_forall_sockets(dsu_program_state.binds, dsu_post_select, readfds);

   	
    /*  Used respond the correct number of file descriptors that triggered the select function. */			
	int _correction = correction;
	correction = 0;
	
	
	/* Avoid changing external behaviour. Most applications cannot handle return value of 0, hence call select recursively. */
	if (result - _correction == 0 && timeout == NULL) {
		DSU_DEBUG_PRINT(" - Restart Select() (%d-%d)\n", (int) getpid(), (int) gettid());


		if (readfds != NULL) *readfds = original_readfds;
		if (writefds != NULL) *writefds = original_writefds;
		if (exceptfds != NULL) *exceptfds = original_exceptfds;


		return dsu_pselect(nfds, readfds, writefds, exceptfds, NULL, NULL);

	}


	#if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( readfds != NULL && FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - User incomming: %d (%d-%d)\n", i, (int) getpid(), (int) gettid());
		}
	DSU_DEBUG_PRINT(" - Return size: %d (%d-%d)\n", result - _correction, (int) getpid(), (int) gettid());
	#endif
	
	
	return result - _correction;
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

