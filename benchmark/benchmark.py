import pandas as pd
import matplotlib.pyplot as plt
import subprocess
import getopt
import sys
import time
import os
import re


loss_of_service = False
comparison = False

full_path = os.path.realpath(__file__)
path, filename = os.path.split(full_path)

webservers = ["nginx", "lighttpd", "apache"]
flags = [[], ["-f", path + "/config/lighttpd.conf"], []]
REQUESTS = 2000000
UPDATES = 3

try:
	opts, args = getopt.getopt(sys.argv[1:], "lc", ["run"])
except getopt.GetoptError as err:
	print(err)
	sys.exit(2)
	

for o, a in opts:
	if o == "-l":
		loss_of_service = True
	if o == "-c":
		comparison = True


if loss_of_service or comparison:
	
	print("Terminate running webservers...")
	for webserver in webservers:
		subprocess.run(["pkill", webserver])

	time.sleep(2)


if loss_of_service:
	
	with open("loss_of_service_updates.plt", "w") as f:
		f.write("seconds\n")
		f.flush()
		
		subprocess.run(["pkill", "lighttpd"])

		print('Start...')
		subprocess.run(["openDSU", "./lighttpd.o", "-f", path + "/config/lighttpd.conf"])
		p = subprocess.Popen(["ab", "-g", "loss_of_service.plt", "-n", str(REQUESTS), "-r", "-s", "750", "-c", "32", "http://localhost/v1.html"])
		time.sleep(10)
		
		if os.fork() == 0:
			for i in range(0, UPDATES):
				
				if os.fork() != 0: # Change pid
					sys.exit(1)
				os.setpgid(0, 0)
				
				print('Update...')		
				f.write(str(int(time.time())) + "\n")
				f.flush()
				subprocess.run(["openDSU", "./lighttpd.o", "-f", path + "/config/lighttpd.conf"])
				time.sleep(10)
			sys.exit(1)	
		else:
			p.wait()			
			subprocess.run(["pkill", "lighttpd"])
	
	print('End')


if comparison:

	with open("comparison.txt", "w") as f:
		i = 0
		for webserver in webservers:
			
			print("Start " + webserver + "...")
			f.write(webserver + "\n")
			f.flush()
			subprocess.run(["./" + webserver + ".o"] + flags[i])
			subprocess.run(["./wrk.o", "-t", "4", "-c", "32", "-d", "30s", "http://localhost/v1.html"], stdout=f)
			subprocess.run(["pkill", webserver])
			time.sleep(2)
			
			print("Start " + webserver + " with openDSU...")
			f.write("openDSU " + webserver + "\n")
			f.flush()
			subprocess.run(["openDSU", "./" + webserver + ".o"] + flags[i])
			subprocess.run(["./wrk.o", "-t", "4", "-c", "32", "-d", "30s", "http://localhost/v1.html"], stdout=f)
			subprocess.run(["pkill", webserver])
			time.sleep(2)
			
			i += 1
	
	print("End")




df1 = pd.read_csv('loss_of_service.plt', sep='\t', index_col=0)
df2 = pd.read_csv('loss_of_service_updates.plt', sep='\t')

df1.sort_values(by=['seconds'], inplace=True)

f = {'seconds':'first', 'ctime':'mean', 'dtime':'mean', 'ttime':'mean', 'wait':'mean'}
df1 = df1.groupby(['seconds']).agg(f)

corr1 = df1['seconds'].iloc[0]
df1['seconds'] -= corr1
df2['seconds'] -= corr1




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




fig, axes = plt.subplots(1, 1)


plt.plot(df1['seconds'][5:], df1['ttime'][5:], 'k-', label='openDSU (Nginx)')


maximum= max(df1['ttime'][5:])
for index, row in df2.iterrows():
	plt.plot([row['seconds'], row['seconds']], [0, maximum], 'k--') # Horizontal line
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


