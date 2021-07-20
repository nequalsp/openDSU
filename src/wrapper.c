#include <unistd.h>
#include <sys/socket.h>


#include "state.h"
#include "core.h"
#include "file.h"


int (*dsu_getsockopt)(int, int, int, void *restrict, socklen_t *restrict);
int (*dsu_setsockopt)(int, int, int, const void *, socklen_t);
int (*dsu_getsockname)(int, struct sockaddr *restrict, socklen_t *restrict);
int (*dsu_getpeername)(int, struct sockaddr *restrict, socklen_t *restrict);
ssize_t (*dsu_read)(int, void *, size_t);
ssize_t (*dsu_recv)(int, void *, size_t, int);
ssize_t (*dsu_recvfrom)(int, void *restrict, size_t, int, struct sockaddr *restrict, socklen_t *restrict);
ssize_t (*dsu_recvmsg)(int, struct msghdr *, int);
ssize_t (*dsu_write)(int, const void *, size_t);
ssize_t (*dsu_send)(int, const void *, size_t, int);
ssize_t (*dsu_sendto)(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
ssize_t (*dsu_sendmsg)(int, const struct msghdr *, int);
int (*dsu_shutdown)(int, int);
int (*dsu_ioctl)(int, unsigned long, char *);


#ifdef DEBUG
int (*dsu_socket)(int, int, int);
#endif


int getsockopt(int sockfd, int level, int optname, void *restrict optval, socklen_t *restrict optlen) {
	DSU_DEBUG_PRINT("getsockopt() %d -> %d level:%d opt:%d (%d)\n", sockfd, dsu_shadowfd(sockfd), level, optname, (int) getpid());
	return dsu_getsockopt(dsu_shadowfd(sockfd), level, optname, optval, optlen);
}


int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
	DSU_DEBUG_PRINT("setsockopt() %d -> %d level:%d opt:%d (%d)\n", sockfd, dsu_shadowfd(sockfd), level, optname, (int) getpid());
	return dsu_setsockopt(dsu_shadowfd(sockfd), level, optname, optval, optlen);
}


int getsockname(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen) {
	DSU_DEBUG_PRINT("Getsockname() (%d)\n", (int) getpid());
	return dsu_getsockname(dsu_shadowfd(sockfd), addr, addrlen);
}


int getpeername(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen) {
	DSU_DEBUG_PRINT("Getpeername() (%d)\n", (int) getpid());
	return dsu_getpeername(dsu_shadowfd(sockfd), addr, addrlen);
}


ssize_t read(int fd, void *buf, size_t count) {
	DSU_DEBUG_PRINT("read() %d -> %d (%d)\n", fd, dsu_shadowfd(fd), (int) getpid());
	return dsu_read(dsu_shadowfd(fd), buf, count);
}


ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
	DSU_DEBUG_PRINT("recv() %d -> %d (%d)\n", sockfd, dsu_shadowfd(sockfd), (int) getpid());
	return dsu_recv(dsu_shadowfd(sockfd), buf, len, flags);
}


ssize_t recvfrom(int sockfd, void *restrict buf, size_t len, int flags, struct sockaddr *restrict src_addr, socklen_t *restrict addrlen) {
	DSU_DEBUG_PRINT("recvfrom() %d -> %d (%d)\n", sockfd, dsu_shadowfd(sockfd), (int) getpid());
	return dsu_recvfrom(dsu_shadowfd(sockfd), buf, len, flags, src_addr, addrlen);
}


ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
	DSU_DEBUG_PRINT("recvmsg() %d -> %d (%d)\n", sockfd, dsu_shadowfd(sockfd), (int) getpid());
	return dsu_recvmsg(dsu_shadowfd(sockfd), msg, flags);
}


ssize_t write(int fd, const void *buf, size_t count) {
	DSU_DEBUG_PRINT("write() %d -> %d (%d)\n", fd, dsu_shadowfd(fd), (int) getpid());
	return dsu_write(dsu_shadowfd(fd), buf, count);
}


ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
	DSU_DEBUG_PRINT("send() %d -> %d (%d)\n", sockfd, dsu_shadowfd(sockfd), (int) getpid());
	return dsu_send(dsu_shadowfd(sockfd), buf, len, flags);
}


ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
	DSU_DEBUG_PRINT("sendto() %d -> %d (%d)\n", sockfd, dsu_shadowfd(sockfd), (int) getpid());
	return dsu_sendto(dsu_shadowfd(sockfd), buf, len, flags, dest_addr, addrlen);
}


ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags) {
	DSU_DEBUG_PRINT("sendmsg() %d -> %d (%d)\n", sockfd, dsu_shadowfd(sockfd), (int) getpid());
	return dsu_sendmsg(dsu_shadowfd(sockfd), msg, flags);
}


int shutdown(int sockfd, int how) {
	DSU_DEBUG_PRINT("Shutdown() (%d)\n", (int) getpid());
	return dsu_shutdown(dsu_shadowfd(sockfd), how);
}


int ioctl(int fd, unsigned long request, char *argp) {
	DSU_DEBUG_PRINT("ioctl() (%d)\n", (int) getpid());

	return dsu_ioctl(dsu_shadowfd(fd), request, argp);
}


#ifdef DEBUG
int socket(int domain, int type, int protocol) {
    DSU_DEBUG_PRINT("Socket() (%d)\n", (int) getpid());
    return dsu_socket(domain, type, protocol);
}
#endif