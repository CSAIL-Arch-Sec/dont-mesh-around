#!/bin/bash

# Kill any stray transmitters in case run.sh did not terminate correctly
sudo killall -9 transmitter
sudo killall -9 transmitter-no-loads

# Delete semaphores
sudo bin/cleanup-sem

# Restore the various frequency settings, if they were changed
echo powersave | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor 2> /dev/null		# set powersave governor
echo 0 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo		# re-enables turbo boost
sudo wrmsr 0x620 0xc18		# re-sets the uncore frequency range

# Restore environment after running experiments
../util/cleanup.sh
