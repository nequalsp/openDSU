#include <arpa/inet.h>


#include "core.h"


int dsu_write_fd(int fd, int sendfd, int port) {
	/* 	Write file descriptor on the stream pipe. Integer with port number/ error is also send in the
		Message diagram. Used from the book: "Unix Network Programming from W.Richard Stevens." */
    
    int32_t nport = htonl(port);
    char *ptr = (char*) &nport;
    int nbytes = sizeof(nport);

	struct msghdr msg;
	struct iovec iov[1];
    
    /* Check whether the socket is valid. */
    int error; socklen_t len; 
    if (getsockopt(sendfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
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


int dsu_read_fd(int fd, int *recvfd, int *port) {
    /* 	Recieve file descriptor on the stream pipe. Integer with port number is also read from the
		Message diagram. Used from the book: "Unix Network Programming from W.Richard Stevens." */
    int32_t nport;
    char *ptr = (char*)&nport;
    int nbytes = sizeof(nport);

	struct msghdr msg;
	struct iovec iov[1];
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

	msg.msg_accrights = (caddr_t) & newfd;
	msg.msg_accrightslen = sizeof(int);
	#endif

	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	iov[0].iov_base = ptr;
	iov[0].iov_len = nbytes;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	/* These calls return the number of bytes received, or -1 if an error occurred. The return value will be 0 when the peer has performed an orderly shutdown. */
	if ( (n = recvmsg(fd, &msg, 0)) <= 0)
		return (n);

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
    
    *port = ntohl(nport);
	return (n);
}

