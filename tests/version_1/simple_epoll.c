#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/epoll.h>


#define PORT    3000
#define MAXMSG  512


int main (int argc, char **argv) {
    
	
	/* Create the socket. */
	struct sockaddr_in name;
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror ("Error creating socket");
		exit (EXIT_FAILURE);
	}

	
	/* Bind socket. */
	name.sin_family = AF_INET;
	name.sin_port = htons(PORT);
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sock, (struct sockaddr *) &name, sizeof(name)) < 0) {
		perror("Error binding");
		exit (EXIT_FAILURE);
	}

	
	/* Listen on socket. */
	if (listen(sock, 1) < 0)
    {
      perror("Error start listening on socket");
      exit(EXIT_FAILURE);
    }

	
	#define MAX_EVENTS 10
	struct sockaddr_in clientname;
   	struct epoll_event ev, events[MAX_EVENTS];
   	int nfds, epollfd;


   epollfd = epoll_create1(0);
   if (epollfd == -1) {
       perror("epoll_create1");
       exit(EXIT_FAILURE);
   }

	
   ev.events = EPOLLIN;
   ev.data.fd = sock;
   if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev) == -1) {
       perror("epoll_ctl: listen_sock");
       exit(EXIT_FAILURE);
   }

   printf("Start listening on port %d...\n", PORT);	
   for (;;) {
		
       nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
       if (nfds == -1) {
           perror("epoll_wait");
           exit(EXIT_FAILURE);
       }

       for (int n = 0; n < nfds; ++n) {

			if (events[n].events == 0) continue;
			
			if (events[n].data.fd == sock) {
				size_t size = sizeof(clientname);
	           	int conn_sock = accept(sock, (struct sockaddr *) &clientname, (socklen_t *) &size);
	           	if (conn_sock == -1) {
	            	perror("accept");
	           		exit(EXIT_FAILURE);
	           	}
	           	ev.events = EPOLLIN;
	           	ev.data.fd = conn_sock;
	           	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
	            	perror("epoll_ctl: conn_sock");
	            	exit(EXIT_FAILURE);
	           	}
			} else {
				/* Read message. */
				char buffer[MAXMSG];
				int nbytes;
				
				nbytes = read(events[n].data.fd, buffer, MAXMSG);
				if (nbytes < 0) {
					/* Read error. */
					perror("Error reading message");
					exit(EXIT_FAILURE);
				} else if (nbytes == 0) {
					/* Close connection. */
					close(events[n].data.fd);
					epoll_ctl(epollfd, EPOLL_CTL_DEL, events[n].data.fd, NULL);
				} else {
					/* Write response. */
					char response[25] = "Hello, this is version 2\0";
					if ( write(events[n].data.fd, response, sizeof(response)-1) < 0) {
						/* Read error. */
						perror("Error writing message");
						exit(EXIT_FAILURE);
					}
				}
			}
		}
	}
}	


              
              

              


































		
		

