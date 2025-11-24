import socket
import time
import json

BROADCAST_IP = '255.255.255.255'
PORT = 63093
INTERVAL = 5  # seconds

# JSON payload
payload = {
    "App": "ForeFlight",
    "GDL90": {
        "port": 4000
    }
}
data = json.dumps(payload).encode('utf-8')

# Create UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

try:
    while True:
        sock.sendto(data, (BROADCAST_IP, PORT))
        print(f"Sent: {data}")
        time.sleep(INTERVAL)
finally:
    sock.close()
