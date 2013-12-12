#!/usr/bin/env python

import sys
import pandas as pd
import numpy as np
from matplotlib import pyplot as pp

if len(sys.argv) < 2:
    print("please specify data file to plot")
    sys.exit(1)

f = open(sys.argv[1])
columns = []

colors = {
        'tun0': '#73d216',
        'usb0': '#cc0000',
        'wwan0': '#3465a4',
        'eth2': '#cc0000',
        'wlan0': '#3465a4',
        'wwan0': "#cc0000",
        }

# read keys
for line in f:
    l = line.strip()

    if not l:
        continue

    key,val = l.split(": ")

    if key not in columns:
        columns.append(key)

# construct empty data frame
data = pd.DataFrame(columns=columns)

# read data from file
d = {}
f.seek(0)
for line in f:
    l = line.strip()

    if not l and not d:
        continue

    if not l:
        # update data frame
        data = data.append(d, ignore_index=True)
        d = {}
        continue

    if l[0] == "#":
        print(l)
        continue

    key,val = l.split(": ")
    try:
        val = float(val)
    except ValueError:
        pass

    d[key] = val

# add last section and merge rows in data frame
data = data.append(d, ignore_index=True)
data = data.groupby(['dev', 'family', 'test']).max()
data = data.reset_index()

# plot rates
for (dev,family),group in data.groupby(['dev', 'family']):
    df = group.sort('test')
    df = df[np.isfinite(df['rate'])]

    if not df.empty:
        pp.subplot(311)
        df.plot(x='test', y='rate', label=dev, linewidth=2, color=colors[dev])

pp.subplot(311)
pp.legend(loc='best')
pp.title("Throughput")
pp.xlabel("")
pp.ylabel("Rate [kbps]")

# plot delays
for dev,group in data.groupby(['dev']):
    if dev == "tun0":
        continue

    df = group.sort('test')

    # delays
    pp.subplot(312)
    df.plot(x='test', y='avg', label=dev, linewidth=2, color=colors[dev])
    df.plot(x='test', y='min', color=colors[dev], label="", linestyle="--")
    df.plot(x='test', y='max', color=colors[dev], label="", linestyle="--")

    # jitter
    pp.subplot(313)
    df.plot(x='test', y='mdev', label=dev, linewidth=2, color=colors[dev])


pp.subplot(312)
pp.legend(loc='best')
pp.title("Delays")
pp.xlabel("")
pp.ylabel("RTT [ms]")

pp.subplot(313)
pp.legend(loc='best')
pp.title("Jitter")
pp.xlabel("Test No. [#]")
pp.ylabel("RTT Mean Deviation [ms]")
pp.show()
