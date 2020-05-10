brctl addbr qemubr1
ip link set dev qemubr1 up

tunctl -u $(whoami) -t qemuif10
brctl addif qemubr0 qemuif10
ip link set qemuif10 up

tunctl -u $(whoami) -t qemuif11
brctl addif qemubr1 qemuif11
ip link set qemuif11 up

ifconfig qemuif11 promisc

qemu-system-x86_64 -enable-kvm -nographic -device isa-debug-exit -m 8G -netdev tap,ifname=qemuif10,id=uknetdev,script=no,vhost=on -device virtio-net-pci,netdev=uknetdev,mac=aa:bb:cc:00:01:02 -netdev tap,ifname=qemuif11,id=uknetdev1,script=no,vhost=on -device virtio-net-pci,netdev=uknetdev1,mac=aa:bb:cc:00:01:03 -cdrom seed.img -smp cpus=15 -cpu host -mem-path /dev/ -display curses -hda debian-10.3.4-20200429-openstack-amd64.qcow2
