#!/usr/bin/python3
#-*-coding:utf8-*-

import socket
import time
import struct

self = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
self.connect(("localhost", 8000))
self.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

print(f"Local address : {self.getsockname()}")

print("Sending login...")
hololensIP = b"192.168.1.109"
values = (1, len(hololensIP), hololensIP)
packer = struct.Struct(f">HI{len(hololensIP)}s")
data   = packer.pack(*values)
self.sendall(data)

while True:
    time.sleep(1.0)
