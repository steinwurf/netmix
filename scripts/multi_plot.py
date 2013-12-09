#!/usr/bin/env python

import sys
import pandas as pd
import numpy as np
from matplotlib import pyplot as pp

if len(sys.argv) < 2:
    print("please specify data file to plot")
    sys.exit(1)

f = open(sys.argv[1])

keys = []
index = ['dev', 'family', 'test']

# read keys
for line in f:
    l = line.strip()

    if not l:
        continue

    key,val = l.split(": ")

    if key not in keys:
        keys.append(key)

# construct data frame
data = pd.DataFrame(index=index, columns=keys)
df = pd.DataFrame(index=index, columns=keys)

d = {}
f.seek(0)
for line in f:
    l = line.strip()

    if not l:
        # update data frame
        df = pd.DataFrame(d, index=index, columns=keys)
        data = data.append(df)
        d = {}
        continue

    key,val = l.split(": ")
    try:
        val = float(val)
    except ValueError:
        pass

    d[key] = val

print(data)
