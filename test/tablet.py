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
hololensIP = b"127.0.0.1"
values = (1, len(hololensIP), hololensIP)
packer = struct.Struct(f">HI{len(hololensIP)}s")
data   = packer.pack(*values)
self.sendall(data)

time.sleep(1.0)

dataset = b"Agulhas_10_resampled.vtk";
values = (3, len(dataset), dataset, 1, 1, 0)
print(f"Sending open dataset dataset.vtk. Size : {2+4+len(dataset)+3*4}")
packer = struct.Struct(f">HI{len(dataset)}sIII")
data   = packer.pack(*values)
self.sendall(data)
