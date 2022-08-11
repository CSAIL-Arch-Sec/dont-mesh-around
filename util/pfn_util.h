#ifndef PFN_UTIL_H
#define PFN_UTIL_H

#include <inttypes.h>

#define PAGEMAP_ENTRY_SIZE 8
#define GET_BIT(X,Y) (X & ((uint64_t)1<<Y)) >> Y
//0-54 bit -> PFN
#define GET_PFN(X) X & 0x7FFFFFFFFFFFFF

uint64_t get_physical_frame_number(uint64_t vpn);

#endif
