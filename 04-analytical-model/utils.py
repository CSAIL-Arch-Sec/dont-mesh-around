from collections import namedtuple
from enum import Enum

from config import *

##################################
# Custom types
# Coordinate of a tile on the die (origin in top left)
Coord = namedtuple('Coord', 'x y') 
# Flow is fully defined by the source, destination, and destination's TileType 
Flow = namedtuple('Flow', 'src dst dst_type')
# get_contention can detect multiple types of contention
ContentionResult = namedtuple('ContentionResult', 'priority roundrobin') 

Direction = Enum('Direction', 'LEFT RIGHT NONE')
FlowType = Enum('FlowType', 'RQ DATA WB')
TileType = Enum('TileType', 'CORE SLICE IMC')

##################################
# Utility functions
def get_coord(slice_id):
    """Returns the coordinate (2D index) of a slice ID within DIE_LAYOUT."""
    for r, row in enumerate(DIE_LAYOUT):
        if slice_id in row:
            return Coord(row.index(slice_id), r)
    print(f'Error: could not find Slice ID {slice_id} in DIE_LAYOUT')
    return None

def print_coord(slice_id):
    """Print a Coord tuple using the notation from the paper.
    
    Note that the x and y coordinates are flipped from their representation in
    utils.py
    """
    coord = get_coord(slice_id)
    return f'({coord.y},{coord.x})'

def get_dir(src, dst):
    """Returns the direction of a flow going from src to dst.

    Vertical flows are treated like horizontal flows that have been rotated
    clockwise 90 deg. Thus, a flow going down will get Direction.RIGHT.
    This function assumes that coordinates are increasing in one direction and 
    that src and dst are ints.
    """
    
    if dst > src:
        return Direction.RIGHT
    if dst < src:
        return Direction.LEFT
    return Direction.NONE

def is_overlapping(a0, a1, b0, b1):
    """Returns True if two paths are overlapping on >= 1 link.

    a0 -> a1 is the first flow
    b0 -> b1 is the second flow
    This function assumes that the flows are colinear.
    Note that the two paths must overlap on at least one link. Overlapping on
    only one slice is insufficient (e.g. 4->5->6 and 6->7->8 do not overlap).
    """
    interval_a = sorted((a0, a1))
    interval_b = sorted((b0, b1))
    return max(interval_a[0], interval_b[0]) < min(interval_a[1], interval_b[1])

def get_horz_lane(src_idx, dst_idx, dst_type):
    """Returns the lane used for a horizontal flow from src to dst.

    core_idx and slice_idx are x coordinates (horizontal indices).
    dst_type is the TileType of the destination.
    """

    if dst_type == TileType.SLICE:
        # Core-to-slice flows
        lane = HORZ_TO_SLICE_LANES[src_idx][dst_idx]
    elif dst_type == TileType.CORE:
        # Slice-to-core flows
        lane = HORZ_TO_CORE_LANES[dst_idx][src_idx]
    else:
        raise ValueError('Invalid dst_type')
    
    assert lane != 'X', 'Requested invalid lane, returned X'
    return lane

def get_vert_lane(src_idx, dst_idx, dst_type):
    """Returns the lane used for a vertical flow from src to dst.

    core_idx and slice_idx are y coordinates (vertical indices).
    dst_type is the TileType of the destination.
    """
    if dst_type == TileType.SLICE:
        # Note technically to a slice, but this indicates a rq or wb flow
        lane = VERT_TO_SLICE_LANES[src_idx][dst_idx]
    elif dst_type == TileType.CORE:
        # Slice-to-core flows
        lane = VERT_TO_CORE_LANES[dst_idx][src_idx]
    else:
        raise ValueError('Invalid dst_type')
    
    assert lane != 'X', 'Requested invalid lane, returned X'
    return lane
