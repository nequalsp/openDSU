#ifndef DSU_COMMUNICATION
#define DSU_COMMUNICATION


/* 	Write file descriptor on the stream pipe. Integer with port number/ error is also send in the
	Message diagram. Used from the book: "Unix Network Programming from W.Richard Stevens." */
int dsu_write_fd(int fd, int sendfd, int port);


/* 	Recieve file descriptor on the stream pipe. Integer with port number is also read from the
	Message diagram. Used from the book: "Unix Network Programming from W.Richard Stevens." */
int dsu_read_fd(int fd, int *recvfd, int *port);



#endif
