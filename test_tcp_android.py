#!/usr/bin/env python3
"""
Test TCP connectivity from Android perspective
"""

import socket

def test_tcp_connection():
    print("Testing TCP connection to PC...")
    
    try:
        # Create TCP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        
        # Try to connect to PC
        result = sock.connect_ex(("192.168.1.103", 8080))
        
        if result == 0:
            print("✅ TCP connection successful!")
            sock.send(b"Hello from Python TCP test")
        else:
            print(f"❌ TCP connection failed: {result}")
            
        sock.close()
        
    except Exception as e:
        print(f"❌ TCP test failed: {e}")

if __name__ == "__main__":
    test_tcp_connection()