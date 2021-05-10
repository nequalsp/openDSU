LIB=/usr/local/lib/openDSU
INCLUDE=/usr/local/include/openDSU


install: make_dir
	cp ../libopenDSU.so $(LIB)/libopenDSU.so
	cp ../openDSU.h $(INCLUDE)/openDSU.h
	cp openDSU.conf /etc/ld.so.conf.d/openDSU.conf	# Ubuntu.

uninstall:
	rm -f $(LIB)/libopenDSU.so
	rm -f $(INCLUDE)/openDSU.h
	rm -f /etc/ld.so.conf.d/openDSU.conf # Ubuntu.

make_dir:
	mkdir -p $(LIB)
	mkdir -p $(INCLUDE)
