# openDSU
Update to a different web server without disruption in service (notably Apache, Nginx and Lighttpd). The web server can be temporarily replaced until a security patch is provided, mitigating the vulnerability.

### Installation
Run **make build** followed by **make install**, where first both the shared library and executable is compiled and then saved in the /usr/local/lib/openDSU and /usr/local/bin/openDSU directory respectively.

### Usage
Either run your application with

* **LD_PRELOAD=/usr/local/lib/openDSU/libopenDSU.so** (LD_PRELOAD=/usr/local/lib/openDSU/libopenDSU.so ./nginx)

or use the executable that does this for you

* **openDSU** (openDSU ./nginx)

