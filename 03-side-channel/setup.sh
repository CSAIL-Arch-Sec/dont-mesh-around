#!/bin/bash

sudo pkill -f mesh-victim
sudo pkill -f mesh-monitor

../util/setup-prefetch-on.sh
