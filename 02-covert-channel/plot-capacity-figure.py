import os
import sys

import matplotlib.pyplot as plt
import numpy as np
import scipy.special as sc


# Source: https://github.com/scipy/scipy/blob/master/doc/source/tutorial/special.rst
def binary_entropy(x):
    return -(sc.xlogy(x, x) + sc.xlog1py(1 - x, -x))/np.log(2)


def read_from_file(filename):
    results = {}
    # Results of all runs are all in the same file
    with open(filename) as f:
        for line in f:
            x, y = line.strip().split()
            results.setdefault(float(x), []).append(int(y))
    return results


def main():
    # Prepare output directory
    out_dir = 'plot'
    try:
        os.makedirs(out_dir)
    except:
        pass

    results = read_from_file(sys.argv[1])
    # Compute the probabilities of bit flip
    error_probabilities = {}
    for bitrate in results.keys():
        for sample in results[bitrate]:
            prob = sample / 100000
            error_probabilities.setdefault(bitrate, []).append(prob)

    # Compute the channel capacities
    capacities = {}
    for bitrate in error_probabilities.keys():
        for error in error_probabilities[bitrate]:
            cap = bitrate * (1 - binary_entropy(error))
            capacities.setdefault(bitrate, []).append(cap)

    # Assemble into lists for plotting
    raw_bitrate = list(sorted(error_probabilities.keys()))
    capacity = [np.mean(capacities[br]) for br in raw_bitrate]
    capacity_std = [np.std(capacities[br]) for br in raw_bitrate]

    error_probability = [np.mean(error_probabilities[br]) for br in raw_bitrate]
    error_probability_std = [np.std(error_probabilities[br]) for br in raw_bitrate]

    # Plot the channel capacity / error probability plot
    plt.rcParams["figure.figsize"] = (6.4, 2.2)
    fig, ax1 = plt.subplots()

    color = 'tab:blue'
    ax1.set_xlabel('Raw bandwidth (Mbps)')
    ax1.set_ylabel('Capacity (Mbps)', color=color)
    line1 = ax1.errorbar(raw_bitrate, capacity, yerr=capacity_std, color=color, marker="s", label="Capacity", capsize=2, linestyle='--')
    ax1.tick_params(axis='y', labelcolor=color)

    ax2 = ax1.twinx()  # instantiate a second axes that shares the same x-axis

    color = 'tab:orange'
    ax2.set_ylabel('Error probability', color=color)  # we already handled the x-label with ax1
    line2 = ax2.errorbar(raw_bitrate, error_probability, yerr=error_probability_std, color=color, marker=".", label="Error probability", linestyle='--', capsize=2)
    ax2.tick_params(axis='y', labelcolor=color)

    lines = [line2, line1]
    labels = [l.get_label() for l in lines]
    ax1.legend(lines, labels, loc="upper left", frameon=True)

    plt.grid()
    plt.tight_layout()
    plt.savefig("plot/capacity-plot.pdf")
    plt.clf()

    print(capacity)
    print(error_probability)


if __name__ == "__main__":
    main()
