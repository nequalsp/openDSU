import requests
import time

while True:
	x = requests.get('http://localhost:3000/v1.html', timeout=5)
	print(x.text)
	time.sleep(1)
