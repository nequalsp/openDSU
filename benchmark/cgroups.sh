# Throttle during benchmarking.

mkdir -p /sys/fs/cgroup/memory/openDSU
sudo cgset -r memory.limit_in_bytes=100000000 openDSU
sudo cgset -r memory.limit_in_bytes=32000000000 openDSU

mkdir /sys/fs/cgroup/cpu/openDSU
sudo cgset -r cpu.cfs_period_us=10000 openDSU
sudo cgset -r cpu.cfs_quota_us =2500 openDSU

sudo cat /sys/fs/cgroup/cpu/openDSU/cpu.cfs_period_us
sudo cat /sys/fs/cgroup/cpu/openDSU/cpu.cfs_quota_us
sudo cat /sys/fs/cgroup/memory/openDSU/memory.limit_in_bytes

mkdir /sys/fs/cgroup/cpuset/openDSU
sudo cgset -r cpuset.cpus=4 openDSU
/sys/fs/cgroup/cpuset/openDSU/cpuset.cpus

sudo cgset -r cpu.cfs_period_us=-1 openDSU
sudo cgset -r cpu.cfs_quota_us=-1 openDSU
	
#sudo cgclassify -g memory:openDSU 9491
#sudo ps -o cgroup 12530 | cat
#sudo cgexec -g cpu,memory:openDSU ./nginx.o
#error_log /home/anoniem/Desktop/openDSU/benchmark/etc/nginx/logs/debug.log debug;


./wrk.o -t 4 -c 32 -d 30s http://localhost/v1.html
ab -n 2000000 -H "Connection: close" -s 60 -c 8 http://localhost/v1.html
cgexec -g cpu,memory:openDSU ./lighttpd.o -f config/lighttpd.conf

<IfModule mpm_event_module>
	ThreadsPerChild 16
	ServerLimit 4
	MaxRequestWorkers 64
	AsyncRequestWorkerFactor 2
</IfModule>

<IfModule mpm_prefork_module>
	StartServers 1
	MinSpareServers 1
	MaxSpareServers 1
	MaxClients 200 #must be customized
	ServerLimit 200 #must be customized
	MaxRequestsPerChild 100
</IfModule>

<IfModule mpm_worker_module>
	ServerLimit         2
	StartServers        2
	MaxRequestWorkers   2
	MinSpareThreads     2
	MaxSpareThreads     2
	ThreadsPerChild     2
</IfModule>
