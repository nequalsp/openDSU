#ifndef DSU_EPOLL
#define DSU_EPOLL


#include <sys/epoll.h>

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

extern int (*dsu_epoll_wait)(int, struct epoll_event *, int, int);

int epoll_create1(int flags);

extern int (*dsu_epoll_create1)(int);

int epoll_create(int flags);

extern int (*dsu_epoll_create)(int);

//int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);

//extern int (*dsu_epoll_ctl)(int, int, int, struct epoll_event *);


//int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask);


//int epoll_pwait2(int epfd, struct epoll_event *events, int maxevents, const struct timespec *timeout, const sigset_t *sigmask);


#endif
