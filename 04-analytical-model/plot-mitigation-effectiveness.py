from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns

from predict_contention import *


def _get_sorted_scores(vic_core):
    rx_scores = {}
    for rx_core in CORES:
        # do not pin to the same core as the victim
        if rx_core == vic_core: continue 
        for rx_slice in SLICES:
            # do not test a length-0 victim
            if rx_slice == rx_core: continue 

            score = 0
            for vic_slice in SLICES:
                config_score = get_config_contention(vic_core, vic_slice, rx_core, rx_slice)
                score += config_score
            rx_scores[(rx_core, rx_slice)] = score
    sorted_rx_scores = sorted(rx_scores.items(), key=lambda kv: kv[1], reverse=True)
    return sorted_rx_scores

def get_csv_with_mitigation_blocking_N_cores():
    """Simulate blocking the N most vulnerable cores.
    
    This experiment considers all possible victim placements and the maximum
    vulnerability score observable by an attacker if N cores are blocked for all
    possible N.
    This function prints the data for Figure 14 that is shown in Table 3.
    It also returns an np.array where the first column is the victim core
    placement and every column afterwards is the maximum observable
    vulnerability score if N cores are blocked.
    """
    print('Victim Core', end='')
    for num in range(25):
        print(f', N={num}', end='')
    print('')

    blocking_effect = list()

    for vic_core in CORES:
        print(f'{vic_core}', end="")
        curr_core_blocking_effect = [vic_core]

        # Get sorted receivers
        sorted_scores = _get_sorted_scores(vic_core)

        for no in range(25):

            # Get sorted *unique* cores of the receivers
            sorted_rx_cores = []
            for rx, score in sorted_scores:
                rx_core = rx[0]
                if rx_core not in sorted_rx_cores:
                    sorted_rx_cores.append(rx_core)

            # Now find the best receiver but with some cores blocked
            max_score = 0
            for rx, score in sorted_scores:
                rx_core = rx[0]
                if rx_core not in sorted_rx_cores[:no]:
                    max_score = score
                    break

            # Print the best score under the constraint of the mitigation
            print(f', {max_score}', end='')
            curr_core_blocking_effect.append(max_score)
        print('')
        blocking_effect.append(curr_core_blocking_effect)
    
    return np.array(blocking_effect)

def make_stripplot(blocking_effect):
    """Produce a stripplot showing how vulnerability scores change as more cores are blocked."""
    Path('plot').mkdir(exist_ok=True)
    output_filename = 'plot/mitigation-effect.pdf' # use the same name as in the document to make uploading to overleaf easier

    blocking_effect = blocking_effect[:, 1:] # strip off the left column
    x = list(range(blocking_effect.shape[1])) 
    cols = blocking_effect.shape[1]
    df = pd.DataFrame({ i: blocking_effect[:, i] for i in range(cols) })

    plt.rcParams["figure.figsize"] = (6, 2)
    ax = sns.stripplot(data=df, jitter=1, size=3, color='C0')
    ax.set_xlabel('Number of reserved cores')
    ax.set_ylabel('Vulnerability score')

    plt.plot(x, np.mean(blocking_effect, axis=0), 'C1-', alpha=0.5, linewidth=3, label='Mean vuln score')
    # plt.grid(axis='y')
    plt.legend()

    plt.tight_layout()
    plt.savefig(output_filename)
    plt.close()

def main():
    blocking_effect = get_csv_with_mitigation_blocking_N_cores()
    make_stripplot(blocking_effect)

if __name__ == '__main__':
    main()
