# Compilation settings
CFLAGS	+= -Wall -Werror
CFLAGS	+= -O3
CFLAGS	+= -g2
CC?=gcc

LIB=/usr/local/lib/openDSU
INCLUDE=/usr/local/include/openDSU

# File location of test scripts.
SRC:=version_0
OBJ:=version_0
INC:=version_0/includes
SRCS:=$(wildcard $(SRC)/*.c)
OBJS:=$(patsubst $(SRC)/%.c,$(SRC)/%.o,$(SRCS))
INCS:=$(wildcard $(INC)/*.c)
INCS_OBJ:=$(patsubst $(INC)/%.c,$(INC)/%.o,$(INCS))


# Generate test files.
test: oracle.o $(INCS_OBJ) $(OBJS)

oracle.o:
	$(CC) $(CFLAGS) -I$(INCLUDE) -L$(LIB) oracle.c -o $@ -lopenDSU

$(INC)/%.o: $(INC)/%.c
	$(CC) -c $(CFLAGS) -I$(INCLUDE) $< -o $@

$(OBJ)/%.o: $(OBJ)/%.c
	$(CC) $(CFLAGS) -I$(INCLUDE) -L$(LIB) $< -o $@ $(INCS_OBJ) -lopenDSU

clean:
	rm -f $(OBJS)
	rm -f $(OBJS)
	rm -f $(INCS_OBJ)
	rm -f oracle.o
