CFLAGS	+= -Wall -Werror
CFLAGS	+= -O3
CFLAGS	+= -g2
CC?=gcc


DEBUG= #-D DEBUG

	
LIB=/usr/local/lib
BIN=/usr/local/bin


all: uninstall build install




build: 
	$(MAKE) -C ./src build





install: $(LIB)/libopenDSU.so $(BIN)/openDSU

$(LIB)/libopenDSU.so: 
	cp libopenDSU.so $(LIB)/libopenDSU.so

$(BIN)/openDSU:
	cp openDSU $(BIN)/openDSU




uninstall:
	rm -f $(LIB)/libopenDSU.so
	rm -f $(BIN)/openDSU





test:
	$(MAKE) -C ./tests test




benchmarks:
	$(MAKE) -C ./benchmark benchmark

apache:
	$(MAKE) -C ./benchmark apache.o

nginx:
	$(MAKE) -C ./benchmark nginx.o

httpdlight:
	$(MAKE) -C ./benchmark httpdlight.o




clean:
	$(MAKE) -C ./tests clean
	$(MAKE) -C ./src clean
	#$(MAKE) -C ./benchmark clean
	#rm -f /var/log/dsu_*
	



