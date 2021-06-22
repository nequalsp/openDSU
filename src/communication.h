#ifndef DSU_COMMUNICATION
#define DSU_COMMUNICATION


#include "core.h"


int dsu_write_fd(int fd, int sendfd, int port);


int dsu_read_fd(int fd, int *recvfd, int *port);

DSU_COMMUNICATION

#endif
