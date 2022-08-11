from predict_contention import *
from utils import print_coord


def make_baseline_heatmap():
    """Print out the vulnerability score for every fully-active core.

    Produces data for the heatmap shown in Figure 12.
    """
    print('victim\tmax_score\tbest_attacker')
    for vic_core in CORES:
        max_score = 0
        best_rx = None
        for rx_core in CORES:
            # do not pin to the same core as the victim
            if rx_core == vic_core: continue
            for rx_slice in SLICES:
                # do not need to test a length-0 victim
                if rx_slice == rx_core: continue 

                score = 0
                for vic_slice in SLICES:
                    config_score = get_config_contention(vic_core, vic_slice, rx_core, rx_slice)
                    score += config_score
                if score >= max_score:
                    max_score = score
                    best_rx = (rx_core, rx_slice)
        
        rx_core, rx_slice = best_rx
        print(f'{print_coord(vic_core)}\t{max_score}\t\t{print_coord(rx_core)}->{print_coord(rx_slice)}')



def main():
    # Figure 12
    make_baseline_heatmap()

if __name__ == '__main__':
    main()

