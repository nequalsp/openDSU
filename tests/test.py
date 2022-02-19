#!/usr/bin/env python

import socket
import time
import subprocess
import signal
import os
import threading
import sys

TCP_IP = '127.0.0.1'
TCP_PORT = 3000

BUFFER_SIZE = 512
MESSAGE = "Hello"
WAIT_TIME = 1
UPDATE_TIME = 5


if len(sys.argv) > 1:
	TCP_PORT = int(sys.argv[1])


def remove_process(signal, frame): # ctr-c
    global processes
    global states
    global plock
    
    plock.acquire()
    print("Remove process ..." )
    if len(processes) == 0:
        print("Quit...")
        sys.exit(0)
    states[-1] = True
    plock.release()
    
    processes[-1].join()
   
    plock.acquire()
    del processes[-1]
    del states[-1]
    plock.release()

def add_non_persistant_process(signal, frame): # ctr-z
    global processes
    global states
    global plock
    
    plock.acquire()

    print("Add non-persistant process ..." )
    p = process(process_id = len(processes), persistant = False)
    processes.append(p)
    states.append(False)
    p.start()
    
    plock.release()
    

def add_persistant_process(signal, frame): # ctr-\
    global processes
    global states
    global plock
    
    plock.acquire()

    print("Add persistant process ..." )
    p = process(process_id = len(processes), persistant = True)
    processes.append(p)
    states.append(False)
    p.start()
    
    plock.release()


class process(threading.Thread):
   
    def __init__(self, process_id, persistant = False):
        threading.Thread.__init__(self)
        self.process_id = process_id
        self.persistant = persistant
    
    def run(self):
        
        if self.persistant:
            persistent(self.process_id)
        else:
            non_persistent(self.process_id)


def non_persistent(process_id):

    while True:
        
        if states[process_id]:
            return
        
        plock.acquire()
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((TCP_IP, TCP_PORT))
        s.send(MESSAGE.encode())
        data = s.recv(BUFFER_SIZE) 
        print("non-persistant", "%.5f" % time.perf_counter(), " : ", data.decode(), flush=True)
        plock.release()
        time.sleep(WAIT_TIME)

def persistent(process_id):

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((TCP_IP, TCP_PORT))
    while True:
        
        if states[process_id]:
            break

        plock.acquire()
        s.send(MESSAGE.encode())
        data = s.recv(BUFFER_SIZE)
        print("persistant    ", "%.5f" % time.perf_counter(), " : ", data.decode(), flush=True)
        plock.release()
        time.sleep(WAIT_TIME)
     
    s.close()

processes = []
states = []
plock = threading.Lock()

signal.signal(signal.SIGINT, remove_process)
signal.signal(signal.SIGQUIT, add_non_persistant_process)
signal.signal(signal.SIGTSTP, add_persistant_process)

while True:
    time.sleep(WAIT_TIME)
