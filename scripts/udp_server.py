#!/usr/bin/env python

import os
import sys
import time
import struct
import socket
import select
import threading

# settings
port = 6349
length = int(sys.argv[1])
stop = "STOP"
exit = False
done = False
dev = ""
family = ""

# counters
test_no = 0
i = 0
b = 0
t0 = 0
t1 = 0
a = 0


if len(sys.argv) >= 3:
    csv = True
else:
    csv = False
    print("Receiving datagrams of length {} bytes".format(length))

if len(sys.argv) >= 4:
    out = open(sys.argv[3], "w")
else:
    out = sys.stdout

udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
udp_sock.bind(('', port))

tcp_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcp_sock.bind(('', port))
tcp_sock.listen(1)


def handle_data(d, f):
    global t0, t1, i, b, test_no, csv, stop, a, done, dev, family

    # Ignore packets from other tests
    rx_test, = struct.unpack(">H", d[0:2])
    if rx_test != test_no:
        return True;

    # Handle first packet in test
    if not t0:
        if not csv:
            print("{}: Received first data".format(test_no), flush=True, file=out)
        t0 = time.time()

    # Check for last packets in test
    if d[2:] == bytes(stop, 'UTF-8'):
        done = True
        return False

    # get device string
    dev = d[2:18].decode("utf-8")
    family = f

    # Update timers
    t1 = time.time()
    b += len(d)
    i += 1

    return True

def do_tcp(s):
    global t0, done, a, exit

    s.settimeout(1)

    while True:
        try:
            c,a = s.accept()
            break
        except socket.timeout:
            # stop if udp was used
            if done:
                return
        except KeyboardInterrupt:
            print("do_tcp init interupted")
            exit = True
            return

    while True:
        try:
            # read data length
            d = c.recv(2)

            # stop if connection is closed
            if not d:
                c.close()
                done = True
                return

            # unpack data length
            l, = struct.unpack(">H", d)

            # receive payload
            d = c.recv(l)
            while len(d) < l:
                d += c.recv(l - len(d))

            # handle data and return if needed
            if not handle_data(d, "tcp"):
                return

        except socket.timeout:
            if exit:
                return
            else:
                continue
        except KeyboardInterrupt:
            print("do_tcp main interupted")
            exit = True
            return

def do_udp(s):
    global t0, a, done, exit

    s.settimeout(1)

    while True:
        try:
            d, a = s.recvfrom(length)
            handle_data(d, "udp")
            break;
        except socket.timeout:
            if done:
                return
        except KeyboardInterrupt:
            print("do_udp init interupted")
            exit = True
            return


    while True:
        try:
            d, a = s.recvfrom(length)

            if not handle_data(d, "udp"):
                return

        except KeyboardInterrupt:
            print("do_udp main interupted")
            exit = True
            return
        except socket.timeout:
            if done or exit:
                return

def print_result():
    global t0, t1, i, b, csv, test_no, a, family
    t = t1 - t0
    r = round(b*8/1014/t)

    if t > 60*60:
        time_str = "{}h {:2}m".format(int(t/60/60), int((t/60)%60))
    else:
        time_str = "{}m {:2}s".format(int(t/60), int(t%60))

    if csv:
        print("dev: {}".format(dev.strip()), file=out)
        print("family: {}".format(family), file=out)
        print("test: {}".format(test_no), file=out)
        print("address: {}".format(a[0]), file=out)
        print("port: {}".format(a[1]), file=out)
        print("time: {}".format(round(t, 2)), file=out)
        print("bytes: {}".format(b), file=out)
        print("packets: {}".format(i), file=out)
        print("rate: {}".format(r), file=out)
    else:
        print("{}: Received {} datagrams of total length {} kB in {}".format(test_no, i, round(b/1024), time_str), file=out)
        print("{}: Rate: {} kbit/s".format(test_no, r), file=out)

while True:
    udp_thread = threading.Thread(target=do_udp, args=(udp_sock,))
    tcp_thread = threading.Thread(target=do_tcp, args=(tcp_sock,))
    udp_thread.start()
    tcp_thread.start()

    try:
        tcp_thread.join()
        udp_thread.join()
    except KeyboardInterrupt:
        exit = True
        done = True
        print("main interupted")
        sys.exit(0)

    if t0:
        print_result()

    print("", file=out)
    out.flush()
    done = False
    test_no += 1
    i = 0
    b = 0
    t0 = 0
    a = 0
    dev = ""
    family = ""
