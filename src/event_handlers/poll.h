#ifndef DSU_POLL
#define DSU_POLL


#include <poll.h>


int poll(struct pollfd *fds, nfds_t nfds, int timeout);


extern int (*dsu_poll)(struct pollfd *, nfds_t, int);


#endif
