#!/bin/bash

# Clean up the environment before running experiments
../util/setup-prefetch-on.sh

# Fix various frequencies (optional)
# These comamnds facilitate analyzing the latency measurements but are not necessary for the attack
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor 2> /dev/null	# set performance governor
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo		# disables turbo boost
sudo wrmsr 0x620 0x1616		# pins the uncore frequency to 2.2 GHz

# Create semaphores
sudo bin/setup-sem
