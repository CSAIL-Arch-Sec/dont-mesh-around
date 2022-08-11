import logging
import os
import re
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from predict_contention import CORES, SLICES, get_config_contention
from utils import print_coord


def analytical_model_verification():
    """For the victim on core 0, find the best attacker slice for each attacker core.
    
    Returns a dictionary mapping each attacker placement to its vulnerability score.
    """

    vuln_score_dict = dict()
    vic_core = 0
    for attacker_core in CORES:
        max_score = 0
        best_attacker = None # track the best attacker placement: (core, slice)
        # we do not pin to the same core as the victim
        if attacker_core == vic_core: continue 
        for attacker_slice in SLICES:
            # do not have a length-0 victim
            if attacker_slice == attacker_core: continue 

            score = 0
            for vic_slice in SLICES:
                config_score = get_config_contention(vic_core, vic_slice, attacker_core, attacker_slice)
                score += config_score
            if score >= max_score:
                max_score = score
                best_attacker = (attacker_core, attacker_slice)
        attacker_core, attacker_slice = best_attacker
        print(f'{print_coord(attacker_core)}->{print_coord(attacker_slice)}: {max_score}')
        vuln_score_dict[attacker_core] = max_score
    return vuln_score_dict


def parse_output(filepath):
    """Parses the output from side-channel/orchestrator.py --analyticalmodelverify

    Returns 4 arrays with the attacker placements tested, the accuracy,
    precision, and recall achieved in each case.
    """
    if not os.path.exists(filepath):
        logging.error(f'Requested file ({filepath}) does not exist. Please check README.md for instructions on how to collect the verification data before running this script.')
        exit()
    with open(filepath, 'r') as f:
        raw = f.read()
    
    # Extract accuracy, precision, and recall values
    data_pattern = re.compile('accuracy = ([0-9\.]+) precision = ([0-9\.]+) recall = ([0-9\.]+)')
    matches = data_pattern.findall(raw)
    accuracy = np.array([float(i[0]) for i in matches])
    precision = np.array([float(i[1]) for i in matches])
    recall = np.array([float(i[2]) for i in matches])
    logging.debug(f'Found {len(matches)} data points in the output.')

    # Extract the core values
    core_pattern = re.compile('\[.*, \'(.*)\', .*, .*, .*\]')
    matches = core_pattern.findall(raw)
    attacker_cores = np.array([int(i) for i in matches])
    logging.debug(f'Found {len(attacker_cores)} cores in the output.')

    return (attacker_cores, accuracy, precision, recall)

def make_plot(attacker_cores, rsa_ml_acc, ecdsa_ml_acc, vuln_score_dict):
    """Makes a plot that validates the analytical model.
    
    Plots the vulnerability score of each attacker placement along with the
    observed accuracies achieved by the ML model for ECDSA and RSA.
    This function accepts 3 Numpy arrays and a dict:
    - attacker_cores: np array of the attacker cores tested
    - rsa_ml_acc: np array of the model accuracy for each attacker placement against RSA
    - ecdsa_ml_acc: np array of the model accuracy for each attacker placement against ECDSA
    - vuln_score_dict: a dictionary mapping each attacker core placement to the
      maximum achievable vulnerability score
    """
    Path('plot').mkdir(exist_ok=True)
    figname = 'plot/model_verification.pdf'

    vuln_scores = np.array([vuln_score_dict[i] for i in attacker_cores])

    std_rsa_ml_acc = (rsa_ml_acc - rsa_ml_acc.mean()) / rsa_ml_acc.std()
    std_ecdsa_ml_acc = (ecdsa_ml_acc - ecdsa_ml_acc.mean()) / ecdsa_ml_acc.std()
    std_vuln_scores = (vuln_scores - vuln_scores.mean()) / vuln_scores.std()

    attacker_cores = np.array(attacker_cores)

    # Re-sort for correct left-to-right plotting
    sort_indx = np.argsort(attacker_cores)
    attacker_cores = attacker_cores[sort_indx]
    std_rsa_ml_acc = std_rsa_ml_acc[sort_indx]
    std_ecdsa_ml_acc = std_ecdsa_ml_acc[sort_indx]
    std_vuln_scores = std_vuln_scores[sort_indx]

    # Plot
    labels = [print_coord(i) for i in attacker_cores]
    attacker_cores = list(range(len(attacker_cores))) # force the x axis to be evenly spaced
    plt.rcParams["figure.figsize"] = (6.4, 2.7)

    plt.axhline(y=0, color='gray', linewidth=1)
    plt.plot(attacker_cores, std_rsa_ml_acc, label='Std RSA ML Acc', marker='o', markersize=6)
    plt.plot(attacker_cores, std_ecdsa_ml_acc, label='Std ECDSA ML Acc', marker='s', markersize=6)
    plt.plot(attacker_cores, std_vuln_scores, label='Std Vuln Score', marker='^', markersize=6)

    plt.xticks(attacker_cores, labels, rotation='vertical')
    plt.xlabel('Attacker core')
    plt.legend(loc='upper right')

    plt.tight_layout()
    plt.savefig(figname)
    plt.close()

def main():
    vuln_score_dict = analytical_model_verification()

    rsa_cores, rsa_acc, _, _ = parse_output('./data/model-verification-rsa.out')
    ecdsa_cores, ecdsa_acc, _, _ = parse_output('./data/model-verification-ecdsa.out')

    assert (rsa_cores == ecdsa_cores).all(), 'RSA and ECDSA tests did not test the same cores in the same order. Check that you used the same orchestrator script for these two experiments.'
    cores = rsa_cores

    make_plot(cores, rsa_acc, ecdsa_acc, vuln_score_dict)
    

if __name__ == '__main__':
    main()
