LIB=/usr/local/lib/openDSU
INCLUDE=/usr/local/include/openDSU


install: make_dir
	cp ../libopenDSU.so $(LIB)/libopenDSU.so
	cp ../openDSU $(LIB)/openDSU

uninstall:
	rm -f $(LIB)/libopenDSU.so
	rm -f $(LIB)/openDSU

make_dir:
	mkdir -p $(LIB)
