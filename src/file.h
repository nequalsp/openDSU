/* CANOT import sys/file.h */
/* File access modes for `open' and `fcntl'. */
#define O_RDONLY	00000000
#define O_WRONLY	00000001
#define O_RDWR		00000002
#define F_DUPFD		0	
#define F_GETFD		1	
#define F_SETFD		2
#define F_GETFL		3	
#define F_SETFL		4
#define O_TMPFILE	020000000

#ifndef O_CREAT
#define O_CREAT		00000100	/* not fcntl */
#endif
#ifndef O_EXCL
#define O_EXCL		00000200	/* not fcntl */
#endif
#ifndef O_NOCTTY
#define O_NOCTTY	00000400	/* not fcntl */
#endif
#ifndef O_TRUNC
#define O_TRUNC		00001000	/* not fcntl */
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK	00004000
#endif




