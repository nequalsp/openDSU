#ifndef openDLU
#define openDLU

struct dlu_state {
	new_version bool,

}; 

int dlu_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
	return select(nfds, readfds, writefds, exceptfds, timeout);
}
#define select(nfds, readfds, writefds, exceptfds, timeout) dlu_select(nfds, readfds, writefds, exceptfds, timeout)


#endif
