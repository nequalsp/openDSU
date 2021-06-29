#define _GNU_SOURCE
       
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "../core.h"
#include "../state.h"
#include "../communication.h"


int (*dsu_poll)(struct pollfd *, nfds_t, int);


int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
	DSU_DEBUG_PRINT("Poll() (%d-%d)\n", (int) getpid(), (int) gettid());
	return dsu_poll(fds, nfds, timeout);
}
