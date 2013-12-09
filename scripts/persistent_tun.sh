#!/bin/bash

iface1=tun1
iface2=tun2
addr1=192.168.3.101/24
addr2=192.168.3.102/24
mtu=1400

if [ -d "/sys/class/net/$iface1" ]; then
    sudo ip tuntap del dev $iface1 mode tun || exit 1
fi

if [ -d "/sys/class/net/$iface2" ]; then
    sudo ip tuntap del dev $iface2 mode tun || exit 1
fi

if [ "$1" != "" ]; then
    exit 0
fi

# create interfaces
sudo ip tuntap add dev $iface1 mode tun || exit 1
sudo ip tuntap add dev $iface2 mode tun || exit 1

# enable interfaces
sudo ip link set dev $iface1 up || exit 1
sudo ip link set dev $iface2 up || exit 1

# add addresses
sudo ip addr add dev $iface1 $addr1
sudo ip addr add dev $iface2 $addr2

# set mtu
sudo ip link set dev $iface1 mtu $mtu
sudo ip link set dev $iface2 mtu $mtu
