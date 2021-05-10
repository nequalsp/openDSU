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

#include "simple_select_linked.h"


#define PORT    3000
#define MAXMSG  512


int init() {
    /* After crash the unix domain socket stays alive. 
       to solve this for now I unlink in de first version. */
    DSU_INIT;
    
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
	
	return start(sock);

}

