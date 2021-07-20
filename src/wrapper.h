#ifndef DSU_UTILS
#define DSU_UTILS


int getsockopt(int sockfd, int level, int optname, void *restrict optval, socklen_t *restrict optlen);
extern int (*dsu_getsockopt)(int, int, int, void *restrict, socklen_t *restrict);


int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
extern int (*dsu_setsockopt)(int, int, int, const void *, socklen_t);


int getsockname(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);
extern int (*dsu_getsockname)(int, struct sockaddr *restrict, socklen_t *restrict);


int getpeername(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);
extern int (*dsu_getpeername)(int, struct sockaddr *restrict, socklen_t *restrict);


extern ssize_t (*dsu_read)(int, void *, size_t);
ssize_t read(int fd, void *buf, size_t count);


extern ssize_t (*dsu_recv)(int, void *, size_t, int);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);


extern ssize_t (*dsu_recvfrom)(int, void *restrict, size_t, int, struct sockaddr *restrict, socklen_t *restrict);
ssize_t recvfrom(int sockfd, void *restrict buf, size_t len, int flags, struct sockaddr *restrict src_addr, socklen_t *restrict addrlen);


extern ssize_t (*dsu_recvmsg)(int, struct msghdr *, int);
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);


extern ssize_t (*dsu_write)(int, const void *, size_t);
ssize_t write(int fd, const void *buf, size_t count);


extern ssize_t (*dsu_send)(int, const void *, size_t, int);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);


extern ssize_t (*dsu_sendto)(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);


extern ssize_t (*dsu_sendmsg)(int, const struct msghdr *, int);
ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);


extern int (*dsu_shutdown)(int, int);
int shutdown(int sockfd, int how);


extern int (*dsu_ioctl)(int, unsigned long, char *);
int ioctl(int fd, unsigned long request, char *argp);


#ifdef DEBUG
int socket(int domain, int type, int protocol);
extern int (*dsu_socket)(int, int, int);
#endif


#endif
