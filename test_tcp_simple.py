#!/usr/bin/env python3
"""
Simple TCP server to test Android connectivity
"""

import socket
import threading

def tcp_server():
    port = 3000
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", port))
    server.listen(1)
    
    print(f"TCP server listening on port {port}...")
    
    try:
        while True:
            client, addr = server.accept()
            print(f"Connection from {addr}")
            data = client.recv(1024)
            print(f"Received: {data}")
            client.close()
    except KeyboardInterrupt:
        print("Server stopped")
    finally:
        server.close()

if __name__ == "__main__":
    tcp_server()