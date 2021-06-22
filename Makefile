CFLAGS	+= -Wall -Werror
CFLAGS	+= -O3
CFLAGS	+= -g2
CC?=gcc


DEBUG= -D DEBUG

	
LIB=/usr/local/lib
BIN=/usr/local/bin


all: build install test


build: 
	$(MAKE) -C ./src build


install: 
	cp libopenDSU.so $(LIB)/libopenDSU.so
	cp openDSU $(BIN)/openDSU


uninstall:
	rm -f $(LIB)/libopenDSU.so
	rm -f $(BIN)/openDSU


test:
	$(MAKE) -C ./tests test


clean: uninstall
	$(MAKE) -C ./tests clean
	$(MAKE) -C ./src clean
	



