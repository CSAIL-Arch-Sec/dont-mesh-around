# Covert Channel on the Mesh Interconnect

The code in this folder implements a basic interprocess covert-channel.
As described in the paper, we use a modified version of the receiver that does not require an EV.

## Prerequisites

- Build all files with `make`
- Ensure that you have the Python virtual environment installed in the parent directory.

## Run

Make sure that your system is idle and minimize the number of background processes that are running and may add noise to the experiment.

### Plot Covert Channel Trace

**Expected Runtime: 2 min**

This experiment demonstrates the covert channel by having the transmitter send a stream of alternating bits.
It outputs a plot with a sample of the trace collected by the receiver.
The default interval used is 3000 cycles, but a different interval can be specified by passing it as an argument to the script.

To run the experiment, run `./run-all-covert.sh [interval]`.

The output of the script can be found in `plot/covert-channel-bits.pdf`.
At an interval of 3000, every other interval should have clear high peaks as shown in Figure 7 in the paper.

### Benchmark Covert Channel Capacity

**Expected Runtime: 30 min**

This experiment determines the maximum achievable channel capacity.
The test runs the covert channel test with different interval values and computes the channel capacity metric.
The resulting plot shows how the channel capacity and the error probability change with the interval.
For this test, the transmitter sends a randomized (non-alternating) sequence of bits to provide a more realistic benchmark.
Each trial is repeated 5 times to provide error bars in the final plot.

To run the experiment, run `./run-all-capacity.sh`.

The output of the script can be found in `plot/capacity-plot.pdf`.
The plot should show the channel capacity peaking around 1.5 Mbps at 3-5 Mbps of raw bandwidth, as shown in Figure 8 in the paper.
