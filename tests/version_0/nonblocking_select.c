#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <fcntl.h>


#define PORT    3000
#define PORT2   3001
#define MAXMSG  512


int main (int argc, char **argv) {
    
	/* Create the socket. */
	struct sockaddr_in name;
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror ("Error creating socket");
		exit (EXIT_FAILURE);
	}

	int flags = fcntl(sock, F_GETFL, 0) | O_NONBLOCK;
	fcntl(sock, F_SETFL, (char *) &flags);

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

	/* -----------Test close function.------------ */
	/* Create another the socket. */
	struct sockaddr_in name2;
	int sock2 = socket(PF_INET, SOCK_STREAM, 0);
	if (sock2 < 0) {
		perror ("Error creating socket");
		exit (EXIT_FAILURE);
	}
	/* Bind socket. */
	name2.sin_family = AF_INET;
	name2.sin_port = htons(PORT2);
	name2.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sock2, (struct sockaddr *) &name2, sizeof(name2)) < 0) {
		perror("Error binding");
		exit (EXIT_FAILURE);
	}

	/* Listen on socket. */
	if (listen(sock2, 1) < 0)
    {
      perror("Error start listening on socket");
      exit(EXIT_FAILURE);
    }

	close(sock2);
	/* ----------------------------------------- */

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
					if (new > 0) {
						FD_SET(new, &active_fd_set);
					}
				} else {
					/* Read message. */
					char buffer[MAXMSG];
  					int nbytes;
					
  					nbytes = read(i, buffer, MAXMSG);
  					if (nbytes == 0) {
						/* Close connection. */
					    close(i);
	                    FD_CLR(i, &active_fd_set);
					} else if (nbytes > 0) {
	  					/* Write response. */
						char response[25] = "Hello, this is version 1\0";
						if ( write(i, response, sizeof(response)-1) < 0) {
							/* Read error. */
							perror("Error writing message");
							exit(EXIT_FAILURE);
						}
					}				
	          	}
	      	}
		}
    }
}

