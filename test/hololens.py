#!/usr/bin/python3
#-*-coding:utf8-*-

import socket
import time
import struct

self = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
self.connect(("localhost", 8000))
self.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

values = (0,)
packer = struct.Struct(f">H")
data   = packer.pack(*values)
self.sendall(data)

while True:
    buf = self.recv(42)
    print(buf)
    time.sleep(1.0)
