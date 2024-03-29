import os
import sys

import matplotlib.pyplot as plt
import numpy as np


def read_from_file(filename):
    result_x = []
    result_y = []
    # Results of all runs are all in the same file
    with open(filename) as f:
        for line in f:
            x, y = line.strip().split()
            result_x.append(int(x))
            result_y.append(int(y))
    return result_x, result_y


def plot_distributions(data, end_at, filename):
    plt.figure(figsize=(6.4, 2))

    # Plot raw data
    plt.ylabel('Latency (cycles)')
    plt.plot(data[0][:end_at], data[1][:end_at])
    plt.grid(axis='y')

    # Mark the boundary between each interval
    cur_iteration = 0
    for i in range(100):
        bar = interval * i
        if (data[0][0] < bar < data[0][end_at - 1]):
            plt.axvline(x=interval*i, color='tab:orange', linestyle='--')
    plt.xlabel('Time (cycles)')

    # Save plot to file
    plt.tight_layout()
    plt.savefig(filename)
    plt.clf()

interval = 0

def main():
    # Prepare output directory
    out_dir = 'plot'
    try:
        os.makedirs(out_dir)
    except:
        pass

    # Check that there is exactly 1 argument
    assert len(sys.argv) == 3, "Specify the files with the results as argument, and the interval"
    result_x, result_y = read_from_file(sys.argv[1])

    global interval
    interval = int(sys.argv[2])

    # Parse trace into intervals
    cur_iteration = 0
    iteration_latencies = {}
    for i in range(len(result_x)):
        x = result_x[i]
        y = result_y[i]
        if (x > interval * (cur_iteration + 1)):
            cur_iteration += 1

        iteration_latencies.setdefault(cur_iteration, []).append(y)
    
    # Print avg length of each iteration in samples
    interval_lens = [len(iteration_latencies[interval]) for interval in iteration_latencies.keys()]
    print('Median samples per interval: ', np.median(interval_lens))

    # Sample 16 intervals to plot
    filtered_result_x = []
    filtered_result_y = []
    start = interval * 146  # Start from, e.g., interval 190
    ended = 0
    for i in range(len(result_x)):
        x = result_x[i]
        y = result_y[i]

        # We can sample a bit more than 16 intervals so that
        # the moving average can continue until the end
        if (x < start):
            continue
        elif (x < start + interval * 20):
            offset = 0
            filtered_result_x.append(x - start + offset)
            filtered_result_y.append(y)

            if x > start + interval * 10 and ended == 0:
                end_at = len(filtered_result_x)
                ended = 1
        else:
            break

    # Plot the data
    selected_samples = filtered_result_x, filtered_result_y
    plot_distributions(selected_samples, end_at, "./plot/covert-channel-bits.pdf")


if __name__ == "__main__":
    main()
