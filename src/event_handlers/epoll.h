#ifndef DSU_EPOLL
#define DSU_EPOLL


#include <sys/epoll.h>


extern struct dsu_fd_list *dsu_epoll_fds;



int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
extern int (*dsu_epoll_wait)(int, struct epoll_event *, int, int);


int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask);
int epoll_pwait2(int epfd, struct epoll_event *events, int maxevents, const struct timespec *timeout, const sigset_t *sigmask);
extern int (*dsu_epoll_pwait)(int, struct epoll_event *, int, int, const sigset_t *);
extern int (*dsu_epoll_pwait2)(int, struct epoll_event *, int, const struct timespec *, const sigset_t *);


int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
extern int (*dsu_epoll_ctl)(int, int, int, struct epoll_event *);

#endif
