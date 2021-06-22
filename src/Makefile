CFLAGS	+= -Wall -Werror
CFLAGS	+= -O3
CFLAGS	+= -g2
CC?=gcc


DEBUG= -D DEBUG	


SRCS:=$(wildcard *.c)
T1OBJS:=$(patsubst %.c,%.o,$(SRCS))
T2OBJS := $(filter-out core.o	, $(T1OBJS))
OBJS := $(filter-out exec.o	, $(T2OBJS))


EVENT_SRCS:=$(wildcard event_handlers/*.c)
EVENT_OBJS:=$(patsubst event_handlers/%.c, event_handlers/%.o,$(EVENT_SRCS))


all: build 


build: ../openDSU ../libopenDSU.so


../libopenDSU.so: $(OBJS) $(EVENT_OBJS)
	$(CC) $(CFLAGS) -shared -fPIC core.c -o $@ $^ $(DEBUG) -ldl -lrt


../openDSU: exec.c
	$(CC) $(CFLAGS) $< -o $@


%.o: %.c
	$(CC) $(CFLAGS) -c -fPIC $< -o $@ $(DEBUG)


clean:
	rm -rf $(OBJS) $(EVENT_OBJS) ../libopenDSU.so ../openDSU

