#include "skx_hash_utils.h"
#include "skx_hash_utils_addr_mapping.h"
#include "pfn_util.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * Gets the corresponding CHA using the reverse engeineered hash function
 */
int get_cha_with_hash(void* virtual_address, bool huge) {
    int xor_map[17] = {0x2f9f, 0x2c31, 0x5ea, 0xc76, 0xf4b, 0x7ff, 0x4c9, 0x2e79, 0x69b, 0xee7, 0x2a20, 0x494, 0x44, 0x571, 0x2e9b, 0x2365, 0x2d26};
    long test_map[14] = {0x1c48300000, 0x0, 0x1469b00000, 0x16bff00000, 0xc7b100000, 0x1a03500000, 0x4b6500000, 0xb2fc00000, 0x1a6ae00000, 0x69ab00000, 0x41f500000, 0x19a2900000, 0x1433d00000, 0xe3f300000};
    ADDR_PTR frame = get_physical_frame_number(((ADDR_PTR)virtual_address & (huge ? 0xffffffffc0000000 : 0xffffffffffffffff)) >> 12);


    ADDR_PTR addr = (ADDR_PTR) virtual_address;
    ADDR_PTR rand_addr_phys = (addr & (huge ? 0x3fffffff : 0xfff)) + ((ADDR_PTR) frame << 12);
    ADDR_PTR ix_bits = (rand_addr_phys >> 6) & 0x3fff;
    ADDR_PTR hash_bits = (rand_addr_phys >> 20);
    int n = 0;

    int second_n = 0;
    ADDR_PTR temp = hash_bits ^ 0x8000;
    
    for (int i = 0; i < 17; i++) {
        int temp_n = 0;
        //printf("i: %i\n", i);
        // for (long a: test_map) {
        for (int j = 0; j < 14; j++) {
            long a = test_map[j];
            temp_n = temp_n << 1;
            //printf("hash_bits: %lx, map: %lx\n", hash_bits >> i, (a >> (20+i)));
            temp_n = temp_n ^ (((hash_bits ^ 0x8000) >> i) & (a >> (20 + i)) & 0x1);
            //printf("temp_n: %i\n", temp_n);
        }
        second_n = second_n ^ temp_n;
    }
    
    
    for (int j = 0; j < 17; j++) {
        if ((temp & 0x1) != 0) {
            n = n ^ xor_map[j];
        }
        temp = temp >> 1;
    }
    
    if (n != second_n) {
        printf("%x, %x\n", n, second_n);
        exit(1);
    }
    //printf("n: %i\n", n);
    int expected_cha = (int)BASE_SEQ[ix_bits^n];

    return expected_cha;

}