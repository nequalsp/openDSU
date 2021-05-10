# Compilation settings
CFLAGS	+= -Wall -Werror
CFLAGS	+= -O3
CFLAGS	+= -g2
CC?=gcc

DEBUG= -D DEBUG	
LIB=/usr/local/lib/openDSU
INCLUDE=/usr/local/include/openDSU

# File location of test scripts.
SRC:=tests/version_0
OBJ:=tests/version_0
INC:=tests/version_0/includes
SRCS:=$(wildcard $(SRC)/*.c)
OBJS:=$(patsubst $(SRC)/%.c,$(SRC)/%.o,$(SRCS))
INCS:=$(wildcard $(INC)/*.c)
INCS_OBJ:=$(patsubst $(INC)/%.c,$(INC)/%.o,$(INCS))


all: build install test


# Compile shared library.	
build: libopenDSU.so

openDSU.o: openDSU.c
	$(CC) -c $(CFLAGS) -fpic $< -o $@ $(DEBUG)

libopenDSU.so: openDSU.o
	$(CC) -shared $< -o $@ $(DEBUG)


install: build
	$(MAKE) -C ./install install

uninstall:
	$(MAKE) -C ./install uninstall


test:
	$(MAKE) -C ./tests test

clean:
	rm -f libopenDSU.so
	rm -f openDSU.o
	$(MAKE) -C ./tests clean

