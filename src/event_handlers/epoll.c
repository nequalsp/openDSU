#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/file.h>


#include "../core.h"
#include "../state.h"
#include "../communication.h"


int (*dsu_epoll_wait)(int, struct epoll_event *, int, int);
int (*dsu_epoll_pwait)(int, struct epoll_event *, int, int, const sigset_t *);
int (*dsu_epoll_pwait2)(int, struct epoll_event *, int, const struct timespec *, const sigset_t *);
int (*dsu_epoll_ctl)(int, int, int, struct epoll_event *);


int dsu_epoll_transfer = 0;
struct dsu_fd_list *dsu_epoll_fds = NULL;


void dsu_epoll_sniff_conn(struct dsu_socket_list *dsu_sockfd, int epollfd) {
    

	if (dsu_sockfd->monitoring) {
		DSU_DEBUG_PRINT(" - Add %d (%d)\n", dsu_sockfd->comfd, (int) getpid());


		struct epoll_event ev;
        ev.data.fd = dsu_sockfd->comfd;
		ev.events = EPOLLIN;


		dsu_epoll_ctl(epollfd, EPOLL_CTL_ADD, dsu_sockfd->comfd, &ev); // TO DO handle error.
	}


}


void dsu_epoll_handle_conn(struct dsu_socket_list *dsu_sockfd, int epollfd, int nfds, struct epoll_event *events) {


	for(int i = 0; i < nfds; i++) {


		if (events[i].events == 0) continue;
				

		if (events[i].data.fd == dsu_sockfd->comfd) {


			dsu_send_fd(dsu_sockfd);
			

			events[i].data.fd = -1; // Mark to be removed.
		}


	}
}


void dsu_pre_epoll_fd(struct dsu_socket_list *dsu_sockfd, int epollfd) {
	

	if (dsu_sockfd->monitoring) { 
		

		DSU_DEBUG_PRINT(" < Lock status %d (%d)\n", dsu_sockfd->port, (int) getpid());
		if (sem_wait(dsu_sockfd->status_sem) == 0) {

            
			if (dsu_sockfd->status[DSU_TRANSFER] > 0) dsu_epoll_transfer = 1;


            /*  Only one process can monitor a blocking socket. During transfer use lock. */
			printf("check %d %d\n", dsu_sockfd->status[DSU_TRANSFER], (fcntl(dsu_sockfd->fd, F_GETFL, 0) & O_NONBLOCK));
			if (dsu_sockfd->status[DSU_TRANSFER] > 0 && !(fcntl(dsu_sockfd->fd, F_GETFL, 0) & O_NONBLOCK)) {
				
                
				DSU_DEBUG_PRINT(" - Try lock fd %d (%d)\n", dsu_sockfd->port, (int) getpid());
				if (flock(dsu_sockfd->lock, LOCK_EX | LOCK_NB) == 0) {

					
					DSU_DEBUG_PRINT(" < Lock fd %d (%d)\n", dsu_sockfd->port, (int) getpid());
					dsu_sockfd->locked = DSU_LOCKED;
					

				} else {
					

					DSU_DEBUG_PRINT(" - Remove %d (%d)\n", dsu_sockfd->fd, (int) getpid());
					if( dsu_epoll_ctl(epollfd, EPOLL_CTL_DEL, dsu_sockfd->fd, NULL) == 0) {
						dsu_socket_remove_fds(&dsu_epoll_fds, dsu_sockfd->fd);
					}


				}


			}


			DSU_DEBUG_PRINT(" > Unlock status %d (%d)\n", dsu_sockfd->port, (int) getpid());
			sem_post(dsu_sockfd->status_sem);


		}

 
	} else {


		DSU_DEBUG_PRINT(" - Remove %d (%d)\n", dsu_sockfd->fd, (int) getpid());
		if(dsu_epoll_ctl(epollfd, EPOLL_CTL_DEL, dsu_sockfd->fd, NULL) == 0) {
			dsu_socket_remove_fds(&dsu_epoll_fds, dsu_sockfd->fd);
		}


	}
}


void dsu_pre_epoll(int epollfd) {

	
	dsu_forall_sockets(dsu_program_state.binds, dsu_pre_epoll_fd, epollfd);


	dsu_forall_sockets(dsu_program_state.binds, dsu_epoll_sniff_conn, epollfd);

	
}


void dsu_epoll_unlock(struct dsu_socket_list *dsu_sockfd) {


	if (dsu_sockfd->locked == DSU_LOCKED) {
        DSU_DEBUG_PRINT(" > Unlock fd %d (%d)\n", dsu_sockfd->port, (int) getpid());
		flock(dsu_sockfd->lock, LOCK_UN);
        //sem_post(dsu_sockfd->fd_sem);
        dsu_sockfd->locked = DSU_UNLOCKED;
    }

	
}


void dsu_epoll_post_fd(struct dsu_socket_list *dsu_sockfd, int epollfd) {
	

	struct epoll_event zero;
	memset(&zero, 0, sizeof(struct epoll_event));

	
	if ( memcmp(&dsu_sockfd->ev, &zero, sizeof(struct epoll_event)) != 0) {


		DSU_DEBUG_PRINT(" - Add %d (%d)\n", dsu_sockfd->fd, (int) getpid());
		if( dsu_epoll_ctl(epollfd, EPOLL_CTL_ADD, dsu_sockfd->fd, &dsu_sockfd->ev) == 0) {
			printf("post +1\n");
			dsu_socket_add_fds(&dsu_epoll_fds, dsu_sockfd->fd);
		}


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
	}
	
	return nfds;
}


int dsu_post_epoll(int epollfd, int nfds, struct epoll_event *events) {


	/*  Handle internal messages. */ 
    dsu_forall_sockets(dsu_program_state.binds, dsu_epoll_handle_conn, epollfd, nfds, events);

	
	/*	Return removed file descriptors. */	
	dsu_forall_sockets(dsu_program_state.binds, dsu_epoll_post_fd, epollfd);
	

	/*	Unlock binded file descriptors if needed. */
	dsu_forall_sockets(dsu_program_state.binds, dsu_epoll_unlock);
	

	/* 	Correct event list by restoring original epoll_event data. */
	return dsu_epoll_correct_events(nfds, events);
			
}



int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
	DSU_DEBUG_PRINT(" Epoll_ctl(%d, %d, %d, event) (%d)\n", epfd, op, fd, (int) getpid());
	
	printf("ctl..\n");
	if (dsu_epoll_ctl(epfd, op, fd, event) == 0) {


		struct dsu_socket_list *dsu_sockfd = dsu_sockets_search_fd(dsu_program_state.binds, fd);


		if (op == EPOLL_CTL_ADD) {


			if (dsu_sockfd != NULL) memcpy(&dsu_sockfd->ev, event, sizeof(struct epoll_event));
			

			dsu_socket_add_fds(&dsu_epoll_fds, fd);


		} else if (op == EPOLL_CTL_DEL) {


			if (dsu_sockfd != NULL) memset(&dsu_sockfd->ev, 0, sizeof(struct epoll_event));

			
			dsu_socket_remove_fds(&dsu_epoll_fds, fd);
		

		} else {


			if (dsu_sockfd != NULL) memcpy(&dsu_sockfd->ev, event, sizeof(struct epoll_event));


		}


		return 0;

	}


	return -1;
}


int fepoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask) {


	DSU_PRE_EVENT;

	
	dsu_epoll_transfer = 0;

	
	dsu_pre_epoll(epfd);

	
	if (dsu_epoll_fds == NULL && dsu_epoll_transfer == 0) DSU_DEACTIVATE;	
	

	DSU_TERMINATION;
	
	
	int nfds = dsu_epoll_pwait(epfd, events, maxevents, timeout, sigmask);


	nfds = dsu_post_epoll(epfd, nfds, events);
	

	return nfds;
}


int repoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask) {
	DSU_DEBUG_PRINT("repoll_pwait(%d, events, %d, %d, sigmask) (%d)\n", epfd, maxevents, timeout, (int) getpid());

	int time = timeout;
	if(dsu_epoll_transfer == 1) time = 500;

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





