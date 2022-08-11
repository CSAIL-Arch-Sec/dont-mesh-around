/**
 * util-cpu-specific.h
 * 
 * Contains processor-specific details about the machine architecture.
 */

#ifndef MACHINE_CONST_H_
#define MACHINE_CONST_H_

/*
 * Cache hierarchy characteristics
 * 
 * To understand these concepts visually, see the presentations here:
 *  https://cs.adelaide.edu.au/~yval/Mastik/
 * 
 * The cache line/block size in my machine is 64 bytes (2^6), meaning that the
 * rightmost 6 bits of the physical address are used as cache block offset. 
 * 
 * The L1 cache in my machine has 64 cache sets (2^6)
 * (cat /sys/devices/system/cpu/cpu0/cache/index0/number_of_sets).
 * That means that the next 6 bits after the block offset of the physical
 * address are used as L1 cache set index.
 * 
 * The LLC cache in my machine has 53248 cache sets (26 * 2^11) (cat
 * /sys/devices/system/cpu/cpu0/cache/index3/number_of_sets).
 * However these sets are split between 26 slices
 * in my CPU.
 * That means that each slice has 2048 (2^11) cache sets.
 * Therefore in my CPU, the next 11 bits of the physical address after the block
 * offset are used as LLC cache set index within each slice.
 *
 */

#define CACHE_BLOCK_SIZE 64 // Cache block and cache line are the same thing
#define CACHE_BLOCK_SIZE_LOG 6

// 32 KiB, 8-way, 64 B/line
// 64 sets (6 index bits)
#define L1_CACHE_WAYS 8
#define L1_CACHE_SETS 64
#define L1_CACHE_SETS_LOG 6
#define L1_CACHE_SIZE (L1_CACHE_SETS) * (L1_CACHE_WAYS) * (CACHE_BLOCK_SIZE)

// 1 MiB, 16-way, 64 B/line, inclusive
// 1024 sets (10 index bits)
#define L2_CACHE_WAYS 16
#define L2_CACHE_SETS 1024
#define L2_CACHE_SETS_LOG 10
#define L2_CACHE_SIZE (L2_CACHE_SETS) * (L2_CACHE_WAYS) * (CACHE_BLOCK_SIZE)

// 1.375 MiB, 11-way, 64 B/line, non-inclusive
// 2048 sets (11 index bits)
#define LLC_CACHE_WAYS 11
#define LLC_CACHE_SETS_PER_SLICE 2048
#define LLC_CACHE_SETS_LOG 11
#define LLC_CACHE_SLICES 26
#define LLC_CACHE_SIZE (LLC_CACHE_SETS_TOTAL) * (LLC_CACHE_WAYS) * (CACHE_BLOCK_SIZE)

/* 
 * Set indexes 
 */

#define L1_SET_INDEX_MASK 0xFC0				 /* 6 bits - [11-6] - 64 sets */
#define L2_SET_INDEX_MASK 0xFFC0			 /* 10 bits - [15-6] - 1024 sets */
#define LLC_SET_INDEX_PER_SLICE_MASK 0x1FFC0 /* 11 bits - [16-6] - 2048 sets */
#define LLC_INDEX_STRIDE 0x20000			 /* Offset required to get the next address with the same LLC cache set index. 17 = bit 16 (MSB bit of LLC_SET_INDEX_PER_SLICE_MASK) + 1 */
#define L2_INDEX_STRIDE 0x10000				 /* Offset required to get the next address with the same L2 cache set index. 16 = bit 15 (MSB bit of L2_SET_INDEX_MASK) + 1 */

/*
 * CPU Topology details
 * 
 * By default, the values are for hyperthreading off.
 */

// Uncomment the following line if hyperthreading is turned on
// #define HYPERTHREADING_ON

#define NUM_SOCKET                  2
#define NUM_CORES_PER_SOCKET        24
#define NUM_CHA                     LLC_CACHE_SLICES

#ifdef HYPERTHREADING_ON
#define NUM_LOG_CORES_PER_SOCKET    48
#else
#define NUM_LOG_CORES_PER_SOCKET    24
#endif

extern const int cha_id_to_cpu[];
extern const int cpu_on_socket[];

/*
 * Memory related constants
 * 
 * A page frame shifted left by PAGE_SHIFT will give us the physcial address of the frame
 * Note that this number is architecture dependent. For x86_64 it is defined as 12.
 */

#define PAGE_SHIFT 12
#define PAGEMAP_LENGTH 8
#define PAGE 4096

#endif
