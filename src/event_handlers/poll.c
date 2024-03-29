#define _GNU_SOURCE

#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <sys/poll.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#include "../core.h"
#include "../state.h"
#include "../communication.h"


int (*dsu_poll)(struct pollfd *, nfds_t, int);
int (*dsu_ppoll)(struct pollfd *, nfds_t, const struct timespec *, const sigset_t *);

int nfds_max;
int correction;


void dsu_compress_fds(struct pollfd *fds, nfds_t *nfds) {
	/*  Remove file descriptors that are set to -1. */	


	for (int i = 0; i < *nfds; i++) {
		if (fds[i].fd == -1) {
			  
			for(int j = i; j < *nfds; j++) {
				fds[j].fd = fds[j+1].fd;
				fds[j].events = fds[j+1].events;
			}
				
			i--;
			(*nfds)--;
		}
  	}


}

void dsu_sniff_conn_poll(struct dsu_socket_list *dsu_sockfd, struct pollfd *fds, nfds_t *nfds) {
    /*  Listen under the hood to accepted connections on public socket. A new generation can request
        file descriptors. During DUAL listening, only one  */
    

	if (dsu_sockfd->monitoring) {

		DSU_DEBUG_PRINT(" - Add %d (%d)\n", dsu_sockfd->comfd, (int) getpid());
		if (*nfds < nfds_max) {
			fds[*nfds].fd = dsu_sockfd->comfd;
			fds[*nfds].events = POLLIN;
			fds[(*nfds)++].revents = 0;
		}

	}
	
    
	/* Contains zero or more accepted connections. */
	struct dsu_fd_list *comfds = dsu_sockfd->comfds;   
	
	while (comfds != NULL) {

	    DSU_DEBUG_PRINT(" - Add %d (%d)\n", comfds->fd, (int) getpid());
		if (*nfds < nfds_max) { // Avoid buffer overflow.
			fds[*nfds].fd = comfds->fd;	
			fds[*nfds].events = POLLIN;
			fds[(*nfds)++].revents = 0;
		}
		
	    comfds = comfds->next;        
	}
	
}


void dsu_handle_conn_poll(struct dsu_socket_list *dsu_sockfd, struct pollfd *fds, nfds_t start, nfds_t end) {
	DSU_DEBUG_PRINT(" - Handle on %d %lu, %lu(%d)\n", dsu_sockfd->port, start, end, (int) getpid());

	
	for(int i = start; i < end; i++) {


		if (fds[i].revents == 0) continue;


		DSU_DEBUG_PRINT("  - %d (%d)\n", fds[i].fd, (int) getpid());

		
		++correction;
		

		/* 	Accept connection requests. */
		if (fds[i].fd == dsu_sockfd->comfd) {


			/*  Race conditions could happend when a "late" fork is performed. The fork happend after 
        		accepting dsu communication connection. */
			int size = sizeof(dsu_sockfd->comfd_addr);
			int acc = dsu_accept4(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, (socklen_t *) &size, SOCK_NONBLOCK);
			
			if ( acc != -1) {
				DSU_DEBUG_PRINT("  - Accept %d on %d (%d)\n", acc, dsu_sockfd->comfd, (int) getpid());
				dsu_socket_add_fds(dsu_sockfd, acc, DSU_INTERNAL_FD);
			} else
				DSU_DEBUG_PRINT("  - Accept failed (%d)\n", (int) getpid());

			continue;
		
		}
	
	
		/* 	Respond to messages. */
		struct dsu_fd_list *comfds =   dsu_sockfd->comfds;
		while (comfds != NULL) {

	
			if (fds[i].fd == comfds->fd) {
				
		        
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
		            
		            continue;
		        } 
		        
		        
		        /*  Other process already read message. */
		        else if (r == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		            goto dsu_next_comfd;
		        
		        
		        /*  Unkown error. */
		        else if (r == -1)
		            port = -1;
		        
		        
				if (port != -1) {
					
					DSU_DEBUG_PRINT(" < Lock status %d (%d)\n", dsu_sockfd->port, (int) getpid());
					sem_wait(dsu_sockfd->status_sem);
					
					DSU_DEBUG_PRINT(" - DSU_TRANSFER %d (%d)\n", dsu_sockfd->port, (int) getpid());
					/* Possible multiple processes respond to requests. */
					if (dsu_sockfd->transfer == 0) {++dsu_sockfd->status[DSU_TRANSFER]; dsu_sockfd->transfer = 1;}
					
					DSU_DEBUG_PRINT(" > Unlock status %d (%d)\n", dsu_sockfd->port, (int) getpid());
					sem_post(dsu_sockfd->status_sem);
					
				}

				
		        DSU_DEBUG_PRINT(" - Send file descriptors on %d (%d)\n", comfds->fd, (int) getpid());
		        dsu_write_fd(comfds->fd, dsu_sockfd->shadowfd, port); // handle return value;
				dsu_write_fd(comfds->fd, dsu_sockfd->comfd, port);
				
				
		    }
		    
		    dsu_next_comfd:
		        comfds = comfds->next;
		            
		}
	}
}


void dsu_pre_poll(struct pollfd *fds, nfds_t *nfds) {
    

	for(int i = 0; i < *nfds; i++) {

		
		struct dsu_socket_list *dsu_sockfd = dsu_sockets_search_fd(dsu_program_state.binds, fds[i].fd);
		if (dsu_sockfd == NULL) continue;
		

		/*	Deactivate socket. */
		fds[i].fd = -1;
		fds[i].revents = 0;
		

		if (dsu_sockfd->monitoring) { 
			

			DSU_DEBUG_PRINT(" < Lock status %d (%d)\n", dsu_sockfd->port, (int) getpid());
			if (sem_wait(dsu_sockfd->status_sem) == 0) {
                

                /*  Only one process can monitor a blocking socket. During transfer use lock. */
				if (dsu_sockfd->status[DSU_TRANSFER] > 0 && dsu_sockfd->blocking) {
					
                    
					DSU_DEBUG_PRINT(" - Try lock fd %d (%d)\n", dsu_sockfd->port, (int) getpid());
					if (sem_trywait(dsu_sockfd->fd_sem) == 0) {
						

						DSU_DEBUG_PRINT(" < Lock fd %d (%d)\n", dsu_sockfd->port, (int) getpid());
						dsu_sockfd->locked = DSU_LOCKED;


						DSU_DEBUG_PRINT(" - Set %d => %d (%d)\n", dsu_sockfd->fd, dsu_sockfd->shadowfd, (int) getpid());
						fds[i].fd = dsu_sockfd->shadowfd;

					}

				} else {

					
					DSU_DEBUG_PRINT(" - Set %d => %d (%d)\n", dsu_sockfd->fd, dsu_sockfd->shadowfd, (int) getpid());
					fds[i].fd = dsu_sockfd->shadowfd;

				}

							

				DSU_DEBUG_PRINT(" > Unlock status %d (%d)\n", dsu_sockfd->port, (int) getpid());
				sem_post(dsu_sockfd->status_sem);
			}
		}
    }


	/* 	Remove the file descriptors set to -1 from the list. */
	dsu_compress_fds(fds, nfds);

}


void dsu_unlock(struct dsu_socket_list *dsu_sockfd) {
	

	if (dsu_sockfd->locked == DSU_LOCKED) {
        DSU_DEBUG_PRINT(" > Unlock fd %d (%d)\n", dsu_sockfd->port, (int) getpid());
        sem_post(dsu_sockfd->fd_sem);
        dsu_sockfd->locked = DSU_UNLOCKED;
    }

	
}


void dsu_set_revents(struct pollfd *fds, struct pollfd *_fds, nfds_t _nfds) {
	/*  Change shadow file descripter to its original file descriptor. Use that all internal
		file descriptors are > nfds. */

	int o = 0;
	for(int i = 0; i < _nfds; i++) {
		
		/*	Use that the compression shifts the list left. And every (original) file descriptor must exist in fds list. */
		if (_fds[i].revents != 0) {
			int fd = dsu_originalfd(_fds[i].fd);
			while ( fd != fds[i+o].fd) ++o;
		
			
			fds[i+o].revents = 1;
		}
				
	}
			
}


void dsu_post_poll(struct pollfd *fds, struct pollfd *_fds, nfds_t _nfds) {
	
	
	/*	Unlock binded file descriptors. */
	dsu_forall_sockets(dsu_program_state.binds, dsu_unlock);   
	

	/*	Set revent in original list. */
	dsu_set_revents(fds, _fds, _nfds);
	
        
}


int ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *timeout_ts, const sigset_t *sigmask) {
	
	DSU_DEBUG_PRINT("PPoll() (%d)\n", (int) getpid());
    /* 	Poll() performs a similar task to select(2): it waits for one of a set of file descriptors to become ready to perform I/O. The set 
		of file descriptors to be monitored is specified in the fds argument, which is an array of structures of the following form:
           struct pollfd {
               int   fd;         file descriptor
               short events;     requested events
               short revents;    returned events
           };
       	The caller should specify the number of items in the fds array in nfds. */
	

	#if DSU_DEBUG == 1
	for(int i = 0; i < nfds; i++)
		DSU_DEBUG_PRINT(" - Listening user: %d (%d)\n", fds[i].fd, (int) getpid());
	#endif


	/* 	On first call to poll, mark worker active and configure binded sockets. */
	if (!dsu_program_state.live) {	
		
		dsu_activate_process();
		dsu_configure_process();
		
		/*	Process is initialized. */
		dsu_program_state.live = 1;
	}
	
	
	/* 	State check on binded sockets. */
	dsu_forall_sockets(dsu_program_state.binds, dsu_monitor_fd);


	if (dsu_termination_detection()) {
		dsu_terminate();
	}


	/*	Create shadow file descriptor list that can be modified. */
	nfds_max = FD_SETSIZE;
	nfds_t _nfds = nfds;
	struct pollfd _fds[nfds_max]; 
	memset(_fds, 0, sizeof(_fds));
	memcpy(_fds, fds, sizeof(struct pollfd) * nfds);
	

    correction = 0;


    /*  Convert to shadow file descriptors, this must be done for binded sockets. */
	dsu_pre_poll(_fds, &_nfds);
    
    
    /*  Sniff on internal communication.  */
    dsu_forall_sockets(dsu_program_state.binds, dsu_sniff_conn_poll, _fds, &_nfds);


	#if DSU_DEBUG == 1
	for(int i = 0; i < _nfds; i++) {
			DSU_DEBUG_PRINT(" - Listening: %d %d(%d)\n", _fds[i].fd, _fds[i].events & POLLIN, (int) getpid());
	}
	#endif


    int result = dsu_ppoll(_fds, _nfds, timeout_ts, sigmask);
    if (result <= 0) {
		return result;
	}
	
    
	#if DSU_DEBUG == 1
	for(int i = 0; i < _nfds; i++) {
		if (_fds[i].revents != 0)
			DSU_DEBUG_PRINT(" - Incomming: %d (%d)\n", _fds[i].fd, (int) getpid());
	}
	#endif
    
    
    /*  Handle messages of new processes. */ 
    dsu_forall_sockets(dsu_program_state.binds, dsu_handle_conn_poll, _fds, nfds, _nfds);
    

    /*  Convert shadow file descriptors back to user level file descriptors to avoid changing the external behaviour. */
   	dsu_post_poll(fds, _fds, _nfds);
	

	#if DSU_DEBUG == 1
	for(int i = 0; i < nfds; i++) {
		if (fds[i].revents != 0)
			DSU_DEBUG_PRINT(" - Incomming user: %d (%d)\n", fds[i].fd, (int) getpid());
	}
	#endif
	
	
	return result - correction;
}


int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
	DSU_DEBUG_PRINT("Poll() (%d)\n", (int) getpid());

	struct timespec timeout_ts;
  	struct timespec *timeout_ts_p = NULL;

	if (timeout >= 0) {
		  timeout_ts.tv_sec = timeout / 1000;
		  timeout_ts.tv_nsec = (timeout % 1000) * 1000000;
		  timeout_ts_p = &timeout_ts;
	}
	
	return ppoll(fds, nfds, timeout_ts_p, NULL);
	
}


/*	Newer libc versions. */
int __poll_chk(struct pollfd *fds, nfds_t nfds, int timeout, size_t fdslen) {
	DSU_DEBUG_PRINT("__poll_chk() (%d)\n", (int) getpid());
	return poll(fds, nfds, timeout);
}


int __ppoll_chk (struct pollfd *fds, nfds_t nfds, const struct timespec *timeout, const __sigset_t *ss, __SIZE_TYPE__ fdslen) {
	DSU_DEBUG_PRINT("__ppoll_chk() (%d)\n", (int) getpid());
	return ppoll(fds, nfds, timeout, ss);
}
