#!/bin/bash

dev=tun0
ip=192.168.3.2/24
mtu=1350

sudo ip tuntap add $dev mode tun
sudo ip link set dev $dev mtu $mtu
sudo ip addr add dev $dev $ip
sudo ip link set dev $dev up
