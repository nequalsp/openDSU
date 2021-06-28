#define _GNU_SOURCE
       
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#include "../core.h"
#include "../state.h"
#include "../communication.h"


int (*dsu_select)(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int correction = 0;
int max_fds = 0;

void dsu_sniff_conn(struct dsu_socket_list *dsu_sockfd, fd_set *readfds) {
    /*  Listen under the hood to accepted connections on public socket. A new generation can request
        file descriptors. During DUAL listening, only one  */
    
	
	if (dsu_sockfd->monitoring) {
		DSU_DEBUG_PRINT(" - Add %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
		
		FD_SET(dsu_sockfd->comfd, readfds);
		if (max_fds < dsu_sockfd->comfd) max_fds = dsu_sockfd->comfd;
	}
	
    
	/* Contains zero or more accepted connections. */
	struct dsu_fd_list *comfds = dsu_sockfd->comfds;   
	
	while (comfds != NULL) {

	    DSU_DEBUG_PRINT(" - Add %d (%d-%d)\n", comfds->fd, (int) getpid(), (int) gettid());
	    
		FD_SET(comfds->fd, readfds);
		if (max_fds < comfds->fd) max_fds = comfds->fd;
		
	    comfds = comfds->next;        
	}
	
}


void dsu_handle_conn(struct dsu_socket_list *dsu_sockfd, fd_set *readfds) {
	
    DSU_DEBUG_PRINT(" - Handle on %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
	
	/*  Race conditions could happend when a "late" fork is performed. The fork happend after 
        accepting dsu communication connection. */
	if (FD_ISSET(dsu_sockfd->comfd, readfds)) {
		
		++correction;
	
		if (dsu_sockfd->monitoring) {
				    
			int size = sizeof(dsu_sockfd->comfd_addr);
			int acc = dsu_accept4(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, (socklen_t *) &size, SOCK_NONBLOCK);
			
			if ( acc != -1) {
			    DSU_DEBUG_PRINT("  - Accept %d on %d (%d-%d)\n", acc, dsu_sockfd->comfd, (int) getpid(), (int) gettid());
			    dsu_socket_add_fds(dsu_sockfd, acc, DSU_INTERNAL_FD);
			} else
				DSU_DEBUG_PRINT("  - Accept failed (%d-%d)\n", (int) getpid(), (int) gettid());
			
		}
		
		DSU_DEBUG_PRINT(" - remove: %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
		FD_CLR(dsu_sockfd->comfd, readfds);
	}
	

    struct dsu_fd_list *comfds =   dsu_sockfd->comfds;   
   	
    while (comfds != NULL) {
    
        if (FD_ISSET(comfds->fd, readfds)) {

			
			DSU_DEBUG_PRINT(" - remove %d (%d-%d)\n", comfds->fd, (int) getpid(), (int) gettid());
            FD_CLR(comfds->fd, readfds);

			
			++correction;
            
            int buffer = 0;
            int port = dsu_sockfd->port;
            
            
            /*  Race condition on recv, hence do not wait, and continue on error. */
            int r = dsu_recv(comfds->fd, &buffer, sizeof(buffer), MSG_DONTWAIT);
            
            
            /*  Connection is closed by client. */
            if (r == 0) {
                
                DSU_DEBUG_PRINT(" - Close on %d (%d-%d)\n", comfds->fd, (int) getpid(), (int) gettid());
                                 
                dsu_close(comfds->fd);
                FD_CLR(comfds->fd, readfds);
                
                struct dsu_fd_list *_comfds = comfds;
                comfds = comfds->next;
                dsu_socket_remove_fds(dsu_sockfd, _comfds->fd, DSU_INTERNAL_FD);
                
                continue;
            } 
            
            
            /*  Other process already read message. */
            else if (r == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
                goto dsu_next_comfd;
            
            
            /*  Unkown error. */
            else if (r == -1)
                port = -1;
            
            
			if (port != -1) {
				
				DSU_DEBUG_PRINT(" - Lock status %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
				sem_wait(dsu_sockfd->status_sem);
				DSU_DEBUG_PRINT(" - DSU_TRANSFER %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
				dsu_sockfd->status[DSU_TRANSFER] = 1;
				DSU_DEBUG_PRINT(" - Unlock status %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
				sem_post(dsu_sockfd->status_sem);
				
			}

			
            DSU_DEBUG_PRINT(" - Send file descriptors on %d (%d-%d)\n", comfds->fd, (int) getpid(), (int) gettid());
            dsu_write_fd(comfds->fd, dsu_sockfd->shadowfd, port); // handle return value;
			dsu_write_fd(comfds->fd, dsu_sockfd->comfd, port);
			
			
        }
        
        dsu_next_comfd:
            comfds = comfds->next;
                
    }

}


void dsu_pre_select(struct dsu_socket_list *dsu_sockfd, fd_set *readfds) {
    /*  Change socket file descripter to its shadow file descriptor. */   
   	
	
    if (FD_ISSET(dsu_sockfd->fd, readfds)) {
		
		
		FD_CLR(dsu_sockfd->fd, readfds);


		if (dsu_sockfd->monitoring) { 
			DSU_DEBUG_PRINT(" - Lock status %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
			sem_wait(dsu_sockfd->status_sem);
			
			if (dsu_sockfd->status[DSU_TRANSFER] == -1) { // Temp....
				
				DSU_DEBUG_PRINT(" - Try lock fd %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
				if (sem_trywait(dsu_sockfd->fd_sem) == 0) {
					
					DSU_DEBUG_PRINT(" - Lock fd %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
					dsu_sockfd->locked = DSU_LOCKED;

					DSU_DEBUG_PRINT(" - Set %d => %d (%d-%d)\n", dsu_sockfd->fd, dsu_sockfd->shadowfd, (int) getpid(), (int) gettid());
					FD_SET(dsu_sockfd->shadowfd, readfds);
					if (max_fds < dsu_sockfd->shadowfd) max_fds = dsu_sockfd->shadowfd;
				}
			
			} else {

				
				DSU_DEBUG_PRINT(" - Set %d => %d (%d-%d)\n", dsu_sockfd->fd, dsu_sockfd->shadowfd, (int) getpid(), (int) gettid());
				FD_SET(dsu_sockfd->shadowfd, readfds);
				if (max_fds < dsu_sockfd->shadowfd) max_fds = dsu_sockfd->shadowfd;


			}
		
			DSU_DEBUG_PRINT(" - Unlock status %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
			sem_post(dsu_sockfd->status_sem);

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

    
    if (FD_ISSET(dsu_sockfd->shadowfd, readfds)) {
		DSU_DEBUG_PRINT(" - Reset %d => %d (%d-%d)\n", dsu_sockfd->shadowfd, dsu_sockfd->fd, (int) getpid(), (int) gettid());
        FD_CLR(dsu_sockfd->shadowfd, readfds);
        FD_SET(dsu_sockfd->fd, readfds);
    }
        
}


int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
	DSU_DEBUG_PRINT("Select() (%d-%d)\n", (int) getpid(), (int) gettid());
    /*  Select() allows a program to monitor multiple file descriptors, waiting until one or more of the file descriptors 
        become "ready". The DSU library will modify this list by removing file descriptors that are transfered, and change
        file descriptors to their shadow file descriptors. */
	
	
	/* */
	fd_set original_readfds;
	if (readfds != NULL) original_readfds = *readfds;

	fd_set original_writefds;
	if (writefds != NULL) original_writefds = *writefds;

	fd_set original_exceptfds;
	if (exceptfds != NULL) original_exceptfds = *exceptfds;

	
	max_fds = nfds;
	
	
    #if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - User listening: %d (%d-%d)\n", i, (int) getpid(), (int) gettid());
		}
	#endif
	
	
	/* 	Mark process as worker.  */
	if (!dsu_program_state.live) {
		dsu_program_state.live = 1;
		sem_wait(dsu_program_state.lock);
		++dsu_program_state.workers[0];
		sem_post(dsu_program_state.lock);
	}
	

	/* 	Mark version of the file descriptors. */
	dsu_forall_sockets(dsu_program_state.binds, dsu_monitor_fd);

	if (dsu_termination_detection()) {
		dsu_terminate();
	}
	
    /*  Convert to shadow file descriptors, this must be done for binded sockets. */
    dsu_forall_sockets(dsu_program_state.binds, dsu_pre_select, readfds);
    
    
    /*  Sniff on internal communication.  */    
    dsu_forall_sockets(dsu_program_state.binds, dsu_sniff_conn, readfds);

    
    //struct timeval tv = {2, 0};
    //if (timeout != NULL) {
    //    tv = *timeout;
    //}
    

	#if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - Listening: %d (%d-%d)\n", i, (int) getpid(), (int) gettid());
		}
	#endif


    int result = dsu_select(max_fds, readfds, writefds, exceptfds, timeout);
    if (result == -1) {
		return result;
	}

    
	#if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - Incomming: %d (%d-%d)\n", i, (int) getpid(), (int) gettid());
		}
	#endif
    
    
    /*  Handle messages of new processes. */ 
    dsu_forall_sockets(dsu_program_state.binds, dsu_handle_conn, readfds);
    

    /*  Convert shadow file descriptors back to user level file descriptors to avoid changing the external behaviour. */
    dsu_forall_sockets(dsu_program_state.binds, dsu_post_select, readfds);

   				
	int _correction = correction;
	correction = 0;
	
	
	/* Cannot handle return value of 0 */
	if (result - _correction == 0 && timeout == NULL) {
		DSU_DEBUG_PRINT(" - Restart Select() (%d-%d)\n", (int) getpid(), (int) gettid());


		if (readfds != NULL) *readfds = original_readfds;
		if (writefds != NULL) *writefds = original_writefds;
		if (exceptfds != NULL) *exceptfds = original_exceptfds;


		return select(nfds, readfds, NULL, NULL, NULL);

	}


	#if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - User incomming: %d (%d-%d)\n", i, (int) getpid(), (int) gettid());
		}
	DSU_DEBUG_PRINT(" - Return size: %d (%d-%d)\n", result - _correction, (int) getpid(), (int) gettid());
	#endif
	
	
	return result - _correction;
}
