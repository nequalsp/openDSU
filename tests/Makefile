# Compilation settings
CFLAGS	+= -Wall -Werror
CFLAGS	+= -O3
CFLAGS	+= -g2 
CC?=gcc

LIBS=-pthread


# File location of test scripts.
SRC0:=version_0
SRCS0:=$(wildcard $(SRC0)/*.c)
OBJS0:=$(patsubst $(SRC0)/%.c,$(SRC0)/%.o,$(SRCS0))

SRC1:=version_1
SRCS1:=$(wildcard $(SRC1)/*.c)
OBJS1:=$(patsubst $(SRC1)/%.c,$(SRC1)/%.o,$(SRCS1))


# Generate test files.
test: $(OBJS0) $(OBJS1)

$(SRC0)/%.o: $(SRC0)/%.c
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

$(SRC1)/%.o: $(SRC1)/%.c
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

clean:
	rm -f $(OBJS0)
	rm -f $(OBJS1)
	rm -rf $(SRC0)/__pycache__/
	rm -rf $(SRC1)/__pycache__/
