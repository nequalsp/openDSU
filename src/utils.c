#include <unistd.h>
#include <sys/socket.h>


#include "state.h"
#include "core.h"
#include "file.h"


int (*dsu_getsockopt)(int, int, int, void *restrict, socklen_t *restrict);
int (*dsu_setsockopt)(int, int, int, const void *, socklen_t);
int (*dsu_getsockname)(int, struct sockaddr *restrict, socklen_t *restrict);
int (*dsu_getpeername)(int, struct sockaddr *restrict, socklen_t *restrict);


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
