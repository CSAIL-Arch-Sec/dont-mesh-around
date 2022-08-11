#!/usr/bin/env bash

# Restore environment after running experiments

# Re-enable prefetchers
sudo wrmsr -a 0x1a4 0

# Re-enable SMT, if it was disabled
echo on | sudo tee /sys/devices/system/cpu/smt/control

# Re-enable transparent hugepages, if they were disabled
echo madvise | sudo tee /sys/kernel/mm/transparent_hugepage/enabled

# Unload MSR module
sudo modprobe -r msr