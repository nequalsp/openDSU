import requests
import time

x = requests.get('http://localhost:80/v1.html', timeout=5)
print(x.text)

time.sleep(120)
print("Close connection")
x.close()
