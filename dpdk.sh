#!/bin/bash

if [[ $(/usr/bin/id -u) -ne 0 ]]; then
    echo "Script is not run as root. Exiting."
    exit 1
fi

ifconfig enp2s0f2np2 down
ifconfig enp2s0f3np3 down

modprobe vfio-pci

dpdk-devbind.py -u enp2s0f2np2 enp2s0f3np3
dpdk-devbind.py -b vfio-pci 0000:02:00.2 0000:02:00.3
