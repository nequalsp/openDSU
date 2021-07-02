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
#include <sys/shm.h>
#include <poll.h>

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

	/* Initialize the set of active sockets. */
	struct sockaddr_in clientname;
	struct pollfd fds[200]; memset(fds, 0, sizeof(fds));
    int nfds = 1; int _ndfs = 0; int compress = 0;
    
    fds[0].fd = sock;
    fds[0].events = POLLIN;

	printf("Start listening on port %d...\n", PORT);
	while (1)
	{
    
        if (poll(fds, nfds, 500) < 0) {
          perror("  poll() failed");
          exit(EXIT_FAILURE);
        }

        _ndfs = nfds;
        for (int i = 0; i < _ndfs; i++)
        {
          
			if(fds[i].revents == 0)
				continue;

			if(fds[i].revents != POLLIN) {
				printf("  Error! revents = %d\n", fds[i].revents);
                exit(EXIT_FAILURE);
			}

			if (fds[i].fd == sock)
			{
				size_t size = sizeof(clientname);
				int new = accept(sock, (struct sockaddr *) &clientname, (socklen_t *) &size);
				if (new < 0) {
					perror("Error accepting message");
					exit(EXIT_FAILURE);
	            }
				
				fds[nfds].fd = new;
				fds[nfds].events = POLLIN;
				nfds++;

			} else {
				
				/* Read message. */
				char buffer[MAXMSG];
				int nbytes;
				
				nbytes = read(fds[i].fd, buffer, MAXMSG);
				if (nbytes < 0) {
					/* Read error. */
					perror("Error reading message");
					exit(EXIT_FAILURE);
				} else if (nbytes == 0) {
					/* Close connection. */
					close(fds[i].fd);
					fds[i].fd = -1;
					compress = 1;
				} else {
					/* Write response. */
					char response[25] = "Hello, this is version 2\0";
					if ( write(fds[i].fd, response, sizeof(response)-1) < 0) {
						/* Read error. */
						perror("Error writing message");
						exit(EXIT_FAILURE);
					}
				}
			}
		}

		if (compress) {

			  compress = 0;

			  for (int i = 0; i < nfds; i++) {
					if (fds[i].fd == -1) {
						  
						for(int j = i; j < nfds; j++) {
							fds[j].fd = fds[j+1].fd;
						}
							
						i--;
						nfds--;
					}
			  }
		}
	}
}	


              
              

              


































		
		

