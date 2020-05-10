id_exp="${1:?You must pass an experiment number 0-4}"

function exp_4 () {
        brctl addbr br-test0
	ip link set dev br-test0 up

        ./add-ns-to-br.sh br-test0 ns1 10.0.1.1/24
        ./add-ns-to-br.sh br-test0 ns2 10.0.1.2/24

        ip netns exec ns1 taskset -c 2,8 ./server &
        sleep 1
        ip netns exec ns2 taskset -c 15 ./client 4 800

        killall server
}

function exp_1() {
        taskset -c 2,8 ./server &
        sleep 1
        taskset -c 15 ./client 1 600

        killall server
}

function exp_2() {

        killall start-qemu.sh
        killall qemu-system-x86_64

}

if [ $1 -eq 4 ]
then
        exp_4
fi

if [ $1 -eq 1 ]
then
	exp_1
fi
