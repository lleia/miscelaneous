import os
import sys
from random import random as rand

if __name__ == '__main__':
	if len(sys.argv) != 2:
		print >> sys.stderr,'Usage: sample count'
		sys.exit(1)
	count = int(sys.argv[1])
	sampling_list = []
	id = 0
	for line in sys.stdin:
		item = line.rstrip('\r\n')
		if id < count:
			sampling_list.append(item)
			id += 1
			continue
		thres = count/float(id+1)
		if rand() < thres:
			replace_id = int(rand() * count)
			sampling_list[replace_id] = item
		id += 1
	for line in sampling_list:
		print >> sys.stdout,line
