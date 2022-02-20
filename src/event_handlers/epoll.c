#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>

#include <netinet/in.h>
#include <netinet/ip.h>

#include "../core.h"
#include "../state.h"
#include "../communication.h"


int (*dsu_epoll_wait)(int, struct epoll_event *, int, int);
int (*dsu_epoll_create1)(int);
int (*dsu_epoll_create)(int);


int dsu_live_epoll = 0;



int epoll_create1(int flags) {
	DSU_DEBUG_PRINT(" Epoll_create1() (%d)\n", (int) getpid());
	return dsu_epoll_create1(flags);
}
	

int epoll_create(int size) {
	DSU_DEBUG_PRINT(" Epoll_create() (%d)\n", (int) getpid());
	return dsu_epoll_create(size);
}


void dsu_epoll_internal_conn(struct dsu_socket_list *dsu_sockfd, int epollfd) {
    /*  Listen under the hood to accepted connections on public socket. A new generation can request
        file descriptors. */

	
	/* Mark ready to be listening. */
	if (dsu_sockfd->comfd_close > 0) {
		DSU_DEBUG_PRINT(" - port %d is ready in on the side of the new version\n", dsu_sockfd->port);
        DSU_DEBUG_PRINT(" - send ready %d\n", dsu_sockfd->comfd_close);
        int buf = 1;
        if (send(dsu_sockfd->comfd_close, &buf, sizeof(int), MSG_CONFIRM) != -1) {
            DSU_DEBUG_PRINT(" - close %d\n", dsu_sockfd->comfd_close);
		    dsu_close(dsu_sockfd->comfd_close);
		    dsu_sockfd->comfd_close = -1;
        }
	}
	
    
	struct epoll_event ev;
	ev.events = EPOLLIN;
	

	DSU_DEBUG_PRINT(" - Add %d (%d)\n", dsu_sockfd->comfd, (int) getpid());
    ev.data.fd = dsu_sockfd->comfd;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, dsu_sockfd->comfd, &ev); // TO DO handle error


    if (!dsu_sockfd->ready) {
		DSU_DEBUG_PRINT(" - Add %d (%d)\n", dsu_sockfd->readyfd, (int) getpid());
    	ev.data.fd = dsu_sockfd->readyfd;
		epoll_ctl(epollfd, EPOLL_CTL_ADD, dsu_sockfd->readyfd, &ev); // TO DO handle error
    }
	

	/* Contains zero or more accepted connections. */
	struct dsu_fd_list *comfds = dsu_sockfd->comfds;
	while (comfds != NULL) {

		
	    DSU_DEBUG_PRINT(" - Add %d (%d)\n", comfds->fd, (int) getpid());
        ev.data.fd = comfds->fd;
		epoll_ctl(epollfd, EPOLL_CTL_ADD, comfds->fd, &ev); // TO DO handle error

		
	    comfds = comfds->next;        
	}


}


void dsu_handle_conn_epoll(struct dsu_socket_list *dsu_sockfd, int epollfd, int nfds, struct epoll_event *events) {
	/*	Handle internal messages, this includes accepting internal connections and respond to file descriptor requests. */

	for(int i = 0; i < nfds; i++) {


		if (events[i].events == 0) continue;
				

		/* 	Accept connection requests. */
		if (events[i].data.fd == dsu_sockfd->comfd) {


			/*  Race conditions could happend when a "late" fork is performed. The fork happend after 
        		accepting dsu communication connection. */
			int size = sizeof(dsu_sockfd->comfd_addr);
			int acc = dsu_accept4(dsu_sockfd->comfd, (struct sockaddr *) &dsu_sockfd->comfd_addr, (socklen_t *) &size, SOCK_NONBLOCK);
			

			if ( acc != -1) {
				DSU_DEBUG_PRINT("  - Accept %d on %d (%d)\n", acc, dsu_sockfd->comfd, (int) getpid());
				dsu_socket_add_fds(dsu_sockfd, acc, DSU_INTERNAL_FD);
			} else
				DSU_DEBUG_PRINT("  - Accept failed (%d)\n", (int) getpid());


			/* Remove */
			events[i].data.fd = -1;
			epoll_ctl(epollfd, EPOLL_CTL_DEL, dsu_sockfd->readyfd, NULL);


			continue;
		
		}

	
		/*  Port is ready in the new version. */
		if (events[i].data.fd == dsu_sockfd->readyfd) {
			dsu_sockfd->ready = 1;

			/* Remove */
			events[i].data.fd = -1;
			epoll_ctl(epollfd, EPOLL_CTL_DEL, dsu_sockfd->readyfd, NULL);			
		}
	

		/* 	Respond to messages. */
		struct dsu_fd_list *comfds =   dsu_sockfd->comfds;
		while (comfds != NULL) {

	
			if (events[i].data.fd == comfds->fd) {

		
				/* Remove */
				events[i].data.fd = -1;
				epoll_ctl(epollfd, EPOLL_CTL_DEL, comfds->fd, NULL);	
				
		        
		        int buffer = 0;
		        int port = dsu_sockfd->port;
		        
		        
		        /*  Race condition on recv, hence do not wait, and continue on error. */
		        int r = recv(comfds->fd, &buffer, sizeof(buffer), MSG_DONTWAIT);
		        
		        
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

				
				if (buffer == 1) {
                

		            /* Mark file descriptor as transferred. */
					DSU_DEBUG_PRINT(" - port %d is ready in the new version\n", dsu_sockfd->port);
					const char *buf = "ready";
					if (send(dsu_sockfd->markreadyfd, &buf, 5, MSG_CONFIRM) < 0) {
						DSU_DEBUG_PRINT(" - send to readyfd %d failed\n", dsu_sockfd->markreadyfd);
					}
					

		            dsu_close(comfds->fd);


		            struct dsu_fd_list *_comfds = comfds;
		            comfds = comfds->next;
		            dsu_socket_remove_fds(dsu_sockfd, _comfds->fd, DSU_INTERNAL_FD);


					continue;
            
		        } else {
		       
		            DSU_DEBUG_PRINT(" - Send file descriptors %d & %d on %d (%d)\n", dsu_sockfd->fd, dsu_sockfd->comfd, comfds->fd, (int) getpid());
		            dsu_write_fd(comfds->fd, dsu_sockfd->fd, port); // handle return value;
					dsu_write_fd(comfds->fd, dsu_sockfd->comfd, port);

		        }

				
		    }
		    
		    dsu_next_comfd:
		        comfds = comfds->next;
		            
		}
	}
}


void dsu_pre_epoll(int epollfd) {

	
	/*	Sniff on internal connections. */
	dsu_forall_sockets(dsu_program_state.binds, dsu_epoll_internal_conn, epollfd);
		
	
}


int dsu_epoll_correct_events(int nfds, struct epoll_event *events) {
	
	
	for(int i = 0; i < nfds; i++) {
		

		/*	Remove internal connections from the event list. */
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


void dsu_post_epoll_fd(struct dsu_socket_list *dsu_sockfd, int epollfd, int nfds, struct epoll_event *events) {
		
	for(int i = 0; i < nfds; i++) {

	
		if (events[i].events == 0) continue;

		
		if (events[i].data.fd == dsu_sockfd->fd) {
			
			if (dsu_sockfd->ready) {
				DSU_DEBUG_PRINT(" - remove public  %d (%d)\n", dsu_sockfd->fd, (int) getpid());
				events[i].data.fd = -1; // Mark to be removed.
				epoll_ctl(epollfd, EPOLL_CTL_DEL, dsu_sockfd->fd, NULL);
			}

		}


	}
	
}



int dsu_post_epoll(int epollfd, int nfds, struct epoll_event *events) {


	/*  Handle internal messages. */ 
    dsu_forall_sockets(dsu_program_state.binds, dsu_handle_conn_epoll, epollfd, nfds, events);
	
	
	/*  Change shadow file descripter to its original file descriptor in epoll. */
	dsu_forall_sockets(dsu_program_state.binds, dsu_post_epoll_fd, epollfd, nfds, events);
	

	/* 	Correct event list by restoring original epoll_event data. */
	return dsu_epoll_correct_events(nfds, events);
			
}


int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {
	/* 	The epoll_wait() system call waits for events on the epoll(7) instance referred to by the file descriptor epfd.  The buffer
		pointed to by events is used to return information from the ready list about file descriptors in the interest list that have some
       	events available.  Up to maxevents are returned by epoll_wait(). The maxevents argument must be greater than zero. */
	DSU_DEBUG_PRINT(" Epoll_wait() (%d)\n", (int) getpid());


	/* 	On first call to select, mark worker active and configure binded sockets. */
	if (!dsu_live_epoll) {
		if (dsu_activate_process() > 0) dsu_live_epoll = 1;
    }
	

	if (dsu_termination_detection()) {
		dsu_terminate();
	}


	/*	Prepare for epoll. */
	dsu_pre_epoll(epfd);


	int nfds = dsu_epoll_wait(epfd, events, maxevents, timeout);


	/*	Translate epoll response. */
	nfds = dsu_post_epoll(epfd, nfds, events);
	
	
	/* Avoid changing external behaviour. Most applications cannot handle return value of 0, hence call select recursively. */
	if (nfds == 0 && timeout < 0) {
		return epoll_wait(epfd, events, maxevents, timeout);
	}
	

	return nfds;
}
