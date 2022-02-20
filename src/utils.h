#ifndef DSU_UTILS
#define DSU_UTILS


//int fcntl(int fd, int cmd, char *argp);
extern int (*dsu_fcntl)(int, int, char *);


//int ioctl(int fd, unsigned long request, char *argp);
extern int (*dsu_ioctl)(int, unsigned long, char *);


//int getsockopt(int sockfd, int level, int optname, void *restrict optval, socklen_t *restrict optlen);
extern int (*dsu_getsockopt)(int, int, int, void *restrict, socklen_t *restrict);


//int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
extern int (*dsu_setsockopt)(int, int, int, const void *, socklen_t);


//int getsockname(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);
extern int (*dsu_getsockname)(int, struct sockaddr *restrict, socklen_t *restrict);


//int getpeername(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);
extern int (*dsu_getpeername)(int, struct sockaddr *restrict, socklen_t *restrict);


#endif
