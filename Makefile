# Compilation settings
CFLAGS	+= -Wall
CFLAGS	+= -O3
CFLAGS	+= -g2
CC?=gcc
LIB=
INC=
ARCH=

# File location
SRC:=./tests
OBJ:=./tests
SRCS:=$(wildcard $(SRC)/*.c)
OBJS:=$(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(SRCS))


all: $(OBJS)

$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	$(CC) $(CFLAGS) $< -o $@ $(LIB)

clean:
	rm -f $(OBJS)
