#!/usr/bin/env python

import sys
import time
import signal
import subprocess as sp
from netifaces import ifaddresses as if_addr
from netifaces import AF_INET

if len(sys.argv) < 6:
    print("specify destination and device(s)")
    sys.exit(1)

mtu     = sys.argv[1]
rate    = sys.argv[2]
dur     = sys.argv[3]
tests   = int(sys.argv[4])
devices = sys.argv[5:]
i       = 0

addrs = []
for item in devices:
    dev,dst,sock = item.split(":")
    src = if_addr(dev)[AF_INET][0]['addr']
    addrs.append((src,dst,sock,dev))

pings = {}
for addr in addrs:
    cmd = ['ping', addr[1], '-I', addr[3], '-B', '-q']
    pings[addr[3]] = cmd

try:
    for test in range(tests):
        for addr in addrs:
            ping_procs = {}
            cmd = ['python', 'udp_client.py', addr[2], addr[1], addr[0], mtu, rate, str(i), addr[3], dur, "1"]
            p = sp.Popen(cmd, stdout=sp.PIPE, stderr=sp.PIPE)

            for dev,cmd in pings.items():
                ping_proc = sp.Popen(cmd, stdout=sp.PIPE, stderr=sp.PIPE)
                ping_procs[dev] = ping_proc

            p.wait()

            for dev,p in ping_procs.items():
                p.send_signal(signal.SIGINT)
                cout,cerr = p.communicate()

                result = cout.splitlines()[-1].decode('utf-8')
                keys,vals = result[4:].split(" = ")
                keys = keys.split("/")

                idx = vals.find(" ")
                vals = vals[:idx].split("/")

                print("dev: {}".format(dev))
                print("family: {}".format(addr[2]))
                print("test: {}".format(i))
                for key,val in zip(keys,vals):
                    print("{}: {}".format(key, val))
                print()


            i += 1
            time.sleep(1)
except KeyboardInterrupt:
    pass
