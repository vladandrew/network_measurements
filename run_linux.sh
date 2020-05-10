brctl addbr qemubr1
ip link set dev qemubr1 up

tunctl -u $(whoami) -t qemuif10
brctl addif qemubr0 qemuif10
ip link set qemuif10 up

tunctl -u $(whoami) -t qemuif11
brctl addif qemubr1 qemuif11
ip link set qemuif11 up

qemu-system-x86_64 -enable-kvm -nographic -device isa-debug-exit -m 4G -netdev tap,ifname=qemuif10,id=uknetdev,script=no -device virtio-net-pci,netdev=uknetdev,mac=aa:bb:cc:00:01:02 -netdev tap,ifname=qemuif11,id=uknetdev1,script=no -device virtio-net-pci,netdev=uknetdev1,mac=aa:bb:cc:00:01:03 -drive file=bionic-server-cloudimg-amd64.img,if=virtio,cache=writeback,index=0 -cdrom seed.img -smp cpus=15 -cpu host -mem-path /dev/ -display curses 
