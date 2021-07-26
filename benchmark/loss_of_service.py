import pandas as pd
import matplotlib.pyplot as plt
import subprocess
import getopt
import sys
import time
import os


run = False


try:
	opts, args = getopt.getopt(sys.argv[1:], "r", ["run"])
except getopt.GetoptError as err:
	print(err)
	sys.exit(2)
	

for o, a in opts:
	if o == "-r":
		run = True


if run:
	
	with open("loss_of_service_updates.plt", "w") as f:
		f.write("seconds\n")
		f.flush()
		
		subprocess.run(["pkill", "nginx"])

		print('Start')
		subprocess.run(["openDSU", "./nginx.o"])
		p = subprocess.Popen(["ab", "-g", "loss_of_service.plt", "-n", "2000000", "-c", "1000", "http://localhost/v1.html"])
		time.sleep(10)
		
		if os.fork() == 0:
			for i in range(0, 3):
				
				if os.fork() != 0: # Change pid
					sys.exit(1)
				os.setpgid(0, 0)
				
				print('Update...')		
				f.write(str(int(time.time())) + "\n")
				f.flush()
				subprocess.run(["openDSU", "./nginx.o"])
				time.sleep(10)
			sys.exit(1)	
		else:
			p.wait()			
			subprocess.run(["pkill", "nginx"])
			print('End')



df1 = pd.read_csv('loss_of_service.plt', sep='\t', index_col=0)
df2 = pd.read_csv('loss_of_service_updates.plt', sep='\t')

df1.sort_values(by=['seconds'], inplace=True)

f = {'seconds':'first', 'ctime':'mean', 'dtime':'mean', 'ttime':'mean', 'wait':'mean'}
df1 = df1.groupby(['seconds']).agg(f)

corr1 = df1['seconds'].iloc[0]
df1['seconds'] -= corr1
df2['seconds'] -= corr1

fig = plt.figure()

plt.plot(df1['seconds'][5:], df1['ttime'][5:], 'k-', label='openDSU (Nginx)')

maximum= max(df1['ttime'][5:])
for index, row in df2.iterrows():
	plt.plot([row['seconds'], row['seconds']], [0, maximum], 'k--') # Horizontal line

plt.ylim((0,None))
plt.xlabel('Duration (s)')
plt.ylabel('Latency (ms)')

plt.legend()

plt.show()


