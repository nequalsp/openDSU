#include <openDSU.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/un.h>


#define PORT    3000
#define MAXMSG  512


int start(int sock) {
	
	/* Initialize the set of active sockets. */
	fd_set active_fd_set, read_fd_set;
	struct sockaddr_in clientname;
  	FD_ZERO(&active_fd_set);
  	FD_SET(sock, &active_fd_set);

	printf("Start listening on port %d...\n", PORT);
	while (1)
	{
		/* Wait for active socket. */
		read_fd_set = active_fd_set;
		if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0) {
			perror("Error running select");
			exit(EXIT_FAILURE);
		}
		
		/* Handle each active socket. */
		for (int i = 0; i < FD_SETSIZE; ++i) {
			if (FD_ISSET(i, &read_fd_set)) {
				if (i == sock) {
					/* Accept new connection request. */
					size_t size = sizeof(clientname);
					int new = accept(sock, (struct sockaddr *) &clientname, (socklen_t *) &size);
					if (new < 0) {
						perror("Error accepting message");
						exit(EXIT_FAILURE);
					}
	            	FD_SET(new, &active_fd_set);
				} else {
					/* Read message. */
					char buffer[MAXMSG];
  					int nbytes;
					
  					nbytes = read(i, buffer, MAXMSG);
  					if (nbytes < 0) {
						/* Read error. */
						perror("Error reading message");
						exit(EXIT_FAILURE);
					} else if (nbytes == 0) {
						; // Do nothing.
					} else {
	  					/* Write response. */
						char response[25] = "Hello, this is version 1\0";
						if ( write(i, response, sizeof(response)-1) < 0) {
							/* Read error. */
							perror("Error writing message");
							exit(EXIT_FAILURE);
						}
					}
					
					/* Close connection. */
					close(i);
	                FD_CLR(i, &active_fd_set);				
	          	}
	      	}
		}
    }
}

