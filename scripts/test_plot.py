#!/usr/bin/env python

import sys
from matplotlib import pyplot as pp

if len(sys.argv) < 2:
    print("please specify data file to plot")
    sys.exit(1)

f = open(sys.argv[1])
d = {}
device = None
family = None
spacing = False
test = -1

for line in f:
    l = line.strip()

    # check for new section
    if not l:
        device = None
        family = None
        test = -1
        continue

    # ignore comments
    if l.lstrip()[0] == '#':
        print(l)
        spacing = True
        continue

    # read data from line
    key,val = l.split(": ")

    # use device as key for next iterations
    if key == "dev":
        device = val
        continue

    if key == "family":
        family = val
        continue

    if key == "test":
        test = int(val)

    # ignore line if device, family or test is not given
    if not device or not family or test == -1:
        continue

    pair = device + " " + family

    # instantiate dictionary for new device
    if pair not in d:
        print(device, pair)
        d[device] = {}
        d[pair] = {}

    # instantiate data list for new key
    if key not in d[pair]:
        d[device][key] = []
        d[pair][key] = []

    # make sure lists has enough elements
    if len(d[device][key]) <= test:
        d[device][key] += [-1]*(test - len(d[device][key]) + 1)

    # convert to float if possible
    try:
        val = float(val)
    except ValueError:
        pass

    d[device][key][test] = val
    d[pair][key].append(val)

if spacing:
    print()

colors = ["#cc0000", "#3465a4"]
markers = ['x', 'v']
summed = {'rates': [], 'index': []}
labels = ["Channel B", "Channel A"]
count = 0

for device,val in d.items():
    rates  = val["rate"]
    delays = val["avg"]
    tests  = val["test"]

    if -1 in tests:
        continue

    if device != "tun0":
        summed['rates'].append(rates)
        summed['index'].append(tests)
        color = colors.pop()
        marker = markers.pop()
        label = labels.pop()
        count += 1
    else:
        device = "RLNC"
        color = "#73d216"
        marker = 'o'
        label = "RLNC"

    pp.subplot(211)
    print(device)
    print(tests)
    print(rates)
    print()
    pp.plot(tests, rates, label=label, linewidth=2, marker=marker, color=color)

    pp.subplot(212)
    pp.plot(tests, delays, label=label, linewidth=2, marker=marker, color=color)

    print("{:5s} {:6.1f} kbps  {:6} ms".format(device, sum(rates)/len(rates), sum(delays)/len(delays)))

if False and count != 1:
    r = summed['rates']
    t = summed['index']
    summed_rate = [sum(a) for a in zip(*r)]
    summed_test = [sum(a)/len(t) for a in zip(*t)]

    pp.plot(summed_test, summed_rate, label="Summed", linewidth=2, marker='<', color="#c17d11")

pp.subplot(211)
pp.title("Throughput for Multipath RLNC on Two Channels")
pp.ylabel("Throughput [kbps]")
pp.legend(loc="best", fontsize="medium")
pp.gca().set_ylim(bottom=0)
pp.grid()

pp.subplot(212)
pp.ylabel("Delays [ms]")
pp.xlabel("Test ID")
pp.grid()

pp.show()
