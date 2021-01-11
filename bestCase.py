#!/usr/bin/env python3
import os 
import sys
import time

RUN_BY_WINDOW = 10 #how many time we compute the throughput for each size of window
fichier1 = open("perf.txt", "w")
fichier1.write("")
fichier1.close()
fichier = open("perf.txt", "r")
res = open("res_perf.txt", "a")
throughput = 0
perf = {}
max = "0"
timeout = 0
timeout_toadd = 0
perf[max] = 0
window = sys.argv[1]

for j in range(RUN_BY_WINDOW):
    os.system("./client2 192.168.122.1 7890 ospf_opt.pdf")
    line = fichier.readline()
    find_timeout = False
    find_throughput = False
    while(not find_throughput or not find_timeout):
        if len(line.split() )>=3:
            if not find_throughput and line.split()[0] == "throughput" :
                to_add = line.split()[2]
                find_throughput = True
            if not find_timeout and line.split()[1] == "timeout" :
                timeout_toadd =  line.split()[3]
                find_timeout = True
        line = fichier.readline()

    throughput += float(to_add)
    timeout += int(timeout_toadd)

res.write(window + " > " + str(throughput/RUN_BY_WINDOW) + "MB/s | Timeout = "+ str(timeout/RUN_BY_WINDOW) +"\n" )

fichier.close()
