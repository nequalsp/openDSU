# Compilation settings
CFLAGS	+= -Wall -Werror
CFLAGS	+= -O3
CFLAGS	+= -g2
CC?=gcc

DEBUG= -D DEBUG	
LIB=/usr/local/lib/openDSU

# File location of test scripts.
SRC:=tests/version_0
OBJ:=tests/version_0
INC:=tests/version_0/includes
SRCS:=$(wildcard $(SRC)/*.c)
OBJS:=$(patsubst $(SRC)/%.c,$(SRC)/%.o,$(SRCS))
INCS:=$(wildcard $(INC)/*.c)
INCS_OBJ:=$(patsubst $(INC)/%.c,$(INC)/%.o,$(INCS))


all: build openDSU install test

rebuild: clean build test


# Compile shared library.	
build: libopenDSU.so

libopenDSU.so: openDSU.c
	$(CC) $(CFLAGS) -shared -fPIC $< -o $@ $(DEBUG) -ldl

openDSU: install/exec.c
	$(CC) $(CFLAGS) $< -o $@

install: build openDSU
	$(MAKE) -C ./install install

uninstall:
	$(MAKE) -C ./install uninstall

test:
	$(MAKE) -C ./tests test

clean:
	rm -f libopenDSU.so
	rm -f openDSU
	$(MAKE) -C ./tests clean

