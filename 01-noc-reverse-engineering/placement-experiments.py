import subprocess
from collections import namedtuple

import numpy as np

Placement = namedtuple('Placement', 'tx_core tx_slice_a tx_slice_b rx_core rx_ms_slice rx_ev_slice')

DIVIDER = '=' * 40

DIE_LAYOUT = [
    [0, 4, 9, 13, 17, 22],
    [-1, 5, 10, 14, 18, -1],
    [1, 6, 11, 15, 19, 23],
    [2, 7, 12, -1, 20, 24],
    [3, 8, -1, 16, 21, 25]
]


def print_coord(slice_id):
    """Return a string that represents slice_id using the notation from the paper."""
    coord = None
    for r, row in enumerate(DIE_LAYOUT):
        if slice_id in row:
            coord = (row.index(slice_id), r)
    if coord is None:
        print(f'Error: could not find Slice ID {slice_id} in DIE_LAYOUT')
    return f'({coord[1]},{coord[0]})'


def get_placement_path(p):
    """Return the path for the data of the experiment with placment p."""
    return f'data/{p.tx_core}-{p.tx_slice_a}-{p.tx_slice_b}-{p.rx_core}-{p.rx_ms_slice}-{p.rx_ev_slice}'


def test_placement(p):
    """Run a single test with a specific tx/rx placement.

    p: Placement consisting of tx_core, tx_slice_a, tx_slice_b, rx_core, rx_ms_slice rx_ev_slice
    Output traces are stored in data/{tx_core}-{tx_slice_a}-{tx_slice_b}-{rx_core}-{rx_ms_slice}-{rx_ev_slice}/
    The output traces should be named tx_on.log and tx_off.log.
    """
    output_path = get_placement_path(p)
    cmd = f'./run-single.sh {p.tx_core} {p.tx_slice_a} {p.tx_slice_b} {p.rx_core} {p.rx_ms_slice} {p.rx_ev_slice} {output_path}'.split(' ')
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def load_trace(filepath):
    return np.genfromtxt(filepath, delimiter=' ')


def filter_trace(trace, percentile=10):
    upper_thresh = 100
    lower_thresh = 40
    return trace[np.logical_and(trace > lower_thresh, trace < upper_thresh)]


def get_latency_diff(p):
    """Post-process a set of tx_on/tx_off traces to get the latency difference."""
    exp_path = get_placement_path(p)
    tx_on_trace = load_trace(f'{exp_path}/tx_on.log')
    tx_off_trace = load_trace(f'{exp_path}/tx_off.log')
    tx_on_mean = np.mean(filter_trace(tx_on_trace[:, 1]))
    tx_off_mean = np.mean(filter_trace(tx_off_trace[:, 1]))
    return round(tx_on_mean - tx_off_mean, 1)


def lane_scheduling_case_study():
    """Reproduce Figure 6a from the paper.

    This case study demonstrates the lane scheduling policy.
    The receiver monitors Core(0,2) -> Slice(0,3).
    The transmitter is varied across all possible positions within the row.
    """
    print(f'{DIVIDER}\nLane Scheduling Policy Case Study')
    print(f'Receiver: {print_coord(9)}->{print_coord(13)}')
    print('Transmitter\tLatency Difference')
    row_0 = [0, 4, 9, 13, 17, 22]
    rx_core = 9
    rx_ms_slice = 13
    rx_ev_slice = rx_core  # use a local EV slice
    for tx_core in row_0:
        if tx_core == rx_core:
            continue
        tx_slice_b = tx_core  # use a local EV slice
        for tx_slice_a in row_0:
            p = Placement(tx_core, tx_slice_a, tx_slice_b, rx_core, rx_ms_slice, rx_ev_slice)
            test_placement(p)
            diff = get_latency_diff(p)
            print(f'{print_coord(p.tx_core)}->{print_coord(p.tx_slice_a)}:\t{diff:4.1f}')
    print(f'{DIVIDER}\n')


def priority_arbitration_case_study():
    """Reproduce Figure 6b from the paper.

    This case study demonstrates the priority arbitration policy.
    The receiver monitors Core(0,0) -> Slice(0,5).
    The transmitter is varied across all possible positions within the row.
    """

    print(f'{DIVIDER}\nLane Scheduling Policy Case Study')
    print(f'Receiver: {print_coord(0)}->{print_coord(22)}')
    print('Transmitter\tLatency Difference')
    row_0 = [0, 4, 9, 13, 17, 22]
    rx_core = 0
    rx_ms_slice = 22
    rx_ev_slice = rx_core  # use a local EV slice
    for tx_core in row_0:
        # Do not pin the tx and rx to the same core
        if tx_core == rx_core:
            continue

        tx_slice_b = tx_core  # Use the local slice
        for tx_slice_a in row_0:
            p = Placement(tx_core, tx_slice_a, tx_slice_b, rx_core, rx_ms_slice, rx_ev_slice)
            test_placement(p)
            diff = get_latency_diff(p)
            print(f'{print_coord(p.tx_core)}->{print_coord(p.tx_slice_a)}:\t{diff:4.1f}')
    print(f'{DIVIDER}\n')


def main():
    lane_scheduling_case_study()
    priority_arbitration_case_study()


if __name__ == '__main__':
    main()
