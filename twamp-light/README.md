# TWAMP Light
This project is a C++ implementation of the TWAMP Light protocol defined in Appendix I of [RFC 5357](https://datatracker.ietf.org/doc/html/rfc5357) for precise two-way network latency measurement using stateless UDP packet reflection.
## Supported Modes
* TWAMP Light Controller/Sender
* TWAMP Light Responder/Reflector
## Installation
```
git clone https://github.com/gxkl7/twamp-light.git
mkdir build
cd build
cmake ..
make
```
## Usage
```
$ ./twamp_light --help

usage:

        IPv4 Address (Required)

        Port Number (Required)

        -f controller || reflector (Required)

        -c packet_count (controller mode only)

        -i interval_ms (controller mode only)

        -t timeout_ms (controller mode only)

-h, --help Help string

defaults: packet_count=3, interval=10ms, timeout=1000ms
```
## Example Output
### Example 1
```
controller_host$ ./twamp_light 10.0.0.246 1024 -f controller -c 5

Mode: controller
IPv4 Address: 10.0.0.246
Port Number: 1024

Packet count: 5
Interval (ms): 10
Timeout (ms): 1000

2025-10-09 23:06:16 Packet sent to 10.0.0.246
2025-10-09 23:06:16 Received response fron 10.0.0.246

2025-10-09 23:06:16 Seq: 844251247 Sender TS: 3688093390595083 Receiver TS: 549516258727833 RTT: 31.4325 ms

2025-10-09 23:06:16 Packet sent to 10.0.0.246
2025-10-09 23:06:16 Received response fron 10.0.0.246

2025-10-09 23:06:16 Seq: 3455039362 Sender TS: 3688093434617708 Receiver TS: 549516280484041 RTT: 5.96829 ms

2025-10-09 23:06:16 Packet sent to 10.0.0.246
2025-10-09 23:06:16 Received response fron 10.0.0.246

2025-10-09 23:06:16 Seq: 3213586098 Sender TS: 3688093453155041 Receiver TS: 549516298885125 RTT: 5.86604 ms

2025-10-09 23:06:16 Packet sent to 10.0.0.246
2025-10-09 23:06:16 Received response fron 10.0.0.246

2025-10-09 23:06:16 Seq: 2954996358 Sender TS: 3688093471584625 Receiver TS: 549516317247791 RTT: 6.5405 ms

2025-10-09 23:06:16 Packet sent to 10.0.0.246
2025-10-09 23:06:16 Received response fron 10.0.0.246

2025-10-09 23:06:16 Seq: 1911575075 Sender TS: 3688093489059583 Receiver TS: 549516337680041 RTT: 8.74667 ms

=========================================================================

Average RTT: 11.7108 ms

Jitter: 7.11178 ms

Loss Percentage: 0%     No of Missing Packets: 0

```
```
reflector_host$ ./twamp_light 10.0.0.246 1024 -f reflector

Mode: reflector
IPv4 Address: 10.0.0.246
Port Number: 1024

Reflector listening on 10.0.0.246:1024

2025-10-09 23:06:16 Packet received from 10.0.0.178
2025-10-09 23:06:16 Responded to 10.0.0.178

2025-10-09 23:06:16 Seq: 844251247 Sender TS: 3688093390595083 Receiver TS: 549516258727833

2025-10-09 23:06:16 Packet received from 10.0.0.178
2025-10-09 23:06:16 Responded to 10.0.0.178

2025-10-09 23:06:16 Seq: 3455039362 Sender TS: 3688093434617708 Receiver TS: 549516280484041

2025-10-09 23:06:16 Packet received from 10.0.0.178
2025-10-09 23:06:16 Responded to 10.0.0.178

2025-10-09 23:06:16 Seq: 3213586098 Sender TS: 3688093453155041 Receiver TS: 549516298885125

2025-10-09 23:06:16 Packet received from 10.0.0.178
2025-10-09 23:06:16 Responded to 10.0.0.178

2025-10-09 23:06:16 Seq: 2954996358 Sender TS: 3688093471584625 Receiver TS: 549516317247791

2025-10-09 23:06:16 Packet received from 10.0.0.178
2025-10-09 23:06:16 Responded to 10.0.0.178

2025-10-09 23:06:16 Seq: 1911575075 Sender TS: 3688093489059583 Receiver TS: 549516337680041
```
### Example 2
```
controller_host$ ./twamp_light 10.0.0.246 1024 -f controller -c 5 -i 10000

Mode: controller
IPv4 Address: 10.0.0.246
Port Number: 1024

Packet count: 5
Interval (ms): 10000
Timeout (ms): 1000

2025-10-09 23:10:13 Packet sent to 10.0.0.246
2025-10-09 23:10:13 Received response fron 10.0.0.246

2025-10-09 23:10:13 Seq: 3532341740 Sender TS: 3688330525552041 Receiver TS: 549753396840750 RTT: 34.0837 ms

2025-10-09 23:10:23 Packet sent to 10.0.0.246
2025-10-09 23:10:23 Received response fron 10.0.0.246

2025-10-09 23:10:23 Seq: 4176071720 Sender TS: 3688340564738708 Receiver TS: 549763412992000 RTT: 10.5417 ms

2025-10-09 23:10:33 Packet sent to 10.0.0.246
2025-10-09 23:10:33 Received response fron 10.0.0.246

2025-10-09 23:10:33 Seq: 2790776093 Sender TS: 3688350580458000 Receiver TS: 549773454494125 RTT: 33.4543 ms

2025-10-09 23:10:43 Packet sent to 10.0.0.246
2025-10-09 23:10:44 No response received (timeout) 10.0.0.246

2025-10-09 23:10:44 Packet sent to 10.0.0.246
2025-10-09 23:10:45 No response received (timeout) 10.0.0.246

=========================================================================

Average RTT: 26.0266 ms

Jitter: 23.2273 ms

Loss Percentage: 40%    No of Missing Packets: 2
```
### Example 3
```
controller_host$ ./twamp_light 10.0.0.246 1024 -f controller -c 1         

Mode: controller
IPv4 Address: 10.0.0.246
Port Number: 1024

Packet count: 1
Interval (ms): 10
Timeout (ms): 1000

2025-10-09 23:12:48 Packet sent to 10.0.0.246
2025-10-09 23:12:48Malformed TWAMP packet: undersized (received 19 bytes, expected 20) from 10.0.0.246

=========================================================================

Average RTT: 0 ms

Jitter: 0 ms

Loss Percentage: 100%   No of Missing Packets: 1

```
### Example 4
```
reflector_host$ ./twamp_light 10.0.0.246 1024 -f reflector

Mode: reflector
IPv4 Address: 10.0.0.246
Port Number: 1024

Reflector listening on 10.0.0.246:1024

2025-10-09 23:16:46 Malformed TWAMP packet: undersized (received 18 bytes, expected 20) from 10.0.0.178
```
