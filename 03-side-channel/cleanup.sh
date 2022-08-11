#!/bin/bash

sudo pkill -f mesh-victim
sudo pkill -f mesh-monitor
sudo pkill -f python3
sudo pkill -f python

../util/cleanup.sh
