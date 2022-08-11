#!/bin/bash

# This script runs a contention experiment for a single transmitter/receiver placement.
# Two traces are generated and placed in $OUTPUT_DIR:
# tx_on.log: receiver latency trace with the transmitter producing traffic on the network
# tx_off.log: receiver latency trace with a fake transmitter that spins without producing network traffic 

TX_CORE=$1
TX_SLICE_A=$2
TX_SLICE_B=$3

RX_CORE=$4
RX_MS_SLICE=$5
RX_EV_SLICE=$6

OUTPUT_DIR=$7

mkdir -p $OUTPUT_DIR

# Kill any stray processes from previous runs
sudo killall -9 transmitter &> /dev/null
sudo killall -9 transmitter-no-loads &> /dev/null
sudo killall -9 receiver &> /dev/null

sleep 0.5
# Start transmitter
sudo ./bin/transmitter $TX_CORE $TX_SLICE_A $TX_SLICE_B & 
# Start receiver
sudo ./bin/receiver $RX_CORE $RX_MS_SLICE $RX_EV_SLICE

sudo killall -9 transmitter &> /dev/null
sudo mv rx_out.log $OUTPUT_DIR/tx_on.log

sleep 0.5
# Run with fake transmitter
sudo ./bin/transmitter-no-loads $TX_CORE &
sudo ./bin/receiver $RX_CORE $RX_MS_SLICE $RX_EV_SLICE

sudo killall -9 transmitter-no-loads &> /dev/null
sudo mv rx_out.log $OUTPUT_DIR/tx_off.log
