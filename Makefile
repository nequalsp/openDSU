# Compilation settings
CFLAGS	+= -Wall
CFLAGS	+= -O3
CFLAGS	+= -g2
CC?=gcc
LIB=
INC=
ARCH=

# File location
SRC0:=./tests/version_0
OBJ0:=./tests/version_0
SRC1:=./tests/version_1
OBJ1:=./tests/version_1
SRCS0:=$(wildcard $(SRC0)/*.c)
OBJS0:=$(patsubst $(SRC0)/%.c,$(OBJ0)/%.o,$(SRCS0))
SRCS1:=$(wildcard $(SRC1)/*.c)
OBJS1:=$(patsubst $(SRC1)/%.c,$(OBJ1)/%.o,$(SRCS1))

all: $(OBJS0) $(OBJS1)

$(OBJ0)/%.o: $(OBJ0)/%.c
	$(CC) $(CFLAGS) $< -o $@ $(LIB)

$(OBJ1)/%.o: $(OBJ1)/%.c
	$(CC) $(CFLAGS) $< -o $@ $(LIB)

clean:
	rm -f $(OBJS0)
	rm -f $(OBJS1)
