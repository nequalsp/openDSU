#ifndef DSU_SELECT
#define DSU_SELECT


#include <sys/select.h>


int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
extern int (*dsu_select)(int, fd_set *, fd_set *, fd_set *, struct timeval *);


int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask);
extern int (*dsu_pselect)(int, fd_set *, fd_set *, fd_set *, const struct timespec *, const sigset_t *);


#endif
