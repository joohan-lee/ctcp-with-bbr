# plot.bdp.py
# CSCI551
# How to use: python plot_bdp.py [path_to_log_file]

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import sys 
	
if len(sys.argv) != 2:
	print('How to use: python plot_bdp.py [path_to_log_file]')

fin = open(sys.argv[1],'r')
line = fin.readline()

ts = []
bdp = []
while line != '':
	data = line.strip().split(',')
	if len(data) == 2:
		ts.append(int(data[0]))
		bdp.append(int(data[1]))

	line = fin.readline()

plt.plot(ts, bdp)
plt.xlabel('Timestamp')
plt.ylabel('BDP')
# plt.show()
plt.savefig('bdp_plot.png')

fin.close()
print('done')	
