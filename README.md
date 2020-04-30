There are 5 experiments that can be ran. Below we describe how to run each experiment.

* Experiment 1 - Linux userspace
`./run_exp 1`

* Experiment 4 - 2 veth pairs connected by a bridge
`./run_exp 4`

* Experiment 2 - Unikraft raw netdev

* Experiment 3 - Linux VM

* Experiment 5 - Unikraft lwip

# Running a linux vm
We're running the Xenial cloud image from http://cloud-images.ubuntu.com/xenial/current/xenial-server-cloudimg-amd64-disk1.img
A cloud config init file is requiered.

A cloud config is needed
`cloud-localds init.img init`

bridge-utils uml-utilities net-tools psmisc
