"""
Analytical model processor configuration

This file describes the setup of the victim machine. All numbers use slice IDs
which are converted into the 2D coordinates used in the paper.
Throughout the code, CHA and slice ID are used synonymously.
"""
#####################################
# Contention weights
#####################################

# Relative weights of various forms of NoC contention
# These were obtained from reverse-engineering results
AD_RING_SCORE = 1
BL_RING_SCORE = 2
AK_RING_SCORE = 1
# Our model predicts but does not score round-robin contention
ROUNDROBIN_SCORE = 0

#####################################
# Processor topology
#####################################

# Slice IDs of the fully active cores
CORES = [0,1,2,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24]
# Slice IDs of the LLC slices (all 26 slices are active on our machine)
SLICES = range(26)

# The physical layout of the slice IDs on the die
# -1 denotes a tile with no slice ID (IMC or fully-disabled core)
DIE_LAYOUT = [
    [0, 4, 9, 13, 17, 22],
    [-1, 5, 10, 14, 18, -1],
    [1, 6, 11, 15, 19, 23],
    [2, 7, 12, -1, 20, 24],
    [3, 8, -1, 16, 21, 25]
]

#####################################
# Lane scheduling policy
#####################################

# Each entry indicates the lane used when traveling horizontally to a slice on
# that tile
HORZ_TO_SLICE_LANES = [
    ['X', 'A', 'B', 'A', 'B', 'A'],
    ['A', 'X', 'B', 'A', 'B', 'A'],
    ['A', 'B', 'X', 'A', 'B', 'A'],
    ['A', 'B', 'A', 'X', 'B', 'A'],
    ['A', 'B', 'A', 'B', 'X', 'A'],
    ['A', 'B', 'A', 'B', 'A', 'X']
]

# Each entry indicates the lane used when traveling horizontally to a core on
# that tile
HORZ_TO_CORE_LANES = [
    ['X', 'B', 'B', 'B', 'B', 'B'],
    ['B', 'X', 'A', 'A', 'A', 'A'],
    ['A', 'A', 'X', 'B', 'B', 'B'],
    ['B', 'B', 'B', 'X', 'A', 'A'],
    ['A', 'A', 'A', 'A', 'X', 'B'],
    ['B', 'B', 'B', 'B', 'B', 'X']
]

# Each entry indicates the lane used when traveling vertically to a slice on
# that tile
VERT_TO_SLICE_LANES = [
    ['X', 'A', 'B', 'B', 'B'],
    ['A', 'X', 'A', 'A', 'A'],
    ['B', 'A', 'X', 'A', 'B'],
    ['A', 'A', 'A', 'X', 'A'],
    ['B', 'B', 'B', 'A', 'X']
]

# Each entry indicates the lane used when traveling vertically to a core on
# that tile
VERT_TO_CORE_LANES = [
    ['X', 'B', 'A', 'B', 'A'],
    ['B', 'X', 'B', 'B', 'A'],
    ['A', 'B', 'X', 'B', 'A'],
    ['A', 'B', 'B', 'X', 'B'],
    ['A', 'B', 'A', 'B', 'X']
]