#include "machine_const.h"

// Returns the lowest value cpu that maps to a particular core
const int cha_id_to_cpu[26] = {0, 13, 7, -1, 
                        1, 14, 8, 19, 2,
                        15, 9, 20, 3,
                        16, 10, 21, 4,
                        17, 11, 22, 5, 18,
                        12, 23, 6, -1};

/**
 * Indexing into cpu_on_socket with the socket number should return a cpu ID to
 * use. Ideally, these values should be correct regardless of the hyperthreading
 * status.
 */
const int cpu_on_socket[NUM_SOCKET] = {0, 24};
