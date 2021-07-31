#define HAVE_MSGHDR_MSG_CONTROL 1


#include <arpa/inet.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


int write_fd(int fd, int sendfd, int port, struct sockaddr_un *name);


int main(int argc, char **argv) {
	
	struct sockaddr_un server_name;
	bzero(&server_name, sizeof(struct sockaddr_un));
    server_name.sun_family = AF_UNIX;
    sprintf(server_name.sun_path, "Xserver.unix");    	// On Linux, sun_path is 108 bytes in size.
    server_name.sun_path[0] = '\0';                 	// Abstract linux socket.
    int commfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (bind(commfd, (struct sockaddr *) &server_name, sizeof(struct sockaddr_un))) {
        perror("binding name to datagram socket");
        return 1;
    }

	struct sockaddr_un server_name1;
	bzero(&server_name1, sizeof(struct sockaddr_un));
    server_name1.sun_family = AF_UNIX;
    sprintf(server_name1.sun_path, "Xserver1.unix");    	// On Linux, sun_path is 108 bytes in size.
    server_name1.sun_path[0] = '\0';                 	// Abstract linux socket.
	if (bind(commfd, (struct sockaddr *) &server_name1, sizeof(struct sockaddr_un))) {
        perror("binding name to datagram socket");
        return 1;
    }
	
	struct sockaddr_un client_name;
	bzero(&client_name, sizeof(struct sockaddr_un));
    client_name.sun_family = AF_UNIX;
    sprintf(client_name.sun_path, "Xclient.unix");    	// On Linux, sun_path is 108 bytes in size.
    client_name.sun_path[0] = '\0';                 	// Abstract linux socket.

	connect(commfd, (struct sockaddr *) &client_name, sizeof(struct sockaddr_un));
	//int fake = socket(AF_UNIX, SOCK_DGRAM, 0);
	//if (bind(fake, (struct sockaddr *) &client_name, sizeof(struct sockaddr_un))) {
    //   perror("binding name to datagram socket");
    //   return 1;
    //}

	int sock = socket(PF_INET, SOCK_STREAM, 0);
	//fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);

	printf("Write...\n");
	if (write_fd(commfd, sock, 3000, &client_name) < 0) {
		perror("failed send");
		return 1;
	}

	//close(fake);
	
	struct stat statbuf;
	fstat(sock, &statbuf);
	
	printf(	"\t\tfd: %d\n\
			flags: %d\n\
			dev: %lu\n\
			ino: %lu\n\
			mode: %u\n\
			nlink: %lu\n\
			uid: %d\n\
			gid: %d\n\
			rdev:%lu\n\
			off: %ld\n\
			blksize: %ld\n\
			blkcnt: %ld\n",
			sock,
			fcntl(sock, F_GETFL, 0),
			statbuf.st_dev,
			statbuf.st_ino,
			statbuf.st_mode,	            
			statbuf.st_nlink,
			statbuf.st_uid,
			statbuf.st_gid,
			statbuf.st_rdev,
			statbuf.st_size,
			statbuf.st_blksize,
			statbuf.st_blocks);

	sleep(5);

	printf(	"\t\tfd: %d\n\
			flags: %d\n\
			dev: %lu\n\
			ino: %lu\n\
			mode: %u\n\
			nlink: %lu\n\
			uid: %d\n\
			gid: %d\n\
			rdev:%lu\n\
			off: %ld\n\
			blksize: %ld\n\
			blkcnt: %ld\n",
			sock,
			fcntl(sock, F_GETFL, 0),
			statbuf.st_dev,
			statbuf.st_ino,
			statbuf.st_mode,	            
			statbuf.st_nlink,
			statbuf.st_uid,
			statbuf.st_gid,
			statbuf.st_rdev,
			statbuf.st_size,
			statbuf.st_blksize,
			statbuf.st_blocks);

	return 0;
}


int write_fd(int fd, int sendfd, int port, struct sockaddr_un *name) {
	/* 	Write file descriptor on the stream pipe. Integer with port number/ error is also send in the
		Message diagram. Used from the book: "Unix Network Programming from W.Richard Stevens." */
    
    //int32_t nport = htonl(port);
    //char *ptr = (char*) &nport;
    //int nbytes = sizeof(nport);

	struct msghdr msg;
	//struct iovec iov[1];
    
    /* Check whether the socket is valid. */
    int error; socklen_t len; 
    if (getsockopt(sendfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
		printf("test\n");
        return -1;
	}
    
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
	msg.msg_accrights = (caddr_t) &sendfd;
	msg.msg_accrightslen = sizeof(int);
	#endif

	msg.msg_name = name;
	msg.msg_namelen = sizeof(struct sockaddr_un);
	//msg.msg_name = NULL;
	//msg.msg_namelen = 0;

	//iov[0].iov_base = ptr;
	//iov[0].iov_len = nbytes;
	//msg.msg_iov = iov;
	//msg.msg_iovlen = 1;

	msg.msg_iov = NULL;
	msg.msg_iovlen = 0;

	return (sendmsg(fd, &msg, MSG_CONFIRM));
}




