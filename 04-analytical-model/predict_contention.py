from utils import *


def is_row_contention(vic_flow, rx_flow, debug=False):
    """Check for priority-based contention between two flows in a row."""

    # Check that everything is on the same row
    assert vic_flow.src.y == vic_flow.dst.y, 'vic_flow is not on the same row'
    assert rx_flow.src.y == rx_flow.dst.y, 'rx_flow is not on the same row'

    contention_result = ContentionResult(False, False)
    if vic_flow.src.x == vic_flow.dst.x or rx_flow.src.x == rx_flow.dst.x or rx_flow.src.y != vic_flow.src.y:
        # One of the paths has length 0, no contention 
        # TODO: technically there could be slice port but this model does not handle that
        # print('len0 path')
        return contention_result

    vic_dir = get_dir(vic_flow.src.x, vic_flow.dst.x)
    rx_dir = get_dir(rx_flow.src.x, rx_flow.dst.x)

    if vic_dir != rx_dir:
        if debug:
            print('diff direction')
        return contention_result
    if not is_overlapping(vic_flow.src.x, vic_flow.dst.x, rx_flow.src.x, rx_flow.dst.x):
        if debug:
            print('not overlapping')
        return contention_result
    
    if (get_horz_lane(vic_flow.src.x, vic_flow.dst.x, vic_flow.dst_type) != 
        get_horz_lane(rx_flow.src.x, rx_flow.dst.x, rx_flow.dst_type)):
        if debug:
            print('different lane')
        return contention_result

    victim_priority = ((vic_dir == Direction.LEFT and vic_flow.src.x > rx_flow.src.x) or
                        (vic_dir == Direction.RIGHT and vic_flow.src.x < rx_flow.src.x))
    if victim_priority:
        return ContentionResult(True, False)
    elif vic_flow.src == rx_flow.src:
        # note that this does not guarantee round robin contention. The parent
        # function needs to use context to determine if it is appropriate to
        # interpret this result
        return ContentionResult(False, True) 
    if debug:
        print('no priority')
    return contention_result

def is_col_contention(vic_flow, rx_flow):
    """Check for priority-based contention between two flows in a column."""

    # Check that everything is on the same col
    assert vic_flow.src.x == vic_flow.dst.x, 'vic_flow is not on the same col'
    assert rx_flow.src.x == rx_flow.dst.x, 'rx_flow is not on the same col'

    contention_result = ContentionResult(False, False)
    if vic_flow.src.y == vic_flow.dst.y or rx_flow.src.y == rx_flow.dst.y or vic_flow.src.x != rx_flow.src.x:
        # One of the paths has length 0, no contention
        # Or the flows are in different cols
        # TODO: unless there's slice port contention?
        return contention_result
    
    vic_dir = get_dir(vic_flow.src.y, vic_flow.dst.y)
    rx_dir = get_dir(rx_flow.src.y, rx_flow.dst.y)

    if vic_dir != rx_dir:
        return contention_result
    if not is_overlapping(vic_flow.src.y, vic_flow.dst.y, rx_flow.src.y, rx_flow.dst.y):
        return contention_result
    
    if (get_vert_lane(vic_flow.src.y, vic_flow.dst.y, vic_flow.dst_type) != 
        get_vert_lane(rx_flow.src.y, rx_flow.dst.y, rx_flow.dst_type)):
        return contention_result

    # Left and right here correspond to up and down. There's no need to complicate this
    victim_priority = ((vic_dir == Direction.LEFT and vic_flow.src.y > rx_flow.src.y) or
                        (vic_dir == Direction.RIGHT and vic_flow.src.y < rx_flow.src.y))
    if victim_priority:
        return ContentionResult(True, False)
    elif vic_flow.src == rx_flow.src:
        return ContentionResult(False, True) 

    return contention_result

def get_config_contention(v_c, v_s, r_c, r_s, debug=False):
    """Get the contention score of specific victim and receiver placement.
    
    v_c: victim core
    v_s: victim slice
    r_c: receiver core
    r_s: receiver slice
    """
    # print('Config: {}, {}, {}, {}'.format(v_c, v_s, r_c, r_s))
    rr_total = 0
    priority_total = 0

    v_c_loc = get_coord(v_c)
    v_s_loc = get_coord(v_s)
    r_c_loc = get_coord(r_c)
    r_s_loc = get_coord(r_s)
    
    # Get the turning coordinate (pivot) from core to slice (forward)
    v_forward_pivot = Coord(v_c_loc.x, v_s_loc.y)
    r_forward_pivot = Coord(r_c_loc.x, r_s_loc.y)
    # Backwards pivots (pivot on slice to core path)
    v_backwards_pivot = Coord(v_s_loc.x, v_c_loc.y)
    r_backwards_pivot = Coord(r_s_loc.x, r_c_loc.y)

    score = 0

    # HORIZONTAL CONTENTION
    # Check for horizontal request contention
    v_rq_flow = Flow(v_forward_pivot, v_s_loc, TileType.SLICE)
    r_rq_flow = Flow(r_forward_pivot, r_s_loc, TileType.SLICE)
    result = is_row_contention(v_rq_flow, r_rq_flow)
    if result.priority:
        if debug:
            print('AD priority')
        score += AD_RING_SCORE # AD ring
        priority_total += 1

    # Check for horizontal data-data contention
    v_da_flow = Flow(v_backwards_pivot, v_c_loc, TileType.CORE)
    r_da_flow = Flow(r_backwards_pivot, r_c_loc, TileType.CORE)
    if debug:
        print(v_da_flow, r_da_flow)
    result = is_row_contention(v_da_flow, r_da_flow)
    if result.priority:
        if debug:
            print('BL priority')
        score += BL_RING_SCORE + AK_RING_SCORE # BL + AK Ring
        priority_total += 1
    elif result.roundrobin:
        if debug:
            print('Data round robin')
        score += ROUNDROBIN_SCORE
        rr_total += 1
    
    # Check for horizontal data-writeback contention
    v_wb_flow = Flow(v_forward_pivot, v_s_loc, TileType.SLICE)
    result = is_row_contention(v_wb_flow, r_da_flow)
    if result.priority:
        if debug:
            print('WB priority')
        score += BL_RING_SCORE # BL Ring
        priority_total += 1
    elif result.roundrobin:
        if debug: 
            print('WB round robin')
        score += ROUNDROBIN_SCORE
        rr_total += 1

    # VERTICAL CONTENTION
    # dst_type here is a little strange. From the RE results, I believe we just
    # look at the ultimate destination since technically, a vertical flow
    # doesn't go to a core or a slice. We just use this to help us distinguish
    # whether it's a wb or a data flow
    v_rq_flow = Flow(v_c_loc, v_forward_pivot, TileType.SLICE)
    r_rq_flow = Flow(r_c_loc, r_forward_pivot, TileType.SLICE)
    result = is_col_contention(v_rq_flow, r_rq_flow)
    if result.priority:
        if debug:
            print('vertical AD priority')
        score += AD_RING_SCORE

    # Check for vertical data-data contention
    v_da_flow = Flow(v_s_loc, v_backwards_pivot, TileType.CORE)
    r_da_flow = Flow(r_s_loc, r_backwards_pivot, TileType.CORE)
    result = is_col_contention(v_da_flow, r_da_flow)
    if result.priority:
        if debug:
            print('vertical data priority')
        score += BL_RING_SCORE + AK_RING_SCORE # BL + AK Ring 
    elif result.roundrobin:
        if debug:
            print('vertical data roundrobin')
        score += ROUNDROBIN_SCORE

    # Check for vertical data-writeback contention
    v_wb_flow = Flow(v_c_loc, v_forward_pivot, TileType.SLICE)
    result = is_col_contention(v_wb_flow, r_da_flow)
    if result.priority:
        if debug:
            print('vertical wb priority')
        score += BL_RING_SCORE # BL Ring
    elif result.roundrobin:
        if debug:
            print('vertical wb round robin')
        score += ROUNDROBIN_SCORE
        
    # TODO: special cases, Is there slice port if both are full vert or full horz?
    # print('rr: {} priority: {}\n'.format(rr_total, priority_total))
    return score
