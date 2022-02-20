#include <unistd.h>
#include <sys/socket.h>


#include "state.h"
#include "core.h"
#include "file.h"


int (*dsu_fcntl)(int, int, char *);
int (*dsu_ioctl)(int, unsigned long, char *);
int (*dsu_getsockopt)(int, int, int, void *restrict, socklen_t *restrict);
int (*dsu_setsockopt)(int, int, int, const void *, socklen_t);
int (*dsu_getsockname)(int, struct sockaddr *restrict, socklen_t *restrict);
int (*dsu_getpeername)(int, struct sockaddr *restrict, socklen_t *restrict);


//int getsockopt(int sockfd, int level, int optname, void *restrict optval, socklen_t *restrict optlen) {
//	DSU_DEBUG_PRINT("getsockopt() %d -> %d level:%d opt:%d (%d)\n", sockfd, dsu_shadowfd(sockfd), level, optname, (int) getpid());
//	return dsu_getsockopt(dsu_shadowfd(sockfd), level, optname, optval, optlen);
//}


//int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
//	DSU_DEBUG_PRINT("setsockopt() %d -> %d level:%d opt:%d (%d)\n", sockfd, dsu_shadowfd(sockfd), level, optname, (int) getpid());
//	return dsu_setsockopt(dsu_shadowfd(sockfd), level, optname, optval, optlen);
//}


//int getsockname(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen) {
//	DSU_DEBUG_PRINT("Getsockname() (%d)\n", (int) getpid());
//	return dsu_getsockname(dsu_shadowfd(sockfd), addr, addrlen);
//}


//int getpeername(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen) {
//	DSU_DEBUG_PRINT("Getpeername() (%d)\n", (int) getpid());
//	return dsu_getpeername(dsu_shadowfd(sockfd), addr, addrlen);
//}


/* The prototype stands out in the list of Unix system calls because of the dots, which usually mark the function as having a variable number of arguments. In a real system, however, a system call can't actually have a variable number of arguments. System calls must have a well-defined prototype, because user programs can access them only through hardware "gates." Therefore, the dots in the prototype represent not a variable number of arguments but a single optional argument, traditionally identified as char *argp. The dots are simply there to prevent type checking during compilation. */
//int fcntl(int fd, int cmd, char *argp) {
//	DSU_DEBUG_PRINT("fcntl() fd: %d (%d)\n", fd, (int) getpid());
//	int v = dsu_fcntl(dsu_shadowfd(fd), cmd, argp);
//	DSU_DEBUG_PRINT(" - return: %d (%d)\n", v, (int) getpid());
//	if (cmd == F_DUPFD) DSU_DEBUG_PRINT(" - DUP (%d)\n", (int) getpid()); //|| cmd == F_DUPFD_CLOEXEC
//	return v;
//}


//int ioctl(int fd, unsigned long request, char *argp) {
//	DSU_DEBUG_PRINT("ioctl() (%d)\n", (int) getpid());
//	
//	return dsu_ioctl(dsu_shadowfd(fd), request, argp);
//}
