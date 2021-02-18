import socket
import time

TCP_IP = '127.0.0.1'
TCP_PORT = 3000

BUFFER_SIZE = 512
MESSAGE = "Hello"
WAIT_TIME = 1

while True:
	s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	s.connect((TCP_IP, TCP_PORT))
	s.send(MESSAGE.encode())
	data = s.recv(BUFFER_SIZE)
	s.close()
	time.sleep(WAIT_TIME)
	print("received data:", data.decode())
