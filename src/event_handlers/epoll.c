#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#include "../core.h"
#include "../wrapper.h"
#include "../state.h"
#include "../communication.h"


int (*dsu_epoll_wait)(int, struct epoll_event *, int, int);
int (*dsu_epoll_pwait)(int, struct epoll_event *, int, int, const sigset_t *);
int (*dsu_epoll_create1)(int);
int (*dsu_epoll_create)(int);
int (*dsu_epoll_ctl)(int, int, int, struct epoll_event *);


int dsu_transfer_epoll = 0;


int epoll_create1(int flags) {
	DSU_DEBUG_PRINT(" Epoll_create1() (%d)\n", (int) getpid());
	return dsu_epoll_create1(flags);
}
	

int epoll_create(int size) {
	DSU_DEBUG_PRINT(" Epoll_create() (%d)\n", (int) getpid());
	return dsu_epoll_create(size);
}


int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
	DSU_DEBUG_PRINT(" Epoll_ctl(%d, %d, %d, event) (%d)\n", epfd, op, fd, (int) getpid());
	/* 	This system call is used to add, modify, or remove entries in the interest list. Because this list is not accessible from user 
		space, the event must be stored for binded sockets. It wil be used in the dsu_post_epoll call to restore the original event 
		settings. */

	struct dsu_socket_list *dsu_sockfd = dsu_sockets_search_fd(dsu_program_state.binds, fd);
	

	if (dsu_sockfd == NULL)	return dsu_epoll_ctl(epfd, op, fd, event);
	
	
	if (op == EPOLL_CTL_ADD || op == EPOLL_CTL_MOD) {
		DSU_DEBUG_PRINT(" - Store (%d)\n", (int) getpid());
		memcpy(&dsu_sockfd->ev, event, sizeof(struct epoll_event));
	} else {
		DSU_DEBUG_PRINT(" - Delete (%d)\n", (int) getpid());
		memset(&dsu_sockfd->ev, 0, sizeof(struct epoll_event));
	}


	return dsu_epoll_ctl(epfd, op, dsu_sockfd->shadowfd, event);
}




void dsu_epoll_internal_conn(struct dsu_socket_list *dsu_sockfd, int epollfd) {
    /*  Listen under the hood to accepted connections on public socket. A new generation can request
        file descriptors. */
    
	struct epoll_event ev;
	ev.events = EPOLLIN;


	if (dsu_sockfd->monitoring) {
		
		DSU_DEBUG_PRINT(" - Add %d (%d)\n", dsu_sockfd->comfd, (int) getpid());
        ev.data.fd = dsu_sockfd->comfd;
		dsu_epoll_ctl(epollfd, EPOLL_CTL_ADD, dsu_sockfd->comfd, &ev); // TO DO error

	}
	

	/* Contains zero or more accepted connections. */
	struct dsu_fd_list *comfds = dsu_sockfd->comfds;   
	
	while (comfds != NULL) {

	    DSU_DEBUG_PRINT(" - Add %d (%d)\n", comfds->fd, (int) getpid());
        ev.data.fd = comfds->fd;
		dsu_epoll_ctl(epollfd, EPOLL_CTL_ADD, comfds->fd, &ev); // TO DO error

		
	    comfds = comfds->next;        
	}

	
}


void dsu_handle_conn_epoll(struct dsu_socket_list *dsu_sockfd, int epollfd, int nfds, struct epoll_event *events) {
	/*	Handle internal messages, this includes accepting internal connections and respond to file descriptor requests. */

	for(int i = 0; i < nfds; i++) {


		if (events[i].events == 0) continue;
				

		/* 	Accept connection requests. */
		if (events[i].data.fd == dsu_sockfd->comfd) {

			
			dsu_accept_internal_connection(dsu_sockfd);

			
			/* Remove from the event list, mark them -1. */
			events[i].data.fd = -1;

		
		}
	

		/* 	Respond to messages. */
		struct dsu_fd_list *comfds =   dsu_sockfd->comfds;
		while (comfds != NULL) {

	
			if (events[i].data.fd == comfds->fd) {
				
		        
		        comfds = dsu_respond_internal_connection(dsu_sockfd, comfds);


				/* Remove from the event list, mark them -1. */
				events[i].data.fd = -1;
				
				
		    } else {
		    

		    	comfds = comfds->next;

			}        
		}
	}
}


void dsu_epoll_shadowfd(struct dsu_socket_list *dsu_sockfd, int epollfd) {
	

	/*	Deactivate socket. */
	if (dsu_sockfd->ev.events != 0) {
		DSU_DEBUG_PRINT(" - remove %d (%d)\n", dsu_sockfd->fd, (int) getpid());
		dsu_epoll_ctl(epollfd, EPOLL_CTL_DEL, dsu_sockfd->fd, NULL);
	}
	

	/* 	Set shadow file descriptor if needed... */
	struct epoll_event ev;
	memcpy(&ev, &dsu_sockfd->ev, sizeof(struct epoll_event));
    ev.data.fd = dsu_sockfd->shadowfd; // Overwrite union ptr, fd, uint32, uint64 to shadow file descriptor.
	

	if (dsu_sockfd->monitoring && dsu_sockfd->ev.events != 0) { 
		

		DSU_DEBUG_PRINT(" < Lock status %d (%d)\n", dsu_sockfd->port, (int) getpid());
		if (sem_wait(dsu_sockfd->status_sem) == 0) {

            
			if (dsu_sockfd->status[DSU_TRANSFER] > 0) dsu_transfer_epoll = 1;


            /*  Only one process can monitor a blocking socket. During transfer use lock. */
			if (dsu_sockfd->status[DSU_TRANSFER] > 0 && dsu_sockfd->blocking) {
				
                
				DSU_DEBUG_PRINT(" - Try lock fd %d (%d)\n", dsu_sockfd->port, (int) getpid());
				if (sem_trywait(dsu_sockfd->fd_sem) == 0) {
					

					DSU_DEBUG_PRINT(" < Lock fd %d (%d)\n", dsu_sockfd->port, (int) getpid());
					dsu_sockfd->locked = DSU_LOCKED;


					DSU_DEBUG_PRINT(" - Set %d => %d (%d)\n", dsu_sockfd->fd, dsu_sockfd->shadowfd, (int) getpid());
					dsu_epoll_ctl(epollfd, EPOLL_CTL_ADD, dsu_sockfd->shadowfd, &ev);

				}

			} else {

				
				DSU_DEBUG_PRINT(" - Set %d => %d (%d)\n", dsu_sockfd->fd, dsu_sockfd->shadowfd, (int) getpid());
				dsu_epoll_ctl(epollfd, EPOLL_CTL_ADD, dsu_sockfd->shadowfd, &ev);

			}

						

			DSU_DEBUG_PRINT(" > Unlock status %d (%d)\n", dsu_sockfd->port, (int) getpid());
			sem_post(dsu_sockfd->status_sem);
		}
	}	
}


void dsu_pre_epoll(int epollfd) {

	
	/*	Set shadow file descriptors. */
	dsu_forall_sockets(dsu_program_state.binds, dsu_epoll_shadowfd, epollfd);


	/*	Sniff on internal connections. */
	dsu_forall_sockets(dsu_program_state.binds, dsu_epoll_internal_conn, epollfd);

	
}


void dsu_epoll_unlock(struct dsu_socket_list *dsu_sockfd) {


	if (dsu_sockfd->locked == DSU_LOCKED) {
        DSU_DEBUG_PRINT(" > Unlock fd %d (%d)\n", dsu_sockfd->port, (int) getpid());
        sem_post(dsu_sockfd->fd_sem);
        dsu_sockfd->locked = DSU_UNLOCKED;
    }

	
}


void dsu_epoll_originalfd(struct dsu_socket_list *dsu_sockfd, int epollfd) {

	/*	To avoid changing the external behaviour, restore the epoll settings. */
	if (dsu_sockfd->ev.events != 0) {
		struct epoll_event ev;
		memcpy(&ev, &dsu_sockfd->ev, sizeof(struct epoll_event));

		DSU_DEBUG_PRINT(" - %d => %d (%d)\n", dsu_sockfd->shadowfd, dsu_sockfd->fd, (int) getpid());
		dsu_epoll_ctl(epollfd, EPOLL_CTL_DEL, dsu_sockfd->shadowfd, NULL);
		dsu_epoll_ctl(epollfd, EPOLL_CTL_ADD, dsu_sockfd->fd, &ev);
	}

}


int dsu_epoll_correct_events(int nfds, struct epoll_event *events) {
	
	
	for(int i = 0; i < nfds; i++) {
		

		/*	Remove internal connections from the event list. These are marked -1.*/
		if (events[i].data.fd == -1) {
			
			for(int j = i; j < nfds-1; j++) {
				events[j].events = events[j+1].events;
				events[j].data = events[j+1].data;
			}

			i--;
			nfds--;

		} 

		
		/*	Change shadow file descripter back to the original file descriptor. Because struct epoll_event is an union of a pointer, 
			file descriptor or an array index we have to use the stored struct epoll_event (See epoll_ctl). */
		else {

			struct dsu_socket_list *dsu_sockfd = dsu_sockets_search_shadowfd(dsu_program_state.binds, events[i].data.fd);
			if (dsu_sockfd != NULL && dsu_sockfd->ev.events != 0) {
				
				uint32_t _events = events[i].events;
				memcpy(&events[i], &dsu_sockfd->ev, sizeof(struct epoll_event));
				events[i].events = _events; // Overwrite the initial event setting with the response of epoll_wait.
			
			}
		}

		
	}
	
	return nfds;
}


int dsu_post_epoll(int epollfd, int nfds, struct epoll_event *events) {


	/*  Handle internal messages. */ 
    dsu_forall_sockets(dsu_program_state.binds, dsu_handle_conn_epoll, epollfd, nfds, events);

	
	/*	Unlock binded file descriptors if needed. */
	dsu_forall_sockets(dsu_program_state.binds, dsu_epoll_unlock);
	
	
	/*  Change shadow file descripter to its original file descriptor in epoll. */
	dsu_forall_sockets(dsu_program_state.binds, dsu_epoll_originalfd, epollfd);
	

	/* 	Correct event list by restoring original epoll_event data. */
	return dsu_epoll_correct_events(nfds, events);
			
}


int fepoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask) {


	DSU_INITIALIZE_EVENT;

	
	dsu_transfer_epoll = 0;

	
	/*	Prepare for epoll. */
	dsu_pre_epoll(epfd);

	
	int nfds = dsu_epoll_pwait(epfd, events, maxevents, timeout, sigmask);


	/*	Translate epoll response. */
	nfds = dsu_post_epoll(epfd, nfds, events);
	

	return nfds;
}


int repoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask) {
	DSU_DEBUG_PRINT("repoll_pwait(%d, events, %d, %d, sigmask) (%d)\n", epfd, maxevents, timeout, (int) getpid());

	int time = timeout;
	if(dsu_transfer_epoll == 1) time = 500;

	int v = 0;	
	while ( v == 0 ) {
		v = fepoll_pwait(epfd, events, maxevents, time, sigmask); // 500 ms
	}

	return v;
	 

}


int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask) {
	DSU_DEBUG_PRINT("epoll_pwait() (%d)\n", (int) getpid());
	/* 	The epoll_wait() system call waits for events on the epoll(7) instance referred to by the file descriptor epfd.  The buffer
		pointed to by events is used to return information from the ready list about file descriptors in the interest list that have some
       	events available.  Up to maxevents are returned by epoll_wait(). The maxevents argument must be greater than zero. */
	

	/*  To support non-blocking sockets, narrow down the time between locks. Do this in while loop, not recursively due to maximum
		recursive dept. */
	if (timeout < 0) {
		repoll_pwait(epfd, events, maxevents, timeout, sigmask);
	}

	return fepoll_pwait(epfd, events, maxevents, timeout, sigmask);

}


int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
	DSU_DEBUG_PRINT("epoll_wait() (%d)\n", (int) getpid());
	return epoll_pwait(epfd, events, maxevents, timeout, NULL);
}


int epoll_pwait2(int epfd, struct epoll_event *events, int maxevents, const struct timespec *timeout, const sigset_t *sigmask) {
	DSU_DEBUG_PRINT("epoll_pwait2() (%d)\n", (int) getpid());
	
	
	/* 	Convert time to miliseconds. */
	int ts = -1; // Indefinite.
	if (timeout != NULL) {
		ts = timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000;
	}
	

	return epoll_pwait(epfd, events, maxevents, ts, NULL);
}





