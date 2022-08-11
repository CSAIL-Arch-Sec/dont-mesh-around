import argparse
import glob
import multiprocessing
import os
import pickle
import statistics
import subprocess
from distutils.dir_util import copy_tree, remove_tree
from distutils.file_util import copy_file
from multiprocessing import Process

import matplotlib.pyplot as plt
import numpy as np
import psutil
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import (accuracy_score, f1_score, precision_score,
                             recall_score)
from sklearn.model_selection import train_test_split
from sklearn.multiclass import OneVsRestClassifier


# -------------------------------------------------------------------------------------------------------------------
# Utility Functions
# -------------------------------------------------------------------------------------------------------------------
def moving_average(x, N):
    cumsum = np.cumsum(np.insert(x, 0, 0))
    return (cumsum[N:] - cumsum[:-N]) / float(N)


# 1-column file -> array of int
def parse_file_1c(fn):
    with open(fn) as f:
        lines = [int(line.strip()) for line in f]
    return np.array(lines)


# -------------------------------------------------------------------------------------------------------------------
# Plotting Functions
# -------------------------------------------------------------------------------------------------------------------

# Plots two traces to visualize the diff: zero on the left and one on the right
def plot_one_vs_zero(traces_bit_tuples, plot_id="0"):
    low_thres = 38		# FIXME: change to other ranges if needed
    high_thres = 120  # FIXME: change to other ranges if needed

    # Parse traces and filter out outliers
    trace_0_dict = {}
    trace_1_dict = {}
    for traces_bit_tuple in traces_bit_tuples:
        trace = traces_bit_tuple[0]
        actual_bit = traces_bit_tuple[1]
        for i, t in enumerate(trace):
            if t >= low_thres and t <= high_thres:
                if (actual_bit == "0"):
                    trace_0_dict.setdefault(i, []).append(t)
                else:
                    trace_1_dict.setdefault(i, []).append(t)

    # For each x, compute average value of y
    trace_0 = []
    trace_0_b = []
    for i, v in trace_0_dict.items():
        if (i == 0):    # Skip very first sample
            continue
        avg = np.mean(v)
        std = np.std(v)
        trace_0.append(avg)
        trace_0_b.append(std)

    # For each x, compute average value of y
    trace_1 = []
    trace_1_b = []
    for i, v in trace_1_dict.items():
        if (i == 0):    # Skip very first sample
            continue
        avg = np.mean(v)
        std = np.std(v)
        trace_1.append(avg)
        trace_1_b.append(std)

    # Change to True if you want to plot the moving average
    # FIXME: Turn on moving average
    if (True):
        window = 8
        trace_0_avg = moving_average(trace_0, window)
        trace_1_avg = moving_average(trace_1, window)
        trace_0_b_avg = moving_average(trace_0_b, window)
        trace_1_b_avg = moving_average(trace_1_b, window)
    else:
        trace_0_avg = np.array(trace_0)
        trace_1_avg = np.array(trace_1)
        trace_0_b_avg = np.array(trace_0_b)
        trace_1_b_avg = np.array(trace_1_b)

    # Prepare figure
    plt.figure(figsize=(6.4, 2))
    xmax = min(len(trace_0_avg), len(trace_1_avg)) - 1
    ymin = min(min(trace_0_avg), min(trace_1_avg))
    ymax = max(max(trace_0_avg), max(trace_1_avg))

    # Plot data to figure
    i = 1
    for mean_std_tuple in [(trace_0_avg, trace_0_b_avg), (trace_1_avg, trace_1_b_avg)]:
        samples, std = mean_std_tuple[0], mean_std_tuple[1]
        plt.subplot(1, 2, i)
        plt.plot(samples)

        # FIXME: Turn on STD
        if (False):
            plt.fill_between(range(len(samples)), samples-std, samples+std, alpha=0.2)

        plt.xlim(0, xmax)
        plt.ylim(ymin, ymax)
        plt.grid(True, which='both')

        plt.xlabel("Latency sample ID")
        if (i == 1):
            plt.title("Bit = 0")
            plt.ylabel('Load latency (cycles)')
        else:
            plt.title("Bit = 1")
        i += 1

    # Save figure to disk
    figname = 'plot/plot-side-channel-{}.pdf'.format(plot_id)
    print('plotting', figname)
    plt.tight_layout()
    plt.savefig(figname)
    plt.close()


# -------------------------------------------------------------------------------------------------------------------
# Data Collection Functions
# -------------------------------------------------------------------------------------------------------------------

# Collects $(runs_per_iteration) samples for the given $(target_iteration) of the victim
def collect(target_iteration, runs_per_iteration):
    if target_iteration <= 0:
        print("Iteration number should be greater than 0")
        exit(0)

    # Delete previous output files
    prev_files = glob.glob("out/*.out")
    for x in prev_files:
        os.remove(x)

    # Run monitor (attacker) until it succeeds:
    monitor_err = 1
    trials = 0
    while (monitor_err != 0 and trials < 3):
        cl = ['./bin/mesh-monitor', str(monitor_coreno), str(monitor_sliceno), str(runs_per_iteration), str(target_iteration)]
        print(cl)
        monitor_popen = subprocess.Popen(cl)

        # Wait for monitor to complete (FIXME: tune timeout if necessary)
        try:
            monitor_err = monitor_popen.wait(timeout=600)
        except:
            print('monitor out of time')
            monitor_err = -1
        print('monitor returned %d.' % (monitor_err))

        trials += 1

    if (trials == 3):
        exit(0)

    # Save output into the desired directory
    try:
        remove_tree("data-single-bit")
    except:
        pass
    os.makedirs("data-single-bit")

    files = glob.glob("out/*.out")
    for x in files:
        copy_file(x, "data-single-bit")


# Collects $(runs_per_iteration_train) training samples for all iterations of the victim
# Also collects $(runs_per_iteration_test) testing samples for all iterations of the victim
def full_key_recovery_collect(runs_per_iteration_train, runs_per_iteration_test, bit_length):

    # Create output folders
    try:
        os.makedirs('out-train')
    except:
        pass
    try:
        os.makedirs('out-test')
    except:
        pass

    # Delete previous output files
    prev_files = glob.glob("out-train/*.out")
    for x in prev_files:
        os.remove(x)

    # Delete previous output files
    prev_files = glob.glob("out-test/*.out")
    for x in prev_files:
        os.remove(x)

    # Run monitor (attacker) until it succeeds:
    monitor_err = 1
    trials = 0
    while (monitor_err != 0 and trials < 3):
        cl = ['./bin/mesh-monitor-full-key-per-iteration', str(monitor_coreno), str(monitor_sliceno), str(runs_per_iteration_train), str(runs_per_iteration_test), str(bit_length)]
        print(cl)
        monitor_popen = subprocess.Popen(cl)

        # Wait for monitor to complete (FIXME: tune timeout if necessary)
        try:
            monitor_err = monitor_popen.wait(timeout=200000)
        except:
            print('monitor out of time')
            monitor_err = -1
        print('monitor returned %d.' % (monitor_err))

        trials += 1

    if (trials == 2):
        print("Monitor failed after %d trials. Giving up" % trials)
        exit(1)

    # Save output into the desired directory
    try:
        remove_tree("data-fkr-train")
    except:
        pass
    # Save output into the desired directory
    try:
        remove_tree("data-fkr-test")
    except:
        pass

    copy_tree("out-train", "data-fkr-train")
    copy_tree("out-test", "data-fkr-test")


# -------------------------------------------------------------------------------------------------------------------
# Parsing Functions
# -------------------------------------------------------------------------------------------------------------------

def get_padded_trace(trace, lowerbound):
    return list(trace) + [np.mean(trace[-1 - int(lowerbound / 10):-1])] * (lowerbound - len(trace))
    # return list(trace) + [trace[-1]] * (lowerbound - len(trace))


# Parses data from the given directory
def parse(directory, iteration_index=""):

    # Prepare to read data from the experiments
    out_files = sorted(glob.glob(directory + ("/*_data_%s*.out" % iteration_index)))
    traces_bit_tuples = []

    # Read the traces
    for f in out_files:

        # Parse actual bit
        parts = f.split('/')[-1].split('.')[0].split('_')
        actual_bit = parts[3]

        # Parse file
        trace = parse_file_1c(f)  # this array contains all the samples
        traces_bit_tuples.append((trace, actual_bit))

    # First, count how many zeros and ones there are in the data parsed
    # This is across many different cryptographic keys
    ones = 0
    zeros = 0
    all_lengths_zeros = []
    all_lengths_ones = []
    for traces_bit_tuple in traces_bit_tuples:
        trace = traces_bit_tuple[0]
        actual_bit = traces_bit_tuple[1]

        if (actual_bit == "0"):
            all_lengths_zeros.append(len(trace))
            zeros += 1
        else:
            all_lengths_ones.append(len(trace))
            ones += 1

    print("Data has", ones, "ones and", zeros, "zeros")

    # Exclude any iterations that did not have both ones and zeros
    # For example, the first iteration always only has ones
    if zeros == 0 or ones == 0:
        print("Skipping because it is either all ones or all zeros")
        return [], [], 0

    # Find median number of samples for zero and one traces
    median_length_zeros = statistics.median(all_lengths_zeros) - 1
    median_length_ones = statistics.median(all_lengths_ones) - 1

    print("Median length 0 is", median_length_zeros)
    print("Median length 1 is", median_length_ones)

    # Set the lower as the length of a vector for our classifier
    lowerbound = int(min(int(median_length_zeros), int(median_length_ones)))

    # Preprocess the traces so that they all have the same length
    preprocessed_traces = []
    labels = []
    for traces_bit_tuple in traces_bit_tuples:
        trace, actual_bit = traces_bit_tuple[0], traces_bit_tuple[1]

        # Exclude traces that are way too short
        if len(trace) < 5:
            continue

        # Pad traces that are too short
        if (len(trace) < lowerbound):
            trace = get_padded_trace(trace, lowerbound)

        # Add padded or cut trace to preprocessed_traces
        preprocessed_traces.append(trace[:lowerbound])
        labels.append(actual_bit)

    return preprocessed_traces, labels, lowerbound

# -------------------------------------------------------------------------------------------------------------------
# ML Train Functions
# -------------------------------------------------------------------------------------------------------------------


def f_pool(name, model, X_train, y_train, X_test, y_test, return_dict):
    # print("Training with", name)
    model.fit(X_train, y_train)
    score = model.score(X_test, y_test)     # This computes the accuracy
    predictions = model.predict(X_test)
    precision = precision_score(y_test, predictions, pos_label="1")
    recall = recall_score(y_test, predictions, pos_label="1")
    return_dict[name] = (model, score, precision, recall)
    print(name, score, precision, recall)


def train(directory, savemodel=0):
    preprocessed_traces, labels, lowerbound = parse(directory)

    # Plot the difference between zeros and ones just for visualization
    preprocessed_traces_bit_tuples = []
    for preprocessed_trace, label in zip(preprocessed_traces, labels):
        preprocessed_traces_bit_tuples.append((preprocessed_trace, label))

    plot_one_vs_zero(preprocessed_traces_bit_tuples)

    # Get train test for this iteration_index
    X_train, X_test, y_train, y_test = train_test_split(preprocessed_traces, labels)

    print("Shape of train is", np.array(X_train).shape)
    print("Shape of test is", np.array(X_test).shape)

    classifiers = [("Random Forest", RandomForestClassifier())]

    # Train the classifier
    processes = []
    manager = multiprocessing.Manager()
    return_dict = manager.dict()
    for name, model in classifiers:
        p = Process(target=f_pool, args=(name, model, X_train, y_train, X_test, y_test, return_dict))
        processes.append(p)
        p.start()

    # Wait for classifiers to end
    for p in processes:
        p.join()

    # Pick the best classifier
    best_score = 0
    recall = 0
    precision = 0
    best_classifier = ""
    for name, _ in classifiers:
        model, score, prec, rec = return_dict[name]
        if score > best_score:
            best_score = score
            precision = prec
            recall = rec
            best_classifier = name
            best_model = model

    print("Final classifier for iteration =", best_classifier, "with accuracy =", best_score, "precision =", precision, "recall =", recall)

    # Save model to file
    if savemodel == 1:
        with open(("models/model.pickle"), "wb") as fp:  # Pickling
            pickle.dump(best_model, fp)
        with open(("models/input_len.pickle"), "wb") as fp:  # Pickling
            pickle.dump(lowerbound, fp)


# -------------------------------------------------------------------------------------------------------------------
# ML Test Functions (used for full key recovery)
# -------------------------------------------------------------------------------------------------------------------

def full_key_recovery_test(exclude_first_bit=0):

    # Read model and input len from trained classifier
    with open(("models/model.pickle"), "rb") as fp:  # Pickling
        model = pickle.load(fp)
    with open(("models/input_len.pickle"), "rb") as fp:  # Pickling
        lowerbound = pickle.load(fp)

    # Find number of iterations for test key (from ground truth)
    out_files = glob.glob("data-fkr-test/0002_data*.out")
    test_key_total_iterations = 0
    for f in out_files:
        parts = f.split('/')[-1].split('.')[0].split('_')
        iteration_index = int(parts[2])
        if iteration_index > test_key_total_iterations:
            test_key_total_iterations = iteration_index

    # Init variables
    samples_score_dict = {}
    samples_total_dict = {}
    trace_lens_all_bits = {}

    # Parse the samples for each iteration of the victim
    for iteration_index in range(1, test_key_total_iterations + 1):

        if exclude_first_bit == 1 and iteration_index == 1:
            continue

        # Pick the respective files
        files = glob.glob("data-fkr-test/*_data_%04d_*.out" % iteration_index)

        # Read the files with data from this iteration
        all_traces = []
        actual_bit = files[0].split('/')[-1].split('.')[0].split('_')[3]
        for f in files:
            trace = parse_file_1c(f)  # this array contains all the samples
            bit = f.split('/')[-1].split('.')[0].split('_')[3]
            if actual_bit != bit:
                print("ERROR! Testing with different keys")
                exit(1)

            # Safety check here that we passed the right argument
            all_traces.append(trace)

        # Make the traces all the same length and filter out outliers
        preprocessed_traces = []
        trace_lens = []
        for trace in all_traces:
            trace_lens.append(len(trace))

            # Exclude traces that are way too short
            if len(trace) < 5:
                continue

            # Pad traces that are too short
            if (len(trace) < lowerbound):
                trace = get_padded_trace(trace, lowerbound)

            preprocessed_traces.append(trace[:lowerbound])

        # Save the median length of the traces for logging purposes
        median_len = np.median(trace_lens)
        trace_lens_all_bits.setdefault(actual_bit, []).append(median_len)

        # Get predictions
        predictions = model.predict(preprocessed_traces)

        # Parse predictions
        correct_count = 0
        incorrect_count = 0
        for i, prediction in enumerate(predictions, 1):

            # If more than half of the predictions are correct, then we consider the majority vote correct
            # If exactly half, then we just do not count the last prediction to make the number odd
            right = 0
            if prediction == actual_bit:
                correct_count += 1
                if correct_count > incorrect_count:
                    right = 1
            else:
                incorrect_count += 1
                if correct_count >= incorrect_count:
                    right = 1
            if right == 1:
                samples_score_dict.setdefault(i, 0)
                samples_score_dict[i] += 1

            samples_total_dict.setdefault(i, 0)
            samples_total_dict[i] += 1

        # If after using all the data for this iteration we did not have a correct prediction
        if right == 0:
            print("Iteration", iteration_index, "predicted wrong")  # . Should be", actual_bit, "but got", predictions)
            print("Trace median len", median_len)
            print("Correct count", correct_count, "and incorrect count", incorrect_count)

        # Plot to see the result anyway
        # plot_single_iteration(preprocessed_traces, iteration_index, actual_bit, right)

    # Print len for debugging purposes
    for key, vals in trace_lens_all_bits.items():
        print(vals)
        print("Median len for", key, "is", np.median(vals))

    # Print the final result by number of votes
    for no_traces, score in sorted(samples_score_dict.items()):
        print("%d %d out of %d (%d%%)" % (no_traces, score, samples_total_dict[no_traces], score / samples_total_dict[no_traces] * 100))


# -------------------------------------------------------------------------------------------------------------------
# Orchestrating Functions for Full Key Recovery, one classifier per iteration
# NOTE: We did not end up using this code in the paper beucause a single classifier ended up being enough
# -------------------------------------------------------------------------------------------------------------------

def per_iteration_work(iteration_index, traces_bit_tuples):

    # First, count how many zeros and ones there are in the data collected for this iteration
    # This is across many different cryptographic keys
    ones = 0
    zeros = 0
    all_lengths_zeros = []
    all_lengths_ones = []
    for traces_bit_tuple in traces_bit_tuples:
        trace = traces_bit_tuple[0]
        actual_bit = traces_bit_tuple[1]

        if (actual_bit == "0"):
            all_lengths_zeros.append(len(trace))
            zeros += 1
        else:
            all_lengths_ones.append(len(trace))
            ones += 1

    print("Iteration", iteration_index, "has", ones, "ones and", zeros, "zeros")

    # Exclude any iterations that did not have both ones and zeros
    # For example, the first iteration always only has ones
    if zeros == 0 or ones == 0:
        print("Skipping iteration", iteration_index, "because it is all ones")
        return

    # Find median number of samples for zero and one traces
    median_length_zeros = statistics.median(all_lengths_zeros) - 1
    median_length_ones = statistics.median(all_lengths_ones) - 1

    # Set the lower as the length of a vector for our classifier
    lowerbound = int(min(int(median_length_zeros), int(median_length_ones)))
    # lowerbound = min(min(all_lengths_zeros), min(all_lengths_ones))

    # Preprocess the traces so that they all have the same length
    preprocessed_traces = []
    labels = []
    for traces_bit_tuple in traces_bit_tuples:
        trace, actual_bit = traces_bit_tuple[0], traces_bit_tuple[1]

        # Exclude traces that are way too short
        if len(trace) < 5:
            continue

        # Pad traces that are too short
        if (len(trace) < lowerbound):
            trace = get_padded_trace(trace, lowerbound)

        preprocessed_traces.append(trace[:lowerbound])
        labels.append(actual_bit)

    preprocessed_traces_bit_tuples = []
    for preprocessed_trace, label in zip(preprocessed_traces, labels):
        preprocessed_traces_bit_tuples.append((preprocessed_trace, label))

    # Plot the difference between zeros and ones just for visualization
    plot_id = "%04d" % iteration_index
    plot_one_vs_zero(preprocessed_traces_bit_tuples, plot_id)

    # Get train test for this iteration_index
    X_train, X_test, y_train, y_test = train_test_split(preprocessed_traces, labels)

    print("Shape of train is", np.array(X_train).shape)
    # print("Shape of test is", np.array(X_test).shape)

    classifiers = [
        ("Random Forest", RandomForestClassifier()),
    ]

    # Train the classifier
    processes = []
    manager = multiprocessing.Manager()
    return_dict = manager.dict()
    for name, model in classifiers:
        p = Process(target=f_pool, args=(name, model, X_train, y_train, X_test, y_test, return_dict))
        processes.append(p)
        p.start()

    for p in processes:
        p.join()

    # Pick the best classifier
    best_score = 0
    best_classifier = ""
    for name, _ in classifiers:
        model, score = return_dict[name]
        if score > best_score:
            best_score = score
            best_classifier = name
            best_model = model

    print("Final classifier for iteration", iteration_index, "=", best_classifier, "with accuracy =", best_score)

    # Save model to file
    with open(("models/model_%04d.pickle" % iteration_index), "wb") as fp:  # Pickling
        pickle.dump(best_model, fp)
    with open(("models/input_len_%04d.pickle" % iteration_index), "wb") as fp:  # Pickling
        pickle.dump(lowerbound, fp)


def train_per_single_iteration():

    # Prepare to read data from the experiments
    out_files = sorted(glob.glob("./data-fkr-train/*_data*.out"))
    all_traces_dict = {}
    rept_len = {}

    # Find number of iterations for each rept (each rept is a diff key)
    for f in out_files:
        parts = f.split('/')[-1].split('.')[0].split('_')
        rept_index = int(parts[0])
        iteration_index = int(parts[2])
        rept_len.setdefault(rept_index, 0)
        if iteration_index > rept_len[rept_index]:
            rept_len[rept_index] = iteration_index

    # Read the traces
    for f in out_files:

        # Parse bit number
        parts = f.split('/')[-1].split('.')[0].split('_')
        rept_index = int(parts[0])

        iteration_index = rept_len[rept_index] - int(parts[2]) + 1
        actual_bit = parts[3]

        # Parse file
        trace = parse_file_1c(f)  # this array contains all the samples
        all_traces_dict.setdefault(iteration_index, []).append((trace, actual_bit))

    # Now process the results of the experiments
    processes = []
    for iteration_index, traces_bit_tuples in all_traces_dict.items():
        p = Process(target=per_iteration_work, args=(iteration_index, traces_bit_tuples))
        processes.append(p)
        p.start()

    for p in processes:
        p.join()


# -------------------------------------------------------------------------------------------------------------------
# Miscellaneous
# -------------------------------------------------------------------------------------------------------------------

def analytical_model_verification():
    """
    Pin the receiver on each core and use the best receiver slice.

    Sender is on CHA 0.
    """
    best_slice = {
        1: 25,
        2: 1,
        4: 23,
        5: 23,
        6: 25,
        7: 1,
        8: 2,
        9: 15,
        10: 23,
        11: 8,
        12: 1,
        13: 6,
        14: 6,
        15: 8,
        16: 2,
        17: 10,
        18: 11,
        19: 12,
        20: 1,
        21: 2,
        22: 6,
        23: 8,
        24: 1
    }
    global monitor_coreno
    global monitor_sliceno
    for core in range(26):
        # Can't pin a core to CHA 3 or 25
        # CHA 0 is the sender
        if core in (0, 3, 25):
            continue
        monitor_coreno = core
        monitor_sliceno = best_slice[monitor_coreno]
        collect(target_iteration, 5000)
        train("data-single-bit")


def is_process_running(processName):
    # Iterate over the all the running process
    for proc in psutil.process_iter():
        try:
            # Check if process name contains the given name string.
            if processName in proc.cmdline():
                return True
        except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
            pass

    # print("Run the victim in the background first")
    return False


# -------------------------------------------------------------------------------------------------------------------
# This is the orchestrator
# -------------------------------------------------------------------------------------------------------------------
if __name__ == '__main__':

    # Set configuration
    monitor_coreno = 9
    monitor_sliceno = 13

    ecdsa_path = './victim/libgcrypt-1.6.3/tests/mesh-victim'
    rsa_path = './victim/libgcrypt-1.5.2/tests/mesh-victim'

    victim_path = rsa_path
    if (is_process_running(rsa_path)):
        victim_path = rsa_path
    elif (is_process_running(ecdsa_path)):
        victim_path = ecdsa_path
    else:
        print("Neither victim is running. Assuming", victim_path)

    # Parse arguments
    parser = argparse.ArgumentParser(description='Orchestrate monitor and victim execution.')

    # Single bit
    parser.add_argument('--collect', type=int, default=0)
    parser.add_argument('--parse', action='store_true')
    parser.add_argument('--train', action='store_true')
    parser.add_argument('--plot', action='store_true')

    # Full key
    parser.add_argument('--fullkeyrecoverycollect', nargs=2)
    parser.add_argument('--fullkeyrecoverytrain', action='store_true')
    parser.add_argument('--fullkeyrecoverytest', action='store_true')

    # Analytical Model Verification
    parser.add_argument('--analyticalmodelverify', action='store_true')
    args = parser.parse_args()

    # Prepare output directories
    try:
        os.makedirs('data')
    except:
        pass
    try:
        os.makedirs('data/ecdsa-0')
    except:
        pass
    try:
        os.makedirs('data/ecdsa-1')
    except:
        pass
    try:
        os.makedirs('data/rsa-0')
    except:
        pass
    try:
        os.makedirs('data/rsa-1')
    except:
        pass
    try:
        os.makedirs('plot')
    except:
        pass
    try:
        os.makedirs('models')
    except:
        pass

    if victim_path == ecdsa_path:
        target_iteration = 4    # iteration 1 is always 1 in ECDSA
        bit_length = 256
        exclude_first_bit = 1   # first bit in ECDSA is always 1
    elif victim_path == rsa_path:
        target_iteration = 1
        bit_length = 1024
        exclude_first_bit = 0

    # Run the orchestrator
    if args.collect:
        no_victim_runs = args.collect
        collect(target_iteration, no_victim_runs)

    # Plot parsed data
    if args.plot:
        # Plot the difference between zeros and ones just for visualization
        preprocessed_traces, labels, lowerbound = parse("data-single-bit")
        preprocessed_traces_bit_tuples = []
        for preprocessed_trace, label in zip(preprocessed_traces, labels):
            preprocessed_traces_bit_tuples.append((preprocessed_trace, label))

        plot_one_vs_zero(preprocessed_traces_bit_tuples)

    # Train and test classifier on the data
    if args.train:
        train("data-single-bit")

    # Collect data for full key recovery
    if args.fullkeyrecoverycollect:

        runs_per_iteration_train = int(args.fullkeyrecoverycollect[0])
        runs_per_iteration_test = int(args.fullkeyrecoverycollect[1])

        print("%d runs train %d runs test" % (runs_per_iteration_train, runs_per_iteration_test))

        full_key_recovery_collect(runs_per_iteration_train, runs_per_iteration_test, bit_length)

    # Test data for full key recovery
    if args.fullkeyrecoverytrain:
        train("data-fkr-train", savemodel=1)

    # Test data for full key recovery
    if args.fullkeyrecoverytest:
        full_key_recovery_test(exclude_first_bit)

    # Get accuracy for receiver on each core
    if args.analyticalmodelverify:
        analytical_model_verification()
