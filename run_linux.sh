tunctl -u $(whoami) -t qemuif10
brctl addif qemubr0 qemuif10
ip link set qemuif10 up

taskset -c 10 qemu-system-x86_64 -enable-kvm -nographic -device isa-debug-exit -m 512M -netdev tap,ifname=qemuif10,id=uknetdev,script=no -device virtio-net-pci,netdev=uknetdev,mac=aa:bb:cc:00:01:02  -drive file=xenial-server-cloudimg-amd64-disk1.img,if=virtio,cache=writeback,index=0 -cdrom seed.img

