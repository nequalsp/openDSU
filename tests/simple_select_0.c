#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/un.h>

//#include "../openDSU.h"

#define PORT    3000
#define MAXMSG  512

/* define if struct msghdr contains the msg_control element */
#define HAVE_MSGHDR_MSG_CONTROL 1
#define DSU_COMM "/tmp/dsu_comm.unix"
#define DSU_COMM_LEN 18
ssize_t write_fd(int fd, void *ptr, size_t nbytes, int sendfd) {
	struct msghdr msg;
	struct iovec iov[1];

	#ifdef  HAVE_MSGHDR_MSG_CONTROL
	union {
		struct cmsghdr cm;
		char    control[CMSG_SPACE(sizeof(int))];
	} control_un;
	struct cmsghdr *cmptr;

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);

	cmptr = CMSG_FIRSTHDR(&msg);
	cmptr->cmsg_len = CMSG_LEN(sizeof(int));
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_RIGHTS;
	*((int *) CMSG_DATA(cmptr)) = sendfd;
	#else
	msg.msg_accrights = (caddr_t) & sendfd;
	msg.msg_accrightslen = sizeof(int);
	#endif

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	iov[0].iov_base = ptr;
	iov[0].iov_len = nbytes;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	return (sendmsg(fd, &msg, 0));
}

int main (int argc, char **argv) {
    

    /******************************************************************************************/
    //DSU_INIT(argc, argv);
    /* Create Unix domain socket. */
    int sockufd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un dsu_name;
    bzero(&dsu_name, sizeof(dsu_name));
    dsu_name.sun_family = AF_UNIX;
    strncpy(dsu_name.sun_path, DSU_COMM, DSU_COMM_LEN);
    /* Bind socket, if it fails and already exists we know that another DSU application is 
       already running. These applications need to know each others listening ports. */
    unlink(DSU_COMM);
    if ( bind(sockufd, (struct sockaddr *) &dsu_name, (socklen_t) sizeof(dsu_name)) != 0) {
            perror("bind");
            exit(EXIT_FAILURE);
    }
    /******************************************************************************************/
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
    printf("Socket: %d\n", sock);
    
    /******************************************************************************************/
    listen(sockufd, 1);    
    int temp_fd =  accept(sockufd, NULL, NULL);    
    if (write_fd(temp_fd, "", 1, sock) < 0) {
        perror("sendmsg");
        exit(EXIT_FAILURE);
    }
    
    /******************************************************************************************/

	/* Listen on socket. */
	if (listen(sock, 1) < 0)
    {
      perror("Error start listening on socket");
      exit(EXIT_FAILURE);
    }
	
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

