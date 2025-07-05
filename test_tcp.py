import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.sendto(b'Hello from Python', ('192.168.1.103', 8080)) 
sock.close()
print('Python UDP packet sent to 192.168.1.103:8080') 