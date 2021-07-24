import pandas as pd
import matplotlib.pyplot as plt
import subprocess
import getopt
import sys


run = False

try:
	opts, args = getopt.getopt(sys.argv[1:], "r", ["run"])
except getopt.GetoptError as err:
	print(err)
	sys.exit(2)
	

for o, a in opts:
	if o == "-r":
		run = True

# Run
if run:
	subprocess.run(["./nginx.o"])
	subprocess.run(["ab", "-g", "data1.plt", "-n", "2000000", "-c", "1000", "http://localhost/v1.html"])
	subprocess.run(["pkill", "nginx"])
	subprocess.run(["openDSU", "./nginx.o"])
	subprocess.run(["ab", "-g", "data2.plt", "-n", "2000000", "-c", "1000", "http://localhost/v1.html"])
	subprocess.run(["pkill", "nginx"])


# Data
df1 = pd.read_csv('data1.plt', sep='\t', index_col=0)
df2 = pd.read_csv('data2.plt', sep='\t', index_col=0)

# Sort based on the second column, time in seconds.
df1.sort_values(by=['seconds'], inplace=True)
df2.sort_values(by=['seconds'], inplace=True)

# Group by second.
f = {'seconds':'first', 'ctime':'mean', 'dtime':'mean', 'ttime':'mean', 'wait':'mean'}
df1 = df1.groupby(['seconds']).agg(f)
df2 = df2.groupby(['seconds']).agg(f)

# Map seconds to starting from 0
corr1 = df1['seconds'].iloc[0]
corr2 = df2['seconds'].iloc[0]
df1['seconds'] -= corr1
df2['seconds'] -= corr2

# Plot the data
fig = plt.figure()

plt.plot(df1['seconds'], df1['ttime'], label='nginx')
plt.plot(df2['seconds'], df2['ttime'], label='openDSU')

plt.ylim((0,None))
plt.xlabel('Duration (s)')
plt.ylabel('Latency (ms)')

plt.legend()

plt.show()


