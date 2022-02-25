import pandas as pd
import matplotlib.pyplot as plt
import subprocess
import getopt
import sys
import time
import os
import re


comparison = False
round_robin = False

full_path = os.path.realpath(__file__)
path, filename = os.path.split(full_path)

webservers = ["nginx", "apache", "lighttpd"]

def start_nginx():
	subprocess.run(["cgset", "-r", "cpu.cfs_quota_us=10000", "openDSU"])
	subprocess.run(["cgexec", "-g", "cpu,memory:openDSU", "openDSU", "./nginx.o"])
	
def start_lighttpd():
	subprocess.run(["cgset", "-r", "cpu.cfs_quota_us=5000", "openDSU"])
	#subprocess.run(["openDSU", "./lighttpd.o", "-f", path + "/config/lighttpd.conf"])
	
	subprocess.run(["cgexec", "-g", "cpu,memory:openDSU", "openDSU", "./lighttpd.o -f " + path + "/config/lighttpd.conf"])

def start_apache():
	subprocess.run(["cgset", "-r", "cpu.cfs_quota_us=20000", "openDSU"])
	subprocess.run(["cgexec", "-g", "cpu,memory:openDSU", "openDSU", "./apache.o -f " + path + "/config/apache.conf"])

def start_web_server(name):
	if name == "nginx":
		start_nginx()
	elif name == "lighttpd":
		start_lighttpd()
	elif name == "apache":
		start_apache()

def terminate_web_servers():
	print("Terminate running webservers...")
	for webserver in webservers:
		subprocess.run(["pkill", webserver])

def start_apache_benchmark(f):
	return subprocess.Popen(["ab", "-g", f, "-n", str(REQUESTS), "-H", "Connection: close", "-s", "60", "-c", "8", "http://localhost/v1.html"])

def start_wrk_benchmark(f):
	subprocess.run(["./wrk.o", "-t", "8", "-c", "64", "-d", "30s", "http://localhost/v1.html"], stdout=f)

try:
	opts, args = getopt.getopt(sys.argv[1:], "cr", ["run"])
except getopt.GetoptError as err:
	print(err)
	sys.exit(2)
	

for o, a in opts:
	if o == "-c":
		comparison = True
	if o == "-r":
		round_robin = True


if comparison:
	
	WAIT_COMP = 15
	terminate_web_servers()
	
	with open("comparison.txt", "w") as f:

		print("CPU bounded")
		
		print("Start nginx")
		f.write("Nginx\n")
		f.flush()
		subprocess.run(["cgset", "-r", "cpu.cfs_quota_us=5000", "openDSU"])
		subprocess.run(["cgexec", "-g", "cpu,memory:openDSU", "./nginx.o"])
		start_wrk_benchmark(f)
		terminate_web_servers()
		time.sleep(WAIT_COMP)

		print("Start openDSU nginx")
		f.write("openDSU nginx\n")
		f.flush()
		subprocess.run(["cgset", "-r", "cpu.cfs_quota_us=5000", "openDSU"])
		subprocess.run(["cgexec", "-g", "cpu,memory:openDSU", "openDSU", "./nginx.o"])
		start_wrk_benchmark(f)
		terminate_web_servers()
		time.sleep(WAIT_COMP)

		print("Start lighttpd")
		f.write("lighttpd\n")
		f.flush()
		subprocess.run(["cgset", "-r", "cpu.cfs_quota_us=2500", "openDSU"])
		subprocess.run(["cgexec", "-g", "cpu,memory:openDSU", "./lighttpd.o", "-f", "config/lighttpd.conf"])
		start_wrk_benchmark(f)
		terminate_web_servers()
		time.sleep(WAIT_COMP)

		print("Start openDSU lighttpd")
		f.write("openDSU lighttpd\n")
		f.flush()
		subprocess.run(["cgset", "-r", "cpu.cfs_quota_us=2500", "openDSU"])
		subprocess.run(["cgexec", "-g", "cpu,memory:openDSU", "openDSU", "./lighttpd.o -f config/lighttpd.conf"])
		start_wrk_benchmark(f)
		terminate_web_servers()
		time.sleep(WAIT_COMP)

		print("Memory bounded")
		
		print("start nginx")
		f.write("nginx\n")
		f.flush()
		subprocess.run(["cgset", "-r", "cpu.cfs_quota_us=-1", "openDSU"])
		subprocess.run(["cgset", "-r", "memory.limit_in_bytes=100000000", "openDSU"])
		subprocess.run(["cgexec", "-g", "cpu,memory:openDSU", "./nginx.o"])
		start_wrk_benchmark(f)
		terminate_web_servers()
		time.sleep(WAIT_COMP)

		print("Start openDSU nginx")
		f.write("openDSU nginx\n")
		f.flush()
		subprocess.run(["cgset", "-r", "cpu.cfs_quota_us=-1", "openDSU"])
		subprocess.run(["cgset", "-r", "memory.limit_in_bytes=100000000", "openDSU"])
		subprocess.run(["cgexec", "-g", "cpu,memory:openDSU", "openDSU", "./nginx.o"])
		start_wrk_benchmark(f)
		terminate_web_servers()
		time.sleep(WAIT_COMP)

		print("Start lighttpd")
		f.write("lighttpd\n")
		f.flush()
		subprocess.run(["cgset", "-r", "cpu.cfs_quota_us=-1", "openDSU"])
		subprocess.run(["cgset", "-r", "memory.limit_in_bytes=100000000", "openDSU"])
		subprocess.run(["cgexec", "-g", "cpu,memory:openDSU", "./lighttpd.o", "-f", "config/lighttpd.conf"])
		start_wrk_benchmark(f)
		terminate_web_servers()
		time.sleep(WAIT_COMP)

		print("Start openDSU lighttpd")
		f.write("lighttpd\n")
		f.flush()
		subprocess.run(["cgset", "-r", "cpu.cfs_quota_us=-1", "openDSU"])
		subprocess.run(["cgset", "-r", "memory.limit_in_bytes=100000000", "openDSU"])
		subprocess.run(["cgexec", "-g", "cpu,memory:openDSU", "openDSU", "./lighttpd.o -f config/lighttpd.conf"])
		start_wrk_benchmark(f)
		terminate_web_servers()
		time.sleep(WAIT_COMP)

		
if round_robin:
	REQUESTS = 2000000
	ROUND_ROBIN_TIME = 10
	
		
	terminate_web_servers()
      
	with open("round_robin_update.plt", "w") as f:
		f.write("seconds\n")
		f.flush()
		
		print('Start nginx')
		start_nginx()
		print('Start benchmarking')
		p = start_apache_benchmark("round_robin.plt")
		time.sleep(ROUND_ROBIN_TIME + 5)

		print('Update to lighttpd')
		start_lighttpd()
		f.write(str(int(time.time())) + "\n")
		f.flush()
		time.sleep(ROUND_ROBIN_TIME)

		print('Update to nginx')
		start_nginx()
		f.write(str(int(time.time())) + "\n")
		f.flush()
		time.sleep(ROUND_ROBIN_TIME)

		print('Update to lighttpd')
		start_lighttpd()
		f.write(str(int(time.time())) + "\n")
		f.flush()
		time.sleep(ROUND_ROBIN_TIME)
				
		p.wait()
		
		print('Terminate webservers')
		#terminate_web_servers()
		
		print('End')


if exists("round_robin.plt") and exists("round_robin_update.plt"):
	f = {'seconds':'first', 'ctime':'mean', 'dtime':'mean', 'ttime':'mean', 'wait':'mean'}
	df1 = pd.read_csv('round_robin.plt', sep='\t', index_col=0)
	df2 = pd.read_csv('round_robin_update.plt', sep='\t')
	df1.sort_values(by=['seconds'], inplace=True)
	#df1 = df1.groupby(['seconds']).agg(f)
	df1 = df1.groupby(['seconds']).size().reset_index(name='counts')
	corr1 = df1['seconds'].iloc[0]
	df1['seconds'] -= corr1
	df2['seconds'] -= corr1
	maximum= max(df1['counts'][5:45])

	plt.plot(df1['seconds'][5:45], df1['counts'][5:45], 'k-', label='openDSU')
	for index, row in df2.iterrows():
		plt.plot([row['seconds'], row['seconds']], [0, maximum], 'k--') # Horizontal line
	plt.ylim((0,None))
	plt.ylabel('Troughput in request/second')
	plt.xlabel('Time in seconds')
	plt.text(7, 1000, "Nginx")
	plt.text(18, 1000, "Lighttpd")
	plt.text(28, 1000, "Nginx")
	plt.text(38, 1000, "Lighttpd")
	plt.show()


if exists("round_robin.plt") and exists("round_robin_update.plt"):
	data = []
	row = []
	with open("comparison.txt", "r") as f:
		
		for line in f:
			
			for webserver in webservers:
				m = re.match("\W*" + webserver, line)
				if m != None:
					if len(row) > 0:
						data.append(row)
					row = [webserver, 0, 0]
				m = re.match("\W*openDSU *" + webserver, line)
				if m != None:
					if len(row) > 0:
						data.append(row)
					row = ["openDSU " + webserver, 0, 0]
			
			# Latency    12.13ms   54.33ms   1.23s    98.67%
			m = re.match("\W*Latency *\d+\.\d+[A-Za-z]{2}", line)
			if m != None:
				row[1] = re.findall("\d+\.\d+[A-Za-z]{2}", line)[0]

			# Req/Sec    36.03k     3.12k   61.36k    91.05%
			m = re.match("\W*Req/Sec *\d+\.\d+[A-Za-z]{1}", line)
			if m != None:
				row[2] = re.findall("\d+\.\d+[A-Za-z]{1}", line)[0]

		if len(row) > 0:
			data.append(row)

		print(data)

sys.exit(0)


fig, axes = plt.subplots(1, 1)


plt.plot(df1['seconds'][5:], df1['ttime'][5:], 'k-', label='openDSU (Nginx)')


maximum= max(df1['ttime'][5:])
for index, row in df2.iterrows():
	plt.plot([row['seconds'], row['seconds']], [0, 
], 'k--') # Horizontal line
plt.ylim((0,None))
plt.xlabel('Duration (s)')
plt.ylabel('Latency (ms)')
plt.legend()
plt.title(label=str(int(REQUESTS/max(df1['seconds']))) + " req/sec during " + str(UPDATES) + " updates")

print(data)


#tbl = axes[1].table(cellText=data, loc="center", colLoc='left', rowLoc='left', cellLoc='left', colLabels= ['', 'latency', 'req/sec'])
#axes[1].axis("off")
#axes[1].set_title(label="Comparison without implementation")
#tbl.auto_set_font_size(False)
#tbl.set_fontsize(14)

plt.show()


