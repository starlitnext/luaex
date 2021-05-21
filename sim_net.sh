

if [ -n "$1" ]; then
  sudo tc qdisc del dev ens33 root
else
  sudo tc qdisc add dev ens33 root handle 1: prio
  sudo tc qdisc add dev ens33 parent 1:3 handle 30: netem loss 20% delay 100ms
  sudo tc filter add dev ens33 protocol ip parent 1:0 u32 match ip dst 30.210.220.83 match ip dport 20001 0xffff flowid 1:3
fi
