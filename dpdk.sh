#!/bin/bash

if [[ $(/usr/bin/id -u) -ne 0 ]]; then
    echo "Script is not run as root. Exiting."
    exit 1
fi

ifconfig enp2s0f0np0 down
ifconfig enp2s0f1np1 down

modprobe vfio-pci

dpdk-devbind.py -u enp2s0f0np0 enp2s0f1np1
dpdk-devbind.py -b vfio-pci 0000:02:00.0 0000:02:00.1
