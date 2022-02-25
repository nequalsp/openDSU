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
int dsu_live = 0;


int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask) {
    DSU_DEBUG_PRINT(" - PSELECT (%d-%d)\n", (int) getpid(), (int) gettid());
    return 0;
}


void dsu_sniff_conn(struct dsu_socket_list *dsu_sockfd, fd_set *readfds) {
    /*  Listen under the hood to accepted connections on public socket. A new generation can request
        file descriptors. During DUAL listening, only one  */
    

	/*	HACK if readfds is NULL TO DO set in program state so that malloc can be called and it will be removed. */
	if (readfds == NULL) return;
	
	
	if (!dsu_sockfd->ready) {
		DSU_DEBUG_PRINT(" - Add %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
		FD_SET(dsu_sockfd->comfd, readfds);
		if (max_fds < dsu_sockfd->comfd + 1) max_fds = dsu_sockfd->comfd + 1;
	
    
        DSU_DEBUG_PRINT(" - Add %d (%d-%d)\n", dsu_sockfd->readyfd, (int) getpid(), (int) gettid());
	    FD_SET(dsu_sockfd->readyfd, readfds);
        if (max_fds < dsu_sockfd->readyfd + 1) max_fds = dsu_sockfd->readyfd + 1;
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
	
    DSU_DEBUG_PRINT(" - Handle on %d (%d-%d)\n", dsu_sockfd->port, (int) getpid(), (int) gettid());
	
	/*  Race conditions could happend when a "late" fork is performed. The fork happend after 
        accepting dsu communication connection. */

	// ADD: && !dsu_sockfd->ready
	if (readfds != NULL && FD_ISSET(dsu_sockfd->comfd, readfds)) {
		
		++correction;
	

		int size = sizeof(dsu_sockfd->comfd_addr);
		int acc = dsu_accept4(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, (socklen_t *) &size, SOCK_NONBLOCK);
		
		
		if ( acc != -1) {
		    DSU_DEBUG_PRINT("  - Accept %d on %d (%d-%d)\n", acc, dsu_sockfd->comfd, (int) getpid(), (int) gettid());
		    dsu_socket_add_fds(dsu_sockfd, acc, DSU_INTERNAL_FD);
		} else
			DSU_DEBUG_PRINT("  - Accept failed (%d-%d)\n", (int) getpid(), (int) gettid());
			
		
		DSU_DEBUG_PRINT(" - remove: %d (%d-%d)\n", dsu_sockfd->comfd, (int) getpid(), (int) gettid());
		FD_CLR(dsu_sockfd->comfd, readfds);
	} 

	
    struct dsu_fd_list *comfds =   dsu_sockfd->comfds;   
   	
    DSU_DEBUG_PRINT(" - LOOP\n");
    while (comfds != NULL) {
        DSU_DEBUG_PRINT(" - check %d readfds %d set %d\n", comfds->fd, readfds != NULL, FD_ISSET(comfds->fd, readfds));    
        if (readfds != NULL && FD_ISSET(comfds->fd, readfds)) {

			
			DSU_DEBUG_PRINT(" - remove %d (%d-%d)\n", comfds->fd, (int) getpid(), (int) gettid());
            FD_CLR(comfds->fd, readfds);

			
			++correction;
            
            int buffer = 0;
            int port = dsu_sockfd->port;
            
            
            /*  Race condition on recv, hence do not wait, and continue on error. */
            int r = recv(comfds->fd, &buffer, sizeof(buffer), MSG_DONTWAIT);
            
            
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
            
            if (buffer == 1) {
                
                /* Mark file descriptor as transferred. */
				DSU_DEBUG_PRINT(" - port %d is ready in the new version\n", dsu_sockfd->port);
				const char *buf = "ready";
				if (send(dsu_sockfd->markreadyfd, &buf, 5, MSG_CONFIRM) < 0) {
					DSU_DEBUG_PRINT(" - send to readyfd %d failed\n", dsu_sockfd->readyfd);
				}
				
                dsu_close(comfds->fd);

                struct dsu_fd_list *_comfds = comfds;
                comfds = comfds->next;
                dsu_socket_remove_fds(dsu_sockfd, _comfds->fd, DSU_INTERNAL_FD);

				continue;
            
            } else {
           
                DSU_DEBUG_PRINT(" - Send file descriptors %d & %d on %d (%d-%d)\n", dsu_sockfd->fd, dsu_sockfd->comfd, comfds->fd, (int) getpid(), (int) gettid());
                dsu_write_fd(comfds->fd, dsu_sockfd->fd, port); // handle return value;
			    dsu_write_fd(comfds->fd, dsu_sockfd->comfd, port);

            }
			
			
        }
        
        dsu_next_comfd:
            comfds = comfds->next;
                
    }

}


void dsu_pre_select(struct dsu_socket_list *dsu_sockfd, fd_set *readfds) {
    /*  Change socket file descripter to its shadow file descriptor. */  
	
    //if (readfds != NULL && FD_ISSET(dsu_sockfd->fd, readfds)) {

		
		/* Mark ready to be listening. */
	//	if (dsu_sockfd->comfd_close > 0) {
	//		DSU_DEBUG_PRINT(" - port %d is ready in on the side of the new version\n", dsu_sockfd->port);
    //       DSU_DEBUG_PRINT(" - send ready %d\n", dsu_sockfd->comfd_close);
    //        int buf = 1;
    //        if (send(dsu_sockfd->comfd_close, &buf, sizeof(int), MSG_CONFIRM) != -1) {
    //            DSU_DEBUG_PRINT(" - close %d\n", dsu_sockfd->comfd_close);
	//		    dsu_close(dsu_sockfd->comfd_close);
	//		    dsu_sockfd->comfd_close = -1;
    //        }
	//			dsu_sockfd->comfd_close = -1;
	//	}


    //}
}


void dsu_post_select(struct dsu_socket_list *dsu_sockfd, fd_set *readfds) {
    /*  Change shadow file descripter to its original file descriptor. */


    if (readfds != NULL && FD_ISSET(dsu_sockfd->readyfd, readfds)) {
        dsu_sockfd->ready = 1;
		++correction;
		FD_CLR(dsu_sockfd->readyfd, readfds);
    }

	
    if (readfds != NULL && FD_ISSET(dsu_sockfd->fd, readfds)) {

		if (dsu_sockfd->ready) {
			DSU_DEBUG_PRINT(" - remove public  %d (%d-%d)\n", dsu_sockfd->fd, (int) getpid(), (int) gettid());
			FD_CLR(dsu_sockfd->fd, readfds);
			++correction;
		}
    }

	if (readfds != NULL && FD_ISSET(dsu_sockfd->fd, readfds)) {

		
		/* Mark ready to be listening. */
		if (dsu_sockfd->comfd_close > 0) {
			DSU_DEBUG_PRINT(" - port %d is ready in on the side of the new version\n", dsu_sockfd->port);
            DSU_DEBUG_PRINT(" - send ready %d\n", dsu_sockfd->comfd_close);
            int buf = 1;
            if (send(dsu_sockfd->comfd_close, &buf, sizeof(int), MSG_CONFIRM) != -1) {
                DSU_DEBUG_PRINT(" - close %d\n", dsu_sockfd->comfd_close);
			    dsu_close(dsu_sockfd->comfd_close);
            }
			dsu_sockfd->comfd_close = -1;
		}


    }
	

	//if (readfds != NULL && FD_ISSET(dsu_sockfd->readyfd, readfds)) {
	//	++correction;
	//	FD_CLR(dsu_sockfd->readyfd, readfds);
	//}
	
        
}


int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
	DSU_DEBUG_PRINT("Select(%d, set, set, set, timeout) (%d-%d)\n", nfds, (int) getpid(), (int) gettid());
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
	

    #if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if ( readfds != NULL && FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - User listening: %d (%d-%d)\n", i, (int) getpid(), (int) gettid());
		}
	#endif
	

	/* 	On first call to select, mark worker active and configure binded sockets. */
	if (!dsu_live) {
		if (dsu_activate_process() > 0) dsu_live = 1;
    }
	

	if (dsu_termination_detection()) {
		dsu_terminate();
	}
	

    /*  Convert to shadow file descriptors, this must be done for binded sockets. */
    dsu_forall_sockets(dsu_program_state.binds, dsu_pre_select, readfds);


    /*  Sniff on internal communication.  */    
    dsu_forall_sockets(dsu_program_state.binds, dsu_sniff_conn, readfds);


	#if DSU_DEBUG == 1
	for(int i = 0; i < FD_SETSIZE; i++)
		if (readfds != NULL && FD_ISSET(i, readfds) ) {
			DSU_DEBUG_PRINT(" - Listening: %d (%d-%d)\n", i, (int) getpid(), (int) gettid());
		}
	DSU_DEBUG_PRINT(" - Size: %d (%d-%d)\n", max_fds, (int) getpid(), (int) gettid());
	#endif

    
    int result = dsu_select(max_fds, readfds, writefds, exceptfds, timeout);
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


		return select(nfds, readfds, writefds, exceptfds, NULL);

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
