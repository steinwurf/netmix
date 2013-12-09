#!/bin/bash

function oct2cidr () {
  local mask bit cidr i
  mask=$1

  if grep -qv '\.' <<<$mask; then
    echo $mask
    return
  fi

  for i in 1 2 3 4; do
    bit=${bit}$(printf "%08d" \
        $(echo 'ibase=10;obase=2;'$(cut -d '.' -f $i <<<$mask) |bc))
  done

  cidr=$(echo -n ${bit%%0*} |wc -m)
  echo $cidr
}

function read_info() {
  nminfo=$(nmcli device list iface $1)
  iface=$(echo -e "$nminfo" | grep "GENERAL.IP-IFACE" | cut -f2 -d:)
  addr=$(echo -e "$nminfo" | grep ip_address | cut -f2 -d=)
  gw=$(echo -n "$nminfo" | grep " routers " | cut -f2 -d=)
  net=$(echo -n "$nminfo" | grep network_number | cut -f2 -d=)
  mask=$(echo -n "$nminfo" | grep " subnet_mask " | cut -f2 -d=)
  cidr=$(oct2cidr $mask)

  echo $iface
  echo $addr
  echo $gw
  echo $net/$cidr
}

function setup_table() {
  table=$1
  sudo ip route flush table $table
  sudo ip route del $net/$cidr
  sudo ip route add $net/$cidr dev $iface src $addr
  sudo ip route add $net/$cidr dev $iface src $addr table $table
  sudo ip route add default via $gw dev $iface table $table
  sudo ip rule add from $addr table $table prio $prio
  sudo ip route add 127.0.0.0/8 dev lo table $table
  prio=$(echo $prio - 1 | bc)
}

function setup_balance() {
    nminfo=$(nmcli device list iface $1)
    gw1=$(echo -n "$nminfo" | grep " routers " | cut -f2 -d=)
    if1=$(echo -e "$nminfo" | grep "GENERAL.IP-IFACE" | cut -f2 -d:)

    nminfo=$(nmcli device list iface $2)
    gw2=$(echo -n "$nminfo" | grep " routers " | cut -f2 -d=)
    if2=$(echo -e "$nminfo" | grep "GENERAL.IP-IFACE" | cut -f2 -d:)

    sudo ip route del default
    sudo ip route add default scope global \
        nexthop via $gw1 dev $if1 weight $3 \
        nexthop via $gw2 dev $if2 weight $4
}

function flush_rule() {
  prio=32767
  sudo ip rule flush

  sudo ip rule add from all lookup default prio $prio
  prio=$(echo $prio - 1 | bc)

  sudo ip rule add from all lookup main prio $prio
  prio=$(echo $prio - 1 | bc)
}

flush_rule

dev1=$1
dev2=$2

read_info $dev1
setup_table t1

echo ""

read_info $dev2
setup_table t2

setup_balance $dev1 $dev2 50 50
