import subprocess
import getopt
import sys
import re
import os


run = False


full_path = os.path.realpath(__file__)
path, filename = os.path.split(full_path)

webservers = ["nginx", "lighttpd"]
flags = [[], ["-f", path + "/config/lighttpd.conf"], []]

try:
	opts, args = getopt.getopt(sys.argv[1:], "r", ["run"])
except getopt.GetoptError as err:
	print(err)
	sys.exit(2)
	

for o, a in opts:
	if o == "-r":
		run = True



if run:
	with open("comparison.txt", "w") as f:
		i = 0
		for webserver in webservers:
			print("Start " + webserver + "...")
			f.write(webserver + "\n")
			f.flush()
			subprocess.run(["./" + webserver + ".o"] + flags[i])
			subprocess.run(["./wrk.o", "-t", "4", "-c", "1000", "-d", "30s", "http://localhost/v1.html"], stdout=f)
			subprocess.run(["pkill", webserver])
			print("Start " + webserver + " with openDSU...")
			f.write("openDSU " + webserver + "\n")
			f.flush()
			subprocess.run(["openDSU", "./" + webserver + ".o"] + flags[i])
			subprocess.run(["./wrk.o", "-t", "4", "-c", "1000", "-d", "30s", "http://localhost/v1.html"], stdout=f)
			subprocess.run(["pkill", webserver])
			print("End")
			i += 1


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
		m = re.match("\W*Latency *\d+\.\d+", line)
		if m != None:
			row[1] = re.findall("\d+\.\d+", line)[0]

		# Req/Sec    36.03k     3.12k   61.36k    91.05%
		m = re.match("\W*Req/Sec *\d+\.\d+", line)
		if m != None:
			row[2] = re.findall("\d+\.\d+", line)[0]

	if len(row) > 0:
		data.append(row)

print(data)
			
