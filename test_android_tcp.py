#!/usr/bin/env python3
"""
Simple TCP server for Android testing
"""

import socket
import threading

def tcp_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", 8080))
    server.listen(1)
    
    print("TCP server listening on 0.0.0.0:8080...")
    print("Waiting for Android connection...")
    
    try:
        while True:
            client, addr = server.accept()
            print(f"✓ Connection from {addr}")
            data = client.recv(1024)
            print(f"✓ Received: {data}")
            client.send(b"Hello from PC")
            client.close()
            print("Connection closed")
    except KeyboardInterrupt:
        print("Server stopped")
    finally:
        server.close()

if __name__ == "__main__":
    tcp_server()