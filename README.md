# openDSU
Update to a different web server without disruption in service (notably Apache, Nginx and Lighttpd). The web server can be temporarily replaced until a security patch is provided, mitigating the vulnerability. This can be done without any changes to the code and compilation.



### Installation
Run **make build** followed by **make install**, where first both the shared library and executable is compiled and then saved in the /usr/local/lib/openDSU and /usr/local/bin/openDSU directory respectively.

### Usage
openDSU is a shared library that must be initialized before the main program starts. Either run your application with

* **LD_PRELOAD=/usr/local/lib/openDSU/libopenDSU.so** (Ex: LD_PRELOAD=/usr/local/lib/openDSU/libopenDSU.so ./nginx)

or use the executable that does this for you
* **openDSU** (Ex: openDSU ./nginx)

to start the first or a newer version of the web server. Different version communicate using shared memory and Unix domain sockets and exchange binded sockets for a smooth transition to the newer version.

<br/>[Benchmark](benchmark/README.md)<br/><br/>
[Tests](tests/README.md)

