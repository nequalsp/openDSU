mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(dir $(mkfile_path))
log_dir	:= $(current_dir)../log


CFLAGS	+= -Wall -Werror
CFLAGS	+= -O3
CFLAGS	+= -g2
CC?=gcc


#DEBUG= -D TEST="\"$(log_dir)\""	
DEBUG= -D DEBUG="\"$(log_dir)\""	
#DEBUG=

SRCS:=$(wildcard *.c)
T1OBJS:=$(patsubst %.c,%.o,$(SRCS))
T2OBJS := $(filter-out core.o	, $(T1OBJS))
OBJS := $(filter-out exec.o	, $(T2OBJS))


EVENT_SRCS:=$(wildcard event_handlers/*.c)
EVENT_OBJS:=$(patsubst event_handlers/%.c, event_handlers/%.o,$(EVENT_SRCS))


all: build 

# core.o to trigger changes.
build: ../openDSU ../libopenDSU.so core.o ../log


../libopenDSU.so: $(OBJS) $(EVENT_OBJS)
	$(CC) $(CFLAGS) -shared -fPIC core.c -o $@ $^ $(DEBUG) -ldl


../openDSU: exec.c
	$(CC) $(CFLAGS) $< -o $@


../log:
	mkdir -p ../log


%.o: %.c
	$(CC) $(CFLAGS) -c -fPIC $< -o $@ $(DEBUG)


clean:
	rm -rf $(OBJS) $(EVENT_OBJS) ../libopenDSU.so ../openDSU core.o $(log_dir)

