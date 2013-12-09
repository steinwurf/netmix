#!/bin/bash

wan=eth0
dev=tun0
ip=192.168.3.1/24
mtu=1350

sudo ip tuntap add $dev mode tun
sudo ip link set dev $dev mtu $mtu
sudo ip addr add dev $dev $ip
sudo ip link set dev $dev up
sudo sysctl -w net.ipv4.ip_forward=1
sudo iptables -t nat -A POSTROUTING -o $wan -j MASQUERADE
sudo iptables -A FORWARD -i $dev -j ACCEPT
