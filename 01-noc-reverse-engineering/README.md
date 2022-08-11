# Mesh Interconnect Reverse Engineering

These scripts facilitate running the transmitter and receiver in different configurations.
We use experiments to confirm our lane scheduling policy and priority arbitration policy.
These scripts generate data for Figure 6.

## Prerequisites

- Make all artifacts with `make`
- Ensure that the Python virtual environment has been installed in the parent directory
- Run `./setup.sh` to prepare the machine

## Running the Case Studies

**Expected Runtime: 3 min**

Running `sudo ../venv/bin/python placement-experiments.py` will produce data that aligns with Figure 6.
Run `./cleanup.sh` to restore the machine settings.

## Troubleshooting

The following are some commonly-observed issues with this script.

- **I see many large negative latency difference values.**

    There are probably too many L1/L2 cache hits that are not being filtered out correctly.
    You can examine the output latency values in `data/{placement-config}/tx_on.out` and `data/{placement-config}/tx_off.out`
    `placement-config` is a 6-number string of the following form: `{tx_core}-{tx_slice_a}-{tx_slice_b}-{monitor_core}-{monitor_ms_slice}-{monitor_ev_slice}`

    Adjust the values in the `filter_trace` function in `placement-experiments.py` to filter out the high and low outliers.
    On our machine, the expected LLC access latency is around 70 cycles.

- **A few reported values do not match Figure 6.**

    These experiments are sensitive to noise and may require running a few re-runs to collect all the data accurately.
    Additionally, values with magnitude less than or equal to 0.5 are considered 0 contention difference.
    Configurations that incur slice port contention (indicated by the hatch-shaded boxes) can have higher variability than other squares.
    These configurations may occasionally see contention differences slightly below 5 or above 10.
