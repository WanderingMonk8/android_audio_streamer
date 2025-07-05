#!/usr/bin/env python3
"""
Simple UDP test to verify PC receiver is working
"""

import socket
import struct
import time

def send_test_packets():
    print("Sending test packets to PC receiver...")
    
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    pc_ip = "192.168.1.103"
    pc_port = 8080
    
    try:
        for i in range(1, 11):
            # Create AudioPacket format: seq_id(4) + timestamp(8) + payload_size(4) + payload
            seq_id = i
            timestamp = int(time.time() * 1000000000)  # nanoseconds
            payload = bytes([i] * 100)  # 100 bytes of test data
            payload_size = len(payload)
            
            # Pack in little endian format
            packet = struct.pack('<I', seq_id)  # sequence_id (4 bytes)
            packet += struct.pack('<Q', timestamp)  # timestamp (8 bytes)
            packet += struct.pack('<I', payload_size)  # payload_size (4 bytes)
            packet += payload  # payload
            
            # Send packet
            sock.sendto(packet, (pc_ip, pc_port))
            print(f"Sent packet {i}: seq={seq_id}, size={len(packet)} bytes")
            
            time.sleep(0.1)  # 100ms delay
            
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()
    
    print("Test complete!")

if __name__ == "__main__":
    send_test_packets()