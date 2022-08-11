#!/usr/bin/env bash

# Set up the environment before running experiments with cache prefetchers enabled

# Ensure the msr kernel module is loaded
sudo modprobe msr

# Enable prefetchers
sudo wrmsr -a 0x1a4 0

# Provision some hugepages
echo 2048 | sudo tee /proc/sys/vm/nr_hugepages

# Disable transparent hugepages (optional)
echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled

# Disable SMT (optional)
echo off | sudo tee /sys/devices/system/cpu/smt/control
