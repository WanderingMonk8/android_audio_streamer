# Simple UDP echo server on PC 
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('0.0.0.0', 9999))
print("UDP Echo server listening on port 9999...")
while True:
    try:
        data, addr = sock.recvfrom(1024) 
        print(f"Received from {addr}: {data}") 
        sock.sendto(b"Echo: " + data, addr) 
    except KeyboardInterrupt:
        break
sock.close()