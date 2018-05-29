#! /usr/bin/env python

import sys
import socket

if "__main__" == __name__:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect(("127.0.0.1", 8008))
    except ConnectionRefusedError as e:
        print(e)
        sys.exit(1)

    data = ""
    with open(sys.argv[1], "rb") as ipf:
        data = ipf.read()

    s.sendall(data)
    rsp = s.recv(1000000)
    with open("output.data", "wb") as opf:
        opf.write(rsp)
    rsp = s.recv(1)
    print(len(rsp))
    s.close()
