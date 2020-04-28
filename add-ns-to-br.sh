bridge=$1
namespace=$2
addr=$3

vethA=veth-$namespace
vethB=eth1

ip netns add $namespace
ip link add $vethA type veth peer name $vethB

ip link set $vethB netns $namespace
ip netns exec $namespace ip addr add $addr dev $vethB
ip netns exec $namespace ip link set $vethB up

ip link set $vethA up

brctl addif $bridge $vethA
