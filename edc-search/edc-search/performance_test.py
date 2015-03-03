#!/usr/bin/env python

import subprocess
import sys
import time

result_file = open('results.txt', 'w')

#for i in range(15,255,15):
for i in range(15,16,1):
	#sockets = i / 15
	sockets = 1
	start_socket = 90
	print 'Executing with ' + str(i) + ' Threads ' + ' on ' + str(sockets) + ' Sockets'
	t0 = time.time()
	subprocess.call(["./run.sh", str(start_socket),str(sockets), str(i)])
	execution_time = time.time() - t0
	result_file.write('Threads: ' + str(i) + ' Execution time: ' + str(execution_time) + '\n') 

result_file.close()
