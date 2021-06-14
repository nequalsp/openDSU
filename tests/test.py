#!/usr/bin/env python

import socket
import time
import subprocess
import signal
import os

TCP_IP = '127.0.0.1'
TCP_PORT = 3000

BUFFER_SIZE = 512
MESSAGE = "Hello"
WAIT_TIME = 1
UPDATE_TIME = 5

#proc1 = subprocess.Popen(['gnome-terminal -- ./tests/version_0/simple_select.o'], shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
#time.sleep(2)

ti = 0
while True:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((TCP_IP, TCP_PORT))
    s.send(MESSAGE.encode())
    data = s.recv(BUFFER_SIZE)
    s.close()
    time.sleep(WAIT_TIME) 
    print(round(time.perf_counter(),5), " : ", data.decode())
    
    #ti = ti + 1
    #if ti == UPDATE_TIME:
    #    print(time.perf_counter(), " : ", "Update")
    #    proc2 = subprocess.Popen(['gnome-terminal -- ./tests/version_1/simple_select.o'], shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    #elif ti == 5*UPDATE_TIME:
    #    os.killpg(os.getpgid(proc2.pid), signal.SIGTERM)
    #    exit()
