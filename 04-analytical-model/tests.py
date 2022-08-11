from predict_contention import *
from utils import *


def test_function(label, func, expected, *args):
    actual = func(*args)
    if expected == actual:
        print('{}: PASSED'.format(label))
    else:
        print('{}: FAILED; expected {}, got {}'.format(label, expected, actual))

if __name__ == '__main__':
    ######################################## 
    # Utils

    ######################################## 
    # Contention Functions
    f0 = Flow(Coord(2,1), Coord(2,1), TileType.SLICE) # Zero hops
    f1 = Flow(Coord(0,0), Coord(5,0), TileType.SLICE) # Lane A
    f2 = Flow(Coord(2,0), Coord(3,0), TileType.SLICE) # Lane A
    f3 = Flow(Coord(2,0), Coord(4,0), TileType.SLICE) # Lane B
    f4 = Flow(Coord(0,0), Coord(3,0), TileType.SLICE) 
    f5 = Flow(Coord(2,0), Coord(4,0), TileType.CORE) 
    f6 = Flow(Coord(1,0), Coord(3,0), TileType.SLICE) 

    test_function('row-contention-0', is_row_contention, ContentionResult(True, False), f1, f2) # Normal case
    test_function('row-contention-1', is_row_contention, ContentionResult(False, False), f2, f1) # no vic priority
    test_function('row-contention-2', is_row_contention, ContentionResult(False, False), f1, f3) # diff lane
    test_function('row-contention-3', is_row_contention, ContentionResult(False, True), f1, f4) # round-robin
    test_function('row-contention-4', is_row_contention, ContentionResult(False, False), f0, f4) # length-zero
    test_function('row-contention-5', is_row_contention, ContentionResult(True, False), f6, f5) # mix core and slice


    ######################################## 
    # Configs

    ########################################
    # Heatmap test
    def heatmap_test():
        for vic_core in [0, 4]:
            max_score = 0
            best_rx = None
            for rx_core in CORES:
                if rx_core == vic_core:
                    continue # we do not pin to the same core as the victim
                for rx_slice in SLICES:
                    if rx_slice == rx_core:
                        continue # do not have a length-0 victim
                    score = 0
                    for vic_slice in SLICES:
                        score += get_config_contention(vic_core, vic_slice, rx_core, rx_slice)
                    if score > max_score:
                        max_score = score
                        best_rx = (rx_core, rx_slice)

            print('Victim Core {:2d}: {}'.format(vic_core, max_score))
            print('Best rx: {}'.format(best_rx))
