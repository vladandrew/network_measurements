#!/bin/bash

usage_and_exit() {
	echo "Usage: $0 <kernel-path> [<ifname> <mac>]"
	exit -2
}

check_if() {
	local ifname="$1"

	ip l show $ifname &>/dev/null
	if [ -n $? ]; then
		# create new tap interface
		tunctl -u $(whoami) -t $IFNAME

		# enable the interface
		$SCRIPT_DIR/ifup.sh $IFNAME
	fi

}

# Check params
[ "$#" -eq 0 ] && usage_and_exit

KERNEL="$1"
IFNAME="$2"
MAC="$3"
DEMONIZE="$4"

SCRIPT_DIR="$(dirname $0)"

# setup network interface
if [ -n "$IFNAME" ]; then
	[ -z "$MAC" ] && usage_and_exit

	shift 3
	check_if $IFNAME

	QEMU_NET_PARAMS="-netdev tap,ifname=$IFNAME,id=uknetdev,script=no -device virtio-net-pci,netdev=uknetdev,mac=$MAC"
else
	shift 1
fi

# run Qemu
arp -s 10.0.0.2 $MAC
taskset -c 2,8 /root/qemu/build/x86_64-softmmu/qemu-system-x86_64 -enable-kvm -nographic -device isa-debug-exit -nodefaults -no-user-config -serial stdio  \
	-kernel $KERNEL  \
	$QEMU_NET_PARAMS \
	"$@" | tee log.txt

