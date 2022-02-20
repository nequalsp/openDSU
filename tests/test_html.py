import requests
import time

while True:
	x = requests.get('http://localhost:80/v1.html', timeout=5)
	x.close()
	print(x.text)
	time.sleep(1)
