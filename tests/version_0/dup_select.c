#define _GNU_SOURCE
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


#define PORT1    3000
#define PORT2    3001
#define MAXMSG  512


int main (int argc, char **argv) {
    
	/* Create the socket. */
	struct sockaddr_in name1;
	int sock1 = socket(PF_INET, SOCK_STREAM, 0);
	if (sock1 < 0) {
		perror ("Error creating socket");
		exit (EXIT_FAILURE);
	}

	/* Bind socket. */
	name1.sin_family = AF_INET;
	name1.sin_port = htons(PORT1);
	name1.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sock1, (struct sockaddr *) &name1, sizeof(name1)) < 0) {
		perror("Error binding");
		exit (EXIT_FAILURE);
	}

	/* Listen on socket. */
	if (listen(sock1, 1) < 0)
    {
      perror("Error start listening on socket");
      exit(EXIT_FAILURE);
    }
	
	
	/* Create another the socket. */
	int sock2 = dup3(sock1, 8, 0);
	

	/* Listen on socket. */
	if (listen(sock2, 1) < 0)
    {
      perror("Error start listening on socket");
      exit(EXIT_FAILURE);
    }


	/* Initialize the set of active sockets. */
	fd_set active_fd_set, read_fd_set;
	struct sockaddr_in clientname;
  	FD_ZERO(&active_fd_set);
  	FD_SET(sock1, &active_fd_set);
	//FD_SET(sock2, &active_fd_set);


	printf("Start listening on port %d & %d...\n", PORT1, PORT2);
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
				
                if (i == sock1 || i == sock2) {
					/* Accept new connection request. */
					size_t size = sizeof(clientname);
					int new = accept(i, (struct sockaddr *) &clientname, (socklen_t *) &size);
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
						/* Close connection. */
					    close(i);
	                    FD_CLR(i, &active_fd_set);
					} else {
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

