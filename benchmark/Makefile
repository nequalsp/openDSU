mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(dir $(mkfile_path))



benchmark: lighttpd nginx.o




# Install lighttpd.
lighttpd: lighttpd1.4/src/lighttpd lighttpd1.4/src/libs/mod_access.so
	cp lighttpd1.4/src/lighttpd lighttpd.o

lighttpd1.4:
	git clone https://github.com/lighttpd/lighttpd1.4.git
	
lighttpd1.4/Makefile.in: lighttpd1.4
	cd lighttpd1.4;\
	./autogen.sh;

lighttpd1.4/Makefile: lighttpd1.4/Makefile.in
	cd lighttpd1.4;\
	./configure --libdir=$(current_dir)lighttpd1.4/src/libs\
		--libexecdir=$(current_dir)lighttpd1.4/src/libs\
		--sysconfdir=$(current_dir)config

lighttpd1.4/src/lighttpd: lighttpd1.4/Makefile
	cd lighttpd1.4;\
	make

lighttpd1.4/src/libs/mod_access.so: lighttpd1.4/Makefile
	cd lighttpd1.4;\
	mkdir -p src/libs;\
	make install




# Install Ningx
nginx:
	git clone https://github.com/nginx/nginx.git

nginx/Makefile: nginx
	cd nginx;\
	./auto/configure \
		--user=root \
		--build=Ubuntu \
		--sbin-path=$(current_dir) \
		--conf-path=$(current_dir)config/nginx.conf \
		--with-select_module \
		--with-poll_module

nginx/objs/nginx: nginx/Makefile
	cd nginx;\
	make

nginx.o: nginx/objs/nginx
	cp nginx/objs/nginx nginx.o




clean:
	rm -rf lighttpd1.4
	rm -rf nginx
	rm -f nginx.o
	rm -f lighttpd.o
