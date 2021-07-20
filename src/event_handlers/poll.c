#define _GNU_SOURCE

#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <sys/poll.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#include "../core.h"
#include "../wrapper.h"
#include "../state.h"
#include "../communication.h"


int (*dsu_poll)(struct pollfd *, nfds_t, int);
int (*dsu_ppoll)(struct pollfd *, nfds_t, const struct timespec *, const sigset_t *);

int dsu_nfds_max;
int dsu_correction;
int dsu_transfer;


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
		if (*nfds < dsu_nfds_max) {
			fds[*nfds].fd = dsu_sockfd->comfd;
			fds[*nfds].events = POLLIN;
			fds[(*nfds)++].revents = 0;
		}

	}
	
    
	/* Contains zero or more accepted connections. */
	struct dsu_fd_list *comfds = dsu_sockfd->comfds;   
	
	while (comfds != NULL) {

	    DSU_DEBUG_PRINT(" - Add %d (%d)\n", comfds->fd, (int) getpid());
		if (*nfds < dsu_nfds_max) { // Avoid buffer overflow.
			fds[*nfds].fd = comfds->fd;	
			fds[*nfds].events = POLLIN;
			fds[(*nfds)++].revents = 0;
		}
		
	    comfds = comfds->next;        
	}
	
}


void dsu_handle_conn_poll(struct dsu_socket_list *dsu_sockfd, struct pollfd *fds, nfds_t start, nfds_t end) {
	DSU_DEBUG_PRINT(" - Handle on %d (%d)\n", dsu_sockfd->port, (int) getpid());

	
	for(int i = start; i < end; i++) {


		if (fds[i].revents == 0) continue;

		
		++dsu_correction;
		

		/* 	Accept connection requests. */
		if (fds[i].fd == dsu_sockfd->comfd) {


			dsu_accept_internal_connection(dsu_sockfd);
		
		}
	
	
		/* 	Respond to messages. */
		struct dsu_fd_list *comfds =   dsu_sockfd->comfds;
		while (comfds != NULL) {

	
			if (fds[i].fd == comfds->fd) {
				
		        
		        comfds = dsu_respond_internal_connection(dsu_sockfd, comfds);
				
				
		    } else {

				comfds = comfds->next;


			}         
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
                

				if (dsu_sockfd->status[DSU_TRANSFER] > 0) dsu_transfer = 1;


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


void dsu_reset_revents(struct pollfd *fds, nfds_t nfds) {

	for(int i = 0; i < nfds; i++) {
		fds[i].revents = 0;
	}

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
	

	#ifdef DEBUG
	for(int i = 0; i < nfds; i++)
		DSU_DEBUG_PRINT(" - Listening user: %d (%d)\n", fds[i].fd, (int) getpid());
	#endif


	DSU_INITIALIZE_EVENT;


	/*	Create copy file descriptor array that can be modified, cannot extend original array
		because the size is unkown. */
	dsu_nfds_max = FD_SETSIZE;
	nfds_t _nfds = nfds;
	struct pollfd _fds[dsu_nfds_max]; 
	memset(_fds, 0, sizeof(_fds));
	memcpy(_fds, fds, sizeof(struct pollfd) * nfds);
	

	/*	Reset revents in original list. */
	dsu_reset_revents(fds, nfds);


	/* 	Reset correction. */
    dsu_correction = 0;


	dsu_transfer = 0;


    /*  Convert to shadow file descriptors, this must be done for binded sockets. */
	dsu_pre_poll(_fds, &_nfds);
    
    
    /*  Sniff on internal communication.  */
    dsu_forall_sockets(dsu_program_state.binds, dsu_sniff_conn_poll, _fds, &_nfds);

	
	/* To support non-blocking sockets, narrow down the time between locks. */
	const struct timespec ts = {.tv_sec = 0, .tv_nsec = 100000 /* 100 microseconds (usec). */ };
	const struct timespec *pts = timeout_ts;
	if (dsu_transfer == 1 && timeout_ts == NULL) {
		pts = &ts;
	}
	
	
	#ifdef DEBUG
	for(int i = 0; i < _nfds; i++) {
			DSU_DEBUG_PRINT(" - Listening: %d %d(%d)\n", _fds[i].fd, _fds[i].events & POLLIN, (int) getpid());
	}
	#endif


    int result = dsu_ppoll(_fds, _nfds, pts, sigmask);
    if (result <= 0) {
		return result;
	}
	
    
	#ifdef DEBUG
	for(int i = 0; i < _nfds; i++) {
		if (_fds[i].revents != 0)
			DSU_DEBUG_PRINT(" - Incomming: %d (%d)\n", _fds[i].fd, (int) getpid());
	}
	#endif
    
    
    /*  Handle messages of new processes. */ 
    dsu_forall_sockets(dsu_program_state.binds, dsu_handle_conn_poll, _fds, nfds, _nfds);
    

    /*  Convert shadow file descriptors back to user level file descriptors to avoid changing the external behaviour. */
   	dsu_post_poll(fds, _fds, _nfds);
	

	#ifdef DEBUG
	for(int i = 0; i < nfds; i++) {
		if (fds[i].revents != 0)
			DSU_DEBUG_PRINT(" - Incomming user: %d (%d)\n", fds[i].fd, (int) getpid());
	}
	#endif
	
	
	return result - dsu_correction;
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
