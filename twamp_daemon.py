#!/usr/bin/env python3
"""
TWAMP Daemon for BGP Latency-Based Path Selection
Reads next-hops from shared memory and measures latency using TWAMP Light
"""

import sys
import time
import struct
import socket
import mmap
import argparse
import signal
from datetime import datetime
from ctypes import Structure, c_uint8, c_uint32, c_uint64

# Constants
TWAMP_SHM_NAME = "/bgp_twamp_shm"
MAX_NEXTHOPS = 1024
DEFAULT_PROBE_CYCLE = 30  # seconds
DEFAULT_PACKET_COUNT = 3
DEFAULT_TIMEOUT = 1.0  # seconds
TWAMP_PORT = 862

# Shared memory structure (matches C struct)
class NexthopEntry(Structure):
    _fields_ = [
        ('addr', c_uint32),           # IPv4 address (network byte order)
        ('active', c_uint8),
        ('measured', c_uint8),
        ('padding', c_uint8 * 2),
        ('latency_ms', c_uint32),
        ('last_updated', c_uint64)
    ]

class TwampShm(Structure):
    _fields_ = [
        ('lock', c_uint8 * 40),       # pthread_mutex_t placeholder
        ('nh_count', c_uint32),
        ('sequence', c_uint32),
        ('padding', c_uint32),
        ('nexthops', NexthopEntry * MAX_NEXTHOPS)
    ]

# Global flag for graceful shutdown
running = True

def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    global running
    print("\n\nShutting down TWAMP daemon...")
    running = False

def ip_to_string(ip_int):
    """Convert 32-bit integer to IP string"""
    return socket.inet_ntoa(struct.pack('!I', ip_int))

def measure_twamp_light(target_ip, port=TWAMP_PORT, count=3, timeout=1.0):
    """
    Simple TWAMP Light implementation
    Sends UDP packets and measures round-trip time
    
    Returns: average latency in milliseconds, or None if failed
    """
    rtts = []
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(timeout)
        
        for i in range(count):
            # TWAMP Light packet format (simplified)
            # Sequence number (4 bytes) + Timestamp (8 bytes) + Padding (8 bytes)
            seq_num = i + 1
            timestamp = int(time.time() * 1_000_000)  # microseconds
            
            packet = struct.pack('!IQQ', seq_num, timestamp, 0)
            
            # Send packet and measure RTT
            start_time = time.perf_counter()
            
            try:
                sock.sendto(packet, (target_ip, port))
                data, addr = sock.recvfrom(1024)
                
                end_time = time.perf_counter()
                rtt_ms = (end_time - start_time) * 1000
                rtts.append(rtt_ms)
                
                print(f"  Packet {seq_num}: {rtt_ms:.2f} ms")
                
            except socket.timeout:
                print(f"  Packet {seq_num}: Timeout")
                continue
            
            # Small delay between packets
            if i < count - 1:
                time.sleep(0.01)
        
        sock.close()
        
        if rtts:
            avg_rtt = sum(rtts) / len(rtts)
            loss_pct = ((count - len(rtts)) / count) * 100
            print(f"  Average RTT: {avg_rtt:.2f} ms, Loss: {loss_pct:.0f}%")
            return avg_rtt
        else:
            print(f"  All packets lost")
            return None
            
    except Exception as e:
        print(f"  Error measuring {target_ip}: {e}")
        return None

def open_shared_memory():
    """Open and map shared memory"""
    try:
        # Open shared memory file
        shm_fd = open(TWAMP_SHM_NAME, 'r+b')
        
        # Memory map the file
        shm_map = mmap.mmap(shm_fd.fileno(), 0)
        
        print("TWAMP Daemon: Connected to shared memory")
        return shm_fd, shm_map
        
    except FileNotFoundError:
        print(f"Error: Shared memory {TWAMP_SHM_NAME} not found")
        print("Make sure BGP has created the shared memory.")
        print("Enable it with: bgp import check-latency")
        return None, None
    except Exception as e:
        print(f"Error opening shared memory: {e}")
        return None, None

def read_nexthop_count(shm_map):
    """Read next-hop count from shared memory"""
    # Skip pthread_mutex (40 bytes), read nh_count (4 bytes)
    offset = 40
    count = struct.unpack('I', shm_map[offset:offset+4])[0]
    return count

def read_nexthop(shm_map, index):
    """Read a single next-hop entry"""
    # Calculate offset: 40 (mutex) + 12 (nh_count, sequence, padding) + index * entry_size
    entry_size = 24  # sizeof(NexthopEntry)
    base_offset = 40 + 12
    offset = base_offset + (index * entry_size)
    
    # Read entry: addr(4) + active(1) + measured(1) + padding(2) + latency(4) + last_updated(8)
    data = shm_map[offset:offset+entry_size]
    addr, active, measured, pad1, pad2, latency, last_updated = struct.unpack('!IBBHIQ', data)
    
    return {
        'addr': addr,
        'active': active,
        'measured': measured,
        'latency_ms': latency,
        'last_updated': last_updated
    }

def write_nexthop_latency(shm_map, index, latency_ms):
    """Update next-hop latency in shared memory"""
    entry_size = 24
    base_offset = 40 + 12
    offset = base_offset + (index * entry_size)
    
    # Read current entry
    nh = read_nexthop(shm_map, index)
    
    # Update measured flag, latency, and timestamp
    measured = 1
    last_updated = int(time.time())
    
    # Write back: addr(4) + active(1) + measured(1) + padding(2) + latency(4) + last_updated(8)
    data = struct.pack('!IBBHIQ', 
                      nh['addr'], 
                      nh['active'], 
                      measured, 
                      0, 0,  # padding
                      latency_ms, 
                      last_updated)
    
    shm_map[offset:offset+entry_size] = data

def mark_nexthop_failed(shm_map, index):
    """Mark next-hop as failed (unmeasured)"""
    entry_size = 24
    base_offset = 40 + 12
    offset = base_offset + (index * entry_size)
    
    # Read current entry
    nh = read_nexthop(shm_map, index)
    
    # Update measured flag and set latency to max
    measured = 0
    latency_ms = 0xFFFFFFFF  # UINT32_MAX
    
    data = struct.pack('!IBBHIQ', 
                      nh['addr'], 
                      nh['active'], 
                      measured, 
                      0, 0,
                      latency_ms, 
                      nh['last_updated'])
    
    shm_map[offset:offset+entry_size] = data

def run_measurement_cycle(shm_map, packet_count):
    """Run one complete measurement cycle"""
    print("\n=== TWAMP Measurement Cycle ===")
    print(datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
    
    # Read next-hop count
    count = read_nexthop_count(shm_map)
    
    if count == 0:
        print("No next-hops to measure.")
        return
    
    print(f"Measuring {count} next-hops...")
    
    measured_count = 0
    
    # Measure each active next-hop
    for i in range(count):
        nh = read_nexthop(shm_map, i)
        
        if not nh['active']:
            continue
        
        ip_str = ip_to_string(nh['addr'])
        print(f"\nNext-hop {i+1}: {ip_str}")
        
        # Perform measurement
        latency = measure_twamp_light(ip_str, TWAMP_PORT, packet_count)
        
        if latency is not None:
            latency_ms = int(latency + 0.5)  # Round to nearest ms
            write_nexthop_latency(shm_map, i, latency_ms)
            measured_count += 1
            print(f"  ✓ Updated shared memory: {latency_ms} ms")
        else:
            mark_nexthop_failed(shm_map, i)
            print(f"  ✗ Marked as failed")
        
        # Small delay between measurements
        time.sleep(1)
    
    print(f"\nMeasurement cycle complete: {measured_count}/{count} next-hops measured successfully")

def main():
    global running
    
    parser = argparse.ArgumentParser(
        description='TWAMP measurement daemon for BGP latency-based path selection'
    )
    parser.add_argument('-c', '--cycle', type=int, default=DEFAULT_PROBE_CYCLE,
                       help=f'Probe cycle interval in seconds (default: {DEFAULT_PROBE_CYCLE})')
    parser.add_argument('-p', '--packets', type=int, default=DEFAULT_PACKET_COUNT,
                       help=f'Packets per measurement (default: {DEFAULT_PACKET_COUNT})')
    
    args = parser.parse_args()
    
    if args.cycle < 10:
        print("Error: Probe cycle must be >= 10 seconds")
        return 1
    
    if args.packets < 1 or args.packets > 100:
        print("Error: Packet count must be between 1 and 100")
        return 1
    
    print("=== TWAMP Measurement Daemon ===")
    print("BGP Latency-Based Path Selection\n")
    print(f"Configuration:")
    print(f"  Probe cycle: {args.cycle} seconds")
    print(f"  Packets per probe: {args.packets}")
    print(f"  TWAMP port: {TWAMP_PORT}\n")
    
    # Setup signal handler
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    # Open shared memory
    shm_fd, shm_map = open_shared_memory()
    if shm_map is None:
        return 1
    
    print("Starting measurement loop (Ctrl+C to stop)...\n")
    
    # Main measurement loop
    try:
        while running:
            run_measurement_cycle(shm_map, args.packets)
            
            if running:
                print(f"\nNext measurement in {args.cycle} seconds...")
                
                # Sleep with checks for shutdown
                for _ in range(args.cycle):
                    if not running:
                        break
                    time.sleep(1)
        
    finally:
        shm_map.close()
        shm_fd.close()
        print("Goodbye!")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())