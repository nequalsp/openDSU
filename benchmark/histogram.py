import pandas as pd
import matplotlib.pyplot as plt
import subprocess
import getopt
import sys
import numpy as np
from scipy import stats

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



df1 = pd.read_csv('data1.plt', sep='\t', index_col=0)
df2 = pd.read_csv('data2.plt', sep='\t', index_col=0)


df1_len = df1['ttime'].count()
df2_len = df2['ttime'].count()

f = {'ttime': ['first', 'count']}
df1 = df1.groupby(['ttime']).agg(f)
df2 = df2.groupby(['ttime']).agg(f)

df1.sort_values(by=[('ttime','first')], ascending=False)
t = 0.0
end = 0
for index, row in df1.iterrows():
	t += row[('ttime','count')]
	if t > 0.025*df1_len:
		end = df1_len - index

df1.sort_values(by=[('ttime','first')], ascending=True)
t = 0.0
start = 0
for index, row in df1.iterrows():
	t += row[('ttime','count')]
	if t > 0.025*df1_len:
		start = index

print(df1[('ttime','count')].count())
df1 = df1.iloc[start:end]






# Plot the data
fig = plt.figure()

plt.plot(df1[('ttime','first')], df1[('ttime','count')], label='nginx')
#plt.plot(df2['seconds'], df2['ttime'], label='openDSU')
#plt.hist(df1['ttime'], density = True)


#plt.ylim((0,None))
#plt.xlabel('Duration (s)')
#plt.ylabel('Latency (ms)')

#plt.legend()

plt.show()


