mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(dir $(mkfile_path))




benchmark: server


server: lighttpd.o nginx.o apache.o



# Install lighttpd.
lighttpd.o: lighttpd1.4/src/lighttpd lighttpd1.4/src/libs/mod_access.so config/lighttpd.conf
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
		--sysconfdir=$(current_dir)config\
		--prefix=$(current_dir)etc/lighttpd

lighttpd1.4/src/lighttpd: lighttpd1.4/Makefile
	cd lighttpd1.4;\
	make

lighttpd1.4/src/libs/mod_access.so: lighttpd1.4/Makefile
	cd lighttpd1.4;\
	mkdir -p src/libs;\
	make install

config/lighttpd.conf:
	cp config/.lighttpd.conf config/lighttpd.conf
	sudo sed -i "s;{{dsu_www}};$(current_dir)www;" config/lighttpd.conf

etc/lighttpd:
	mkdir -p $(current_dir)etc/lighttpd




# Install Ningx
nginx:
	git clone https://github.com/nginx/nginx.git

nginx/Makefile: nginx
	cd nginx;\
	./auto/configure \
		--user=root \
		--prefix=$(current_dir)etc/nginx\
		--sbin-path=$(current_dir) \
		--conf-path=$(current_dir)config/nginx.conf \
		--with-select_module \
		--with-poll_module

nginx/objs/nginx: nginx/Makefile
	cd nginx;\
	make

config/nginx.conf:
	cp config/.nginx.conf config/nginx.conf
	sudo sed -i "s;{{dsu_www}};$(current_dir)www;" config/nginx.conf

etc/nginx:
	mkdir -p $(current_dir)etc/nginx

etc/nginx/logs: etc/nginx
	mkdir -p $(current_dir)etc/nginx/logs

nginx.o: nginx/objs/nginx config/nginx.conf etc/nginx/logs
	cp nginx/objs/nginx nginx.o




# Install Apache httpd
apache:
	git clone https://github.com/apache/httpd.git
	cp -r httpd apache
	rm -rf httpd

apache/configure: apache
	cd apache;\
	svn co http://svn.apache.org/repos/asf/apr/apr/trunk srclib/apr;\
	./buildconf

apache/Makefile: apache/configure
	cd apache;\
	./configure\
		--prefix=$(current_dir)etc/apache\
		--libdir=$(current_dir)etc/apache/modules\
		--with-included-apr
		--with-apr=$(current_dir)apache/srclib/apr
		--with-mpm=prefork
		

etc/apache:
	mkdir -p $(current_dir)etc/apache

etc/apache/logs: etc/apache
	mkdir -p $(current_dir)etc/apache/logs

etc/apache/httpd: apache/Makefile
	cd apache;\
	make;\
	make install
	sudo rm -f etc/apache/conf/httpd.conf

etc/apache/conf/httpd.conf:
	cp config/.apache.conf config/apache.conf
	sudo sed -i "s;{{dsu_www}};$(current_dir)www;" config/apache.conf
	cp config/apache.conf etc/apache/conf/httpd.conf

apache.o: etc/apache/logs etc/apache/httpd etc/apache/conf/httpd.conf
	cp apache/.libs/httpd apache.o
	



clean:
	rm -rf lighttpd1.4 nginx apache etc
	rm -f nginx.o lighttpd.o apache.o httpd
	rm -f config/nginx.conf config/apache.conf config/lighttpd.conf

