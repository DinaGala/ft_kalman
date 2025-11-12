#!/usr/bin/env python3
"""
Simple UDP test harness that mimics the minimal behavior of imu-sensor-stream
for debugging the `kalman` client.

Usage:
  ./imu_test_harness.py -p 4242 -t READY

Behavior:
 - Waits for a handshake token from a client.
 - Sends a few informational lines, then sends a TRUE POSITION block (three
   floating numbers, newline-separated) as the initial packet.
 - Waits up to 1s for the client's estimation reply. Computes Euclidean
   distance between true position and client's estimate. If distance > 5.0,
   prints "Error: Delta is too high" and exits with code 2. Otherwise prints
   success and continues sending a couple of updates (SPEED and position)
   for quick end-to-end checks.
"""

import argparse
import socket
import sys
import time
import math

parser = argparse.ArgumentParser()
parser.add_argument("-p", "--port", type=int, default=4242)
parser.add_argument("-t", "--token", default="START")
parser.add_argument("--threshold", type=float, default=5.0)
args = parser.parse_args()

PORT = args.port
TOKEN = args.token
THRESHOLD = args.threshold

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", PORT))
print(f"Listening on UDP port {PORT} for handshake (token={TOKEN})")

# receive handshake
data, addr = sock.recvfrom(4096)
recv = data.decode('utf-8', errors='ignore').strip()
print("RECV", recv)
if recv != TOKEN:
    print(f"Unexpected handshake token: {recv!r}, expected {TOKEN!r}")

# send informational messages
def send(msg, delay=0.05):
    sock.sendto(msg.encode('utf-8'), addr)
    time.sleep(delay)

send("Trajectory Generation. . .\n\n")
send("Trajectory Generated!\nSending Info. . .\n\n")

# initial true position
true_pos = (2.0, -3.0, 0.5)
send("MSG_START\n")
send("[00:00:00.000]TRUE POSITION\n")
# send the three coords as one packet (newline separated)
pos_msg = f"{true_pos[0]}\n{true_pos[1]}\n{true_pos[2]}\n"
send(pos_msg, delay=0.01)
print("SEND TRUE POSITION ->", pos_msg.replace('\n', ' | '))

# wait for client's reply (1s)
sock.settimeout(1.0)
try:
    data, _ = sock.recvfrom(4096)
except socket.timeout:
    print("Error: client did not reply within 1s")
    sys.exit(3)

reply = data.decode('utf-8', errors='ignore').strip()
print("RECV REPLY ->", reply)
# parse reply as three doubles
try:
    toks = reply.split()
    est = (float(toks[0]), float(toks[1]), float(toks[2]))
except Exception as e:
    print("Error: failed to parse client reply:", e)
    sys.exit(4)

# compute distance
dx = est[0] - true_pos[0]
dy = est[1] - true_pos[1]
dz = est[2] - true_pos[2]
dist = math.sqrt(dx*dx + dy*dy + dz*dz)
print(f"Distance between true and estimate: {dist:.6f}")
if dist > THRESHOLD:
    print("Error: Delta is too high")
    sys.exit(2)

print("OK: distance within threshold")

# send a SPEED tag and the numeric as two packets to match observed format
send("SPEED\n")
send("65.0\n")

# send another position update (three numbers on one line)
upd = (true_pos[0] + 0.01, true_pos[1] + 0.02, true_pos[2])
send(f"{upd[0]} {upd[1]} {upd[2]}\n")

# accept reply for this update within 1s (optional)
sock.settimeout(1.0)
try:
    data, _ = sock.recvfrom(4096)
    print("RECV REPLY 2 ->", data.decode('utf-8', errors='ignore').strip())
except socket.timeout:
    print("Client did not reply to second update within 1s (expected for testing)")

print("Test harness done")
sys.exit(0)
