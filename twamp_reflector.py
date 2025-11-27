#!/usr/bin/env python3
"""
Simple TWAMP Light Reflector
Listens on all interfaces (0.0.0.0) and reflects packets back
"""

import socket
import struct
import time
import signal
import sys

TWAMP_PORT = 862
running = True

def signal_handler(sig, frame):
    global running
    print("\nShutting down reflector...")
    running = False

def main():
    global running
    
    print("=== TWAMP Light Reflector ===")
    print(f"Listening on 0.0.0.0:{TWAMP_PORT} (all interfaces)\n")
    
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    # Bind to 0.0.0.0 to listen on ALL interfaces
    sock.bind(('0.0.0.0', TWAMP_PORT))
    sock.settimeout(1.0)  # 1 second timeout for checking running flag
    
    print("Reflector started successfully")
    print("Press Ctrl+C to stop\n")
    
    packet_count = 0
    
    try:
        while running:
            try:
                # Receive packet
                data, addr = sock.recvfrom(1024)
                
                if len(data) < 20:
                    continue
                
                packet_count += 1
                
                # Parse sequence number from packet
                seq_num = struct.unpack('!I', data[0:4])[0]
                
                # Echo packet back immediately
                sock.sendto(data, addr)
                
                print(f"[{packet_count}] Reflected to {addr[0]}:{addr[1]} (seq: {seq_num})")
                
            except socket.timeout:
                # Timeout is normal, just check if we should continue
                continue
            except Exception as e:
                if running:
                    print(f"Error processing packet: {e}")
    
    finally:
        sock.close()
        print(f"\nReflector stopped. Total packets reflected: {packet_count}")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())