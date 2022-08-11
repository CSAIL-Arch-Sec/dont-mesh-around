#!/bin/bash
cd "${BASH_SOURCE%/*}/" || exit # cd into correct directory

# Parse args
if [ $# -eq 1 ]; then
	INTERVAL=$1
elif [ $# -lt 1 ]; then
	INTERVAL=3000
else
	echo "ERROR: Incorrect number of arguments"
	echo "./run.sh [interval]"
	exit
fi

echo "Running covert channel test with an interval of $INTERVAL cycles"

# Kill previous processes
sudo killall transmitter &> /dev/null

./setup.sh

# Run
until
	sudo ./bin/transmitter 8 5 $INTERVAL & # > /dev/null &
	sleep 1
	sudo ./bin/receiver-no-ev 7 6 ./out/receiver-contention.out $INTERVAL #> /dev/null
do
	echo "Repeating iteration $i because it failed"
	sudo killall transmitter &> /dev/null
done

sleep 0.05
sudo killall transmitter &> /dev/null

./cleanup.sh

echo "Generating plot"
source ../venv/bin/activate
python plot-channel-bits-figure.py out/receiver-contention.out $INTERVAL
python print-errors.py out/receiver-contention.out $INTERVAL
deactivate
