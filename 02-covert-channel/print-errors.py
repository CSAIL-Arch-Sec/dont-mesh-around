import argparse
import multiprocessing as mp
from collections import namedtuple

import numpy as np

ParseParams = namedtuple('ParseParams', 'interval offset contention_frac threshold score')
interval = None
result_x = None
result_y = None
SCORE_MAX = 9999999999999
pattern = "1110011001010000110111110101011110001001111001010001100011110011100100011101110010010100100100011001110101111010111110100010000100100111001111100111011010110110011011011000011010001010000101110010010110001010110000001001111111001111010111111001111111000100000000100011011101011000001100010000000110000011001101101111010100011000100110100011001000100011011000010100100011100101011010011000110011001100001101101001101111011110000100001010001100001100000010111111110111110100110011100000011101001100110011001010101011011101000101110111101000001000110101110000100100010110110100101101001100110101110011000010111011010111111100001101011000000000011101011101000111101111110110010100010000101001100110000010000011111100101101010110001111011111100000001110110100011000011010101100111010100100101100000011000100101011000101111001011011111101101011010100111000101000101111101000111101001111100101100010011111111000100011010101101010100001110000011101011000001101100010100100001110000000100100000000000010100000"
patternlen = len(pattern)

# The first 100 intervals are discarded
# The following 1000 intervals are the training set
# The following 100000 intervals are the testing set
discard_intervals = 100
train_intervals = 1000
test_intervals = 100000

discard_intv_start = 0
discard_intv_end = discard_intervals
train_intv_start = discard_intervals
train_intv_end = train_intv_start + train_intervals
test_intv_start = discard_intervals + train_intervals
test_intv_end = test_intv_start + test_intervals

def read_from_file(filename):
    """Read a 2-column receiver trace file."""
    result_x = []
    result_y = []
    with open(filename) as f:
        for line in f:
            x, y = line.strip().split()
            result_x.append(int(x))
            result_y.append(int(y))
    return result_x, result_y


def diff_letters(a, b):
    return sum(a[i] != b[i] for i in range(len(a)))


def parse_intervals_into_bits(intervals, threshold, min_contention_frac):
    """Classify each interval as a bit 1 or bit 0.
    
    For each interval, if more than min_contention_frac of the samples are
    greater than the threshold, then the sample is a 1.
    """
    result = ""
    for _, samples in intervals.items():
        contention = 0
        for sample in samples:
            if sample >= threshold:
                contention += 1
        if (contention > min_contention_frac * len(samples)):
            result += "1"
        else:
            result += "0"
    return result

def per_offset_worker(parse_params):
    """For a given offset, find the optimal threshold and contention_frac values.
    
    This worker is launched in the multiprocessing stage of the post-processing.
    The worker is provided with the interval and an offset value which describes
    how many cycles the trace should be shifted by to align with the bit
    boundary. parse_params is a ParseParams named tuple that contains the
    particular set of parameters used for parsing.
    """
    # Parse trace into intervals
    offset = parse_params.offset
    interval = parse_params.interval

    cur_interval_no = 0
    train_intervals = {}
    for i in range(len(result_x)):
        x = result_x[i]
        y = result_y[i]
        if ((x + offset) > interval * (cur_interval_no + 1)):
            cur_interval_no += 1

        if (discard_intv_start <= cur_interval_no < discard_intv_end):
            continue
        if (train_intv_start <= cur_interval_no < train_intv_end):
            train_intervals.setdefault(cur_interval_no, []).append(y)
        else:
            break

    # We use the training set to find the best threshold for this interval
    # (offline) and the remaining parsed data to get the error rate (online). We
    # try different thresholds because, depending on the CC interval, the best
    # threshold to use is different (e.g., with larger intervals we can use a
    # lower threshold than with shorter intervals).
    # We try different fractions of contention samples observed (e.g. 10% of the
    # samples must show contention for the bit to be counted as a 1)
    thresholds = range(70, 120, 2)	# FIXME: adjust these thresholds for your CPU
    contention_fracs = range(1, 70, 4) # test from 0.01 to 0.7 in intervals of 0.04

    best_contention_frac = None
    best_threshold = None
    best_score = SCORE_MAX

    for min_contention_frac in contention_fracs:
        for threshold in thresholds:
            # Parse the intervals into bits
            result = parse_intervals_into_bits(train_intervals, threshold, min_contention_frac / 100)

            if random_pattern:
                # Because we don't know at what point in the random sequence of
                # bits we started sampling, we need to test against all possible
                # shifts of the pattern. This is *much* faster in numpy, so we
                # convert to np arrays for this step.
                score = patternlen

                nppattern = np.array([int(i) for i in pattern])
                result = np.array([int(i) for i in result])
                for i in range(patternlen):
                    # This is what the numpy function below is effectively doing:
                    # candidate = pattern[i:] + pattern[:i]
                    # newscore = diff_letters(result, candidate)
                    newscore = np.count_nonzero(np.roll(nppattern, i) != result)
                    if (newscore < score):
                        score = newscore
            else:
                # Compare these bits with the ground truth
                # The ground truth is either 0101... or 1010...
                candidate_1 = "01" * (len(result) // 2)
                candidate_2 = "10" * (len(result) // 2)

                # Get the number of bit flips between the decoded stream and the
                # (correct) ground truth
                score_1 = diff_letters(result, candidate_1)
                score_2 = diff_letters(result, candidate_2)
                score = min(score_1, score_2)
            
            # Pick the best score
            if (score <= best_score):
                best_threshold = threshold
                best_contention_frac = min_contention_frac / 100
                best_score = score
    return ParseParams(interval, offset, best_contention_frac, best_threshold, best_score)

def main():
    global interval, result_x, result_y, random_pattern

    parser = argparse.ArgumentParser()
    parser.add_argument('result_path', help='Path to the receiver trace')
    parser.add_argument('interval', help='Interval used in the covert channel run', type=int)
    parser.add_argument('--random_pattern',
        help='Expect random bits rather than alternating bits',
        action='store_true',
        default=False
    )
    args = parser.parse_args()

    result_x, result_y = read_from_file(args.result_path)
    interval = args.interval
    random_pattern = args.random_pattern

    # Parse trace into intervals
    pool = mp.Pool(processes=40)
    offsets = range(0, interval // 2, interval // 80)
    params = [ParseParams(interval, o, None, None, None) for o in offsets]

    best_params_per_offset = pool.map(per_offset_worker, params)

    # Select the best parameter
    best_params = min(best_params_per_offset, key=lambda x: x.score)
    best_offset = best_params.offset
    best_threshold = best_params.threshold
    best_contention_frac = best_params.contention_frac

    # print('Best params: {}'.format(best_params))

    # Generate test set
    cur_interval_no = 0
    test_intervals = {}
    for i in range(len(result_x)):
        x = result_x[i]
        y = result_y[i]
        if ((x + best_offset) > interval * (cur_interval_no + 1)):
            cur_interval_no += 1

        if (test_intv_start <= cur_interval_no < test_intv_end):
            test_intervals.setdefault(cur_interval_no, []).append(y)
        elif cur_interval_no > test_intv_end:
            break
        else:
            continue

    # Parse the intervals into bits
    result = parse_intervals_into_bits(test_intervals, best_threshold, best_contention_frac)

    if random_pattern:
        score = SCORE_MAX
        best_offset = 0

        # Find the best offset using the first patternlen intervals
        nppattern = np.array([int(i) for i in pattern])
        npresult = np.array([int(i) for i in result])
        for i in range(patternlen):
            newscore = np.count_nonzero(np.roll(nppattern, i) != npresult[:patternlen])
            if (newscore < score):
                best_offset = i
                score = newscore
        
        # Evaluate on the entire collected result
        extended_pattern = np.tile(nppattern, len(result) // patternlen + 1)[:len(result)]
        score = np.count_nonzero(np.roll(extended_pattern, best_offset) != npresult)
    else:
        # Compare these bits with the ground truth
        # The ground truth is either 0101... or 1010...
        candidate_1 = "01" * ((len(result) // 2) + 1)
        candidate_2 = "10" * ((len(result) // 2) + 1)

        # Print the number of bit flips between the
        # decoded stream and the (correct) ground truth
        score_1 = diff_letters(result, candidate_1[:len(result)])
        score_2 = diff_letters(result, candidate_2[:len(result)])
        score = min(score_1, score_2)

    # print('Errors: {}/{} ({}%)'.format(score, len(result), score * 100 / len(result)))
    print(score)


if __name__ == "__main__":
    main()
