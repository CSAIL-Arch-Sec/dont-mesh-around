#!/bin/bash
cd "${BASH_SOURCE%/*}/" || exit # cd into correct directory

CPU_GHZ=2.2

rm -rf out/capacity-data.out
./setup.sh

for ITERATION in {1..5}; do
	echo Iteration $ITERATION
	for BITRATE in 0.5 1 1.5 2 2.5 3 3.5 4 4.5 5 5.5 6 6.5 7 7.5 8; do
		
		# Compute interval for the desired bitrate
		source ../venv/bin/activate
		INTERVAL=$(python print-interval-for-rate.py $CPU_GHZ $BITRATE)
		deactivate

		# Kill previous processes
		sudo killall -9 transmitter-rand-bits &> /dev/null
		sudo killall -9 receiver-no-ev &> /dev/null

		# Run
		until
			sudo ./bin/transmitter-rand-bits 8 5 $INTERVAL > /dev/null &
			sleep 1
			sudo ./bin/receiver-no-ev 7 6 ./out/receiver-contention.out $INTERVAL > /dev/null
		do
			echo "Repeating iteration because it failed"
			sudo killall transmitter-rand-bits &> /dev/null
		done

		sleep 0.05
		sudo killall -9 transmitter-rand-bits &> /dev/null
		sudo killall -9 receiver-no-ev &> /dev/null

		source ../venv/bin/activate
		ERRORS=$(python print-errors.py out/receiver-contention.out $INTERVAL --random_pattern)
		deactivate

		echo "$BITRATE $ERRORS" >> out/capacity-data.out
		sleep 1
	done
done

echo "Generating plot"
source ../venv/bin/activate
python plot-capacity-figure.py out/capacity-data.out
deactivate
./cleanup.sh