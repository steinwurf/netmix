#!/usr/bin/env python

import os
import sys
import socket
import time
import struct

if len(sys.argv) < 7:
    print("Usage: {} <sock_type> <dst> <src> <pkt-len> <rate> <test_no> [time]".format(sys.argv[0]))
    sys.exit(0)

port = 6349
family = sys.argv[1]
dst    = sys.argv[2]
src    = sys.argv[3]
length = int(sys.argv[4])
rate   = int(sys.argv[5])
test_no = int(sys.argv[6])
dev    = bytes(sys.argv[7].ljust(16), "utf-8")
stop = "STOP"
interval = 1/(1024*rate/length/8)

if len(sys.argv) >= 8:
    timeout = int(sys.argv[8])
else:
    timeout = 0

if len(sys.argv) >= 10:
    csv = True
else:
    csv = False

num = struct.pack(">H", test_no)
len_pack = struct.pack(">H", length - 2)

if family == "udp":
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((src, 0))
    message = num + dev + open("/dev/urandom","rb").read(length - 2 - len(dev))
elif family == "tcp":
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind((src, 0))
    sock.connect((dst, port))
    message = len_pack + num + dev + open("/dev/urandom","rb").read(length - 4 - len(dev))
else:
    print("unknown socket family: {}".format(family))
    sys.exit(1)

i = 0

if not csv:
    print("Sending data of length {} bytes to {}".format(length, dst))


def send_udp(sock):
    global i

    s = interval
    while True:
        try:
            s0 = time.time()
            sock.sendto(message, (dst, port))
            i += 1
            time.sleep(max(0, interval + s))
            s = interval - (time.time() - s0)

            if timeout and time.time() > t0 + timeout:
                break

        except KeyboardInterrupt:
            break

        except socket.gaierror as e:
            print(e, file=sys.stderr)
            sys.exit(1)

        except Exception as e:
            print(e, file=sys.stderr)
            sys.exit(1)


def send_tcp(sock):
    global i

    while True:
        try:
            sock.send(message)
            i += 1

            if timeout and time.time() > t0 + timeout:
                break

        except KeyboardInterrupt:
            break

t0 = time.time()
if family == "udp":
    send_udp(sock)
    t1 = time.time()

    message = num + bytes(stop, 'UTF-8')
    for j in range(10):
        sock.sendto(message, (dst, port))
        time.sleep(1)
elif family == "tcp":
    send_tcp(sock)
    t1 = time.time()
    sock.close()



t = t1 - t0
b = i*length
r = round(i*length*8/1014/t)

if t > 60*60:
    time_str = "{}h {:2}m".format(int(t/60/60), int((t/60)%60))
else:
    time_str = "{}m {:2}s".format(int(t/60), int(t%60))

if csv:
    print("time: {}".format(round(t, 2)))
    print("bytes: {}".format(b))
    print("packets: {}".format(i))
    print("rate: {}".format(r))
else:
    print("Sent {} datagrams of total length {} kB in {}".format(i, round(b/1024), time_str))
    print("Rate: {} kbit/s".format(r))
