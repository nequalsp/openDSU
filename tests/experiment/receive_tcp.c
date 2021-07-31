#define HAVE_MSGHDR_MSG_CONTROL 1

#include <arpa/inet.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


int read_fd(int fd, int *recvfd, int *port);


int main(int argc, char **argv) {
	
	struct sockaddr_un server_name;
	bzero(&server_name, sizeof(struct sockaddr_un));
    server_name.sun_family = AF_UNIX;
    sprintf(server_name.sun_path, "Xserver.unix");    // On Linux, sun_path is 108 bytes in size.
    server_name.sun_path[0] = '\0';                 // Abstract linux socket.
    int commfd = socket(AF_UNIX, SOCK_STREAM, 0);

	int rsock = 0; int port;

	
	printf("Connect\n");
	if (connect(commfd, (struct sockaddr *) &server_name, sizeof(struct sockaddr_un)) == 0) {
		printf("Read\n");
		int n = read_fd(commfd, &rsock, &port);
		printf("n=%d\n", n);
		if (n < 0) {
			perror("read error");
			return 1;
		}
	} else {
		perror("connect error");
		return 1;
	}
	
	printf("recieved %d\n", rsock);


	int sock = socket(PF_INET, SOCK_STREAM, 0);
	dup2(rsock, sock);
	

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

	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);

	return 0;
}


int read_fd(int fd, int *recvfd, int *port) {
    /* 	Recieve file descriptor on the stream pipe. Integer with port number is also read from the
		Message diagram. Used from the book: "Unix Network Programming from W.Richard Stevens." */


	struct msghdr msg;
	int n;


	#ifdef HAVE_MSGHDR_MSG_CONTROL
	union {
		struct cmsghdr cm;
		char     control[CMSG_SPACE(sizeof (int))];
	} control_un;
	struct cmsghdr  *cmptr;

	msg.msg_control  = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);
	#else
	int newfd;
	msg.msg_accrights = (caddr_t) &newfd;
	msg.msg_accrightslen = sizeof(int);
	#endif


	msg.msg_name = NULL;
	msg.msg_namelen = 0;


	struct iovec iov[1];
	char ptr;
	iov[0].iov_base = &ptr;
	iov[0].iov_len = 1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;


	/* These calls return the number of bytes received, or -1 if an error occurred. The return value will be 0 when the peer has performed an orderly shutdown. */
	if ( (n = recvmsg(fd, &msg, 0)) <= 0) {	
		return (n);
	}

	#ifdef  HAVE_MSGHDR_MSG_CONTROL
	if ( (cmptr = CMSG_FIRSTHDR(&msg)) != NULL &&
		cmptr->cmsg_len == CMSG_LEN(sizeof(int))) {
		if (cmptr->cmsg_level != SOL_SOCKET) {
			*recvfd = -1; return -1;
        }
		if (cmptr->cmsg_type != SCM_RIGHTS) {
			*recvfd = -1; return -1;
        }
		*recvfd = *((int *) CMSG_DATA(cmptr));
	} else
		*recvfd = -1;
	#else
	if (msg.msg_accrightslen == sizeof(int))
		*recvfd = newfd;
	else
		*recvfd = -1;
	#endif
    
	return (n);
}



