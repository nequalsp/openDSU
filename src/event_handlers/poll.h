#ifndef DSU_POLL
#define DSU_POLL


#include <sys/poll.h>

int __poll_chk(struct pollfd *fds, nfds_t nfds, int timeout, size_t fdslen);
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
extern int (*dsu_poll)(struct pollfd *, nfds_t, int);


int __ppoll_chk (struct pollfd *fds, nfds_t nfds, const struct timespec *timeout, const __sigset_t *ss, __SIZE_TYPE__ fdslen);
int ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *timeout_ts, const sigset_t *sigmask);
extern int (*dsu_ppoll)(struct pollfd *, nfds_t, const struct timespec *, const sigset_t *);

#endif
