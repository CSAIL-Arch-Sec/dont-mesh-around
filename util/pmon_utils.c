/**
 * pmon_utils.cpp
 * 
 * Utility functions for working with Intel's uncore performance monitors.
 * Detailed information can be found in the Intel Xeon Processor Scalable Family
 * Uncore Performance Monitoring Reference Manual
*/

#include "pmon_utils.h"
#include "pmon_reg_defs.h"
#include "machine_const.h"

#include <fcntl.h>          // open(), close()
#include <errno.h>          // errno
#include <string.h>         // strerror()
#include <unistd.h>         // sysconf()
#include <stdio.h>          // printf(), sprintf(), etc
#include <stdlib.h>         // exit()
#include <assert.h>         // assert()
#include <x86intrin.h>

char filename[MAX_FILENAME_LEN];

/**
 * Set a counter control register within a PMON unit. Each PMON unit has 
 * 4 counter registers that can be configured to count different events.
 * 
 * msr_fd: file descriptor for the msr interface
 * msr_addr: Address of the specfic control register in msr-space (see Section 1.8)
 * event_code: hex code for the desired event to be measured
 * umask: a filter that can be applied to an event
 * 
 * See the Uncore Performance Monitoring Guide Section 1.4
 */
void set_pmon_cha_msr_ctr_ctrl_reg(int msr_fd, uint64_t msr_addr, 
                            uint64_t event_code, uint64_t umask) {
    uint64_t msr_val = 1UL << PMON_CTL_en |             // enable counter
                       event_code << PMON_CTL_ev_sel |  // select event
                       umask << PMON_CTL_umask;         // set umask
    WRITE_MSR(msr_fd, msr_addr, msr_val);
}

/**
 * Read a counter register within a PMON unit. Each PMON unit has 4 counter 
 * registers.
 * 
 * msr_fd: file descriptor for the msr interface
 * core: the core/CHA number to read from
 * n: the index of the counter to read (0-3)
 */
uint64_t read_pmon_cha_msr_ctr_reg(int msr_fd, int core, int n) {
    uint64_t msr_val;
    READ_MSR(msr_fd, CHA_MSR_PMON_CTR(core, n), msr_val);
    // Mask out lower 48 bits; higher order bits are reserved (1.4.1, Table 1-7)
    return msr_val & 0xFFFFFFFFFFFFul; 
}

/**
 * Returns the core (as defined by the uncore pmon registers) corresponding
 * to the logical cpu (i.e. cpu as defined in /proc/cpuinfo)
 * Mappings determined by the experiments in pmon_core_mapping
 * 
 * Note: this is only guaranteed to be correct for the fatality machine
 */
int cpu_to_core(int cpu) {
    #ifndef FATALITY
    printf("ERROR: FATALITY is not defined. Make sure cpu_to_core is configured"
        " for your machine!\n");
    exit(-1);
    #endif

    cpu %= 10;
    return (cpu < 5) ? 2 * cpu : (cpu - 5) * 2 + 1;
}

/**
 * Returns the lower-value cpu (i.e. cpu as defined in /proc/cpuinfo) that 
 * maps to the core (as defined by the uncore pmon registers).
 * Mappings determined by the experiments in pmon_core_mapping
 * 
 * Note: this is only defined for the fatality machine at the moment
 */
int core_to_cpu(int core) {
    #ifdef FATALITY
        return (core % 2) ? core / 2 + 5 : core / 2;
    #else
        printf("ERROR: FATALITY is not defined. Make sure cpu_to_core is configured"
            " for your machine!\n"); 
        exit(-1);
    #endif
}

int get_active_cpus() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

/**
 * Opens a file using the MSR driver from the specified cpu.
 * 
 * To open the MSR interface to a particular socket, use a cpu that is located
 * on the desired socket. Returns the file descriptor.
 */
int open_msr_fd(int cpu) {
    sprintf(filename, "/dev/cpu/%d/msr", cpu);
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        fprintf(stderr, "[ERROR] cannot open %s: %s\n", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return fd;
}

void close_msr_fd(int fd) {
    if (close(fd) != 0) {
        fprintf(stderr, "[ERROR] cannot close fd: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void open_msr_interface(int num_cpus, int msr_fd[]) {
    // Get MSR interface
    // Caller must ensure msr_fd is large enough to hold all file descriptors
    for (int i = 0; i < num_cpus; i++) {
        sprintf(filename, "/dev/cpu/%d/msr", i);
        #ifdef DEBUG
            printf("DEBUG: opening %s\n", filename);
        #endif
        msr_fd[i] = open(filename, O_RDWR);
        if (msr_fd[i] == -1) {
            printf("ERROR: %s; cannot open %s\n", strerror(errno), filename);
            exit(-1);
        }
    }
}

void close_msr_interface(int num_cpus, int msr_fd[]) {
    for (int i = 0; i < num_cpus; i++) {
        sprintf(filename, "/dev/cpu/%d/msr", i);
        close(msr_fd[i]);
        if (msr_fd[i] == -1) {
            printf("ERROR: %s; cannot close %s\n", strerror(errno), filename);
            exit(-1);
        }
    }
}

void freeze_all_counters(int msr_fd) {
    // 1.3.2.1 - Freeze all uncore counters by setting 
    //  U_MSR_PMON_GLOBAL_CTL.frz_all to 1
    uint64_t msr_num = U_MSR_PMON_GLOBAL_CTL;
    uint64_t msr_val = 1UL << U_MSR_PMON_GLOBAL_CTL_frz_all;
    if (WRITE_MSR(msr_fd, msr_num, msr_val) == -1) {
        perror("Error freezing all counters");
        exit(EXIT_FAILURE);
    }
}

void unfreeze_all_counters(int msr_fd) {
    uint64_t msr_val = 1UL << U_MSR_PMON_GLOBAL_CTL_unfrz_all;
    uint64_t msr_num = U_MSR_PMON_GLOBAL_CTL;
    if (WRITE_MSR(msr_fd, msr_num, msr_val) == -1) {
        perror("Error unfreezing all counters");
        exit(EXIT_FAILURE);
    }
}

int get_corresponding_cha(void *virtual_address) {
    int nr_cpus = get_active_cpus();
    // printf("DEBUG: found %d active cpus\n", nr_cpus);
    assert(nr_cpus <= NUM_LOG_CORES_PER_SOCKET * NUM_SOCKET);

    int msr_fd[NUM_LOG_CORES_PER_SOCKET * NUM_SOCKET];     // MSR device driver files

    open_msr_interface(nr_cpus, msr_fd);
    int result = get_corresponding_cha_no_msr(virtual_address, msr_fd);
    close_msr_interface(nr_cpus, msr_fd);
    return result;
}

int get_corresponding_cha_no_msr(void *virtual_address, int msr_fd[NUM_LOG_CORES_PER_SOCKET * NUM_SOCKET]) {
    // TODO: avoid using these #defines within the library function

    #define CHA_TEST_REPS   10000   // Use 10k accesses to test for CHA association

    int msr_readouts[NUM_LOG_CORES_PER_SOCKET * NUM_SOCKET]; // Values read from each core
    volatile int result;

    uint64_t msr_num, msr_val;
    // Set up a counter in each CHA
    int core = 0; // these msrs can be accessed through any core's driver. Core 0 chosen arbitrariliy
    // 1.9.2.a - Freeze all uncore counters 
    freeze_all_counters(msr_fd[core]);

    for (int cha = 0; cha < NUM_CHA; cha++) {
        // Calculate all offsets (1.8.1)
        uint64_t cha_msr_pmon_unit_ctrl = CHA_MSR_PMON_BASE + cha * 0x10;
        uint64_t cha_msr_pmon_ctrl0 = cha_msr_pmon_unit_ctrl + 1;
        uint64_t cha_msr_pmon_filter0 = cha_msr_pmon_unit_ctrl + 5;
        uint64_t cha_msr_pmon_filter1 = cha_msr_pmon_filter0 + 1;

        // 1.9.2.d Reset counters in each box
        msr_val = 0x3;
        msr_num = cha_msr_pmon_unit_ctrl;
        WRITE_MSR(msr_fd[core], msr_num, msr_val);

        // 1.9.2.b Enable counting for each monitor
        // 1.9.2.c Select event to monitor (i.e. program event control register umask and ev_sel bits)
        msr_val = 1UL << PMON_CTL_en |           // 1 = enable
                  0x34UL << PMON_CTL_ev_sel |    // 0x34 = LLC_LOOKUP event 
                  0x3UL << PMON_CTL_umask;       // 0x3 = DATA_READ umask
        msr_num = cha_msr_pmon_ctrl0;
        // #ifdef DEBUG
        // printf("DEBUG: Write cha%02d_msr_pmon_ctrl0 (0x%lx): 0x%lx\n", cha, msr_num, msr_val);
        // #endif
        WRITE_MSR(msr_fd[core], msr_num, msr_val);

        // Set CHAFilter0[26:17] (2.2.6.2)
        // 0xFF = count all states
        msr_val = 0xFFUL << CHA_MSR_PMON_FILTER0_state;
        msr_num = cha_msr_pmon_filter0;
        WRITE_MSR(msr_fd[core], msr_num, msr_val);

        // Turn off Filter1 (2.2.6.2, see Note under Table 2-54)
        msr_val = 0x3BUL;
        msr_num = cha_msr_pmon_filter1;
        WRITE_MSR(msr_fd[core], msr_num, msr_val);
    }

    // 1.9.2.f Enable counting on global level 
    unfreeze_all_counters(msr_fd[core]);

    // counting has started

    for (int i = 0; i < CHA_TEST_REPS; i++) {
        _mm_clflush((char *)virtual_address);
        result = *(char *)virtual_address;
    }

    // 1.9.3.a Freeze values globally
    freeze_all_counters(msr_fd[core]);

    // Read value from all CHAs from Ctr0
    // Store the highest count and the second highest count from the counters
    int max_count = 0;
    int max_count_cha = 0;
    int second_max_count = 0;
    int second_max_count_cha = 0;

    for (int cha = 0; cha < NUM_CHA; cha++) {
        uint64_t cha_msr_pmon_ctr0 = CHA_MSR_PMON_CTR_BASE + cha * 0x10;
        msr_num = cha_msr_pmon_ctr0;
        READ_MSR(msr_fd[core], msr_num, msr_val);
        // mask out lower 48 bits; higher order bits are reserved (1.4.1, Table 1-7)
        msr_val &= 0xFFFFFFFFFFFF; 

        if (msr_val > max_count) {
            second_max_count = max_count;
            second_max_count_cha = max_count_cha;
            max_count = msr_val;
            max_count_cha = cha;
        } else if (msr_val > second_max_count) {
            second_max_count = msr_val;
            second_max_count_cha = cha;
        }
    }

    // We expect the highest count to vastly outweight the second highest count
    // If this is not the case, then there was too much noise during the experiment
    // As a sanity check, we make sure that the maximum count is at least 1/3 of the test flushes.
    if (max_count < CHA_TEST_REPS / 3) {
        printf("ERROR: not enough counts to guarantee good CHA detection\nMax LLC_LOOKUP value was %d for a total of %d loads\n", max_count, CHA_TEST_REPS);
        return -1;
    }
    if (second_max_count != 0 && max_count / second_max_count < 2) {
        // Multiple potential CHAs found
        printf("ERROR: multiple CHA candidates detected.\n");

        // for (int cha = 0; cha < NUM_CHA; cha++) {
        //     // Dump all pmon values
        //     uint64_t cha_msr_pmon_ctr0 = CHA_MSR_PMON_CTR_BASE + cha * 0x10;
        //     msr_num = cha_msr_pmon_ctr0;
        //     READ_MSR(msr_fd[core], msr_num, msr_val);
        //     msr_val &= 0xFFFFFFFFFFFF; 
        //     printf("DEBUG: cha %d LLC_LOOKUPs (0x%lx): %ld\n", cha, msr_num, msr_val);
        // }
        return -1;
    }

    // Everything looks good, return the CHA with the max count
    return max_count_cha;
}

/**
 * Returns an address local to the specified core within the provided buffer
 */
void *get_addr_in_core(int core, void *buf, long buf_size) {
    uintptr_t target = (uintptr_t)buf;
    while (1) {
        // TODO: deal with errors in get_corresponding_cha here
        if (get_corresponding_cha((void *)target) == core) {
            break;
        }
        target += 64; // 64 byte cache line (the next 63 addr have the same cha)
        if (target >= (uintptr_t)buf + buf_size) {
            printf("ERROR: could not find target addr with CHA %d within buf\n",
                core);
            exit(-1);
        }
    }
    return (void *)target;
}

/**
 * Sets all 4 counters on a core to measure the four directions of the AD ring.
 */
void set_ad_ring_monitoring(int msr_fd, int core) {
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 0), 
                                    VERT_RING_AD_IN_USE, 
                                    0x3UL); // 0x3 = umask for up ring (even and odd)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 1),
                                    VERT_RING_AD_IN_USE, 
                                    0xcUL); // 0xc = umask for down ring (even and odd)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 2),
                                    HORZ_RING_AD_IN_USE, 
                                    0x3UL); // 0x3 = umask for left ring (even and odd)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 3),
                                    HORZ_RING_AD_IN_USE, 
                                    0xcUL); // 0xc = umask for right ring (even and odd)
}

/**
 * Sets all 4 counters on a core to measure the vertical directions of the AD ring.
 */
void set_ad_vert_ring_monitoring(int msr_fd, int core) {
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 0), 
                                    VERT_RING_AD_IN_USE, 
                                    0x1UL); // 0x1 = umask for up ring (even)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 1),
                                    VERT_RING_AD_IN_USE, 
                                    0x2UL); // 0x2 = umask for up ring (odd)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 2),
                                    VERT_RING_AD_IN_USE, 
                                    0x4UL); // 0x4 = umask for down ring (even)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 3),
                                    VERT_RING_AD_IN_USE, 
                                    0x8UL); // 0x8 = umask for down ring (odd)
}

/**
 * Sets all 4 counters on a core to measure the vertical directions of the AD ring.
 */
void set_ad_horz_ring_monitoring(int msr_fd, int core) {
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 0), 
                                    HORZ_RING_AD_IN_USE, 
                                    0x1UL); // 0x1 = umask for up ring (even)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 1),
                                    HORZ_RING_AD_IN_USE, 
                                    0x2UL); // 0x2 = umask for up ring (odd)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 2),
                                    HORZ_RING_AD_IN_USE, 
                                    0x4UL); // 0x4 = umask for down ring (even)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 3),
                                    HORZ_RING_AD_IN_USE, 
                                    0x8UL); // 0x8 = umask for down ring (odd)
}

/**
 * Sets all 4 counters on a core to measure the four directions of the IV ring.
 */
void set_iv_ring_monitoring(int msr_fd, int core) {
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 0), 
                                    VERT_RING_IV_IN_USE, 
                                    0x1UL); // 0x1 = umask for up ring
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 1),
                                    VERT_RING_IV_IN_USE, 
                                    0x4UL); // 0x4 = umask for down ring
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 2),
                                    HORZ_RING_IV_IN_USE, 
                                    0x1UL); // 0x1 = umask for left ring
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 3),
                                    HORZ_RING_IV_IN_USE, 
                                    0x4UL); // 0x4 = umask for right ring
}

/**
 * Sets all 4 counters on a core to measure the four directions of the AK ring.
 */
void set_ak_ring_monitoring(int msr_fd, int core) {
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 0), 
                                    VERT_RING_AK_IN_USE, 
                                    0x3UL); // 0x3 = umask for up ring (even and odd)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 1),
                                    VERT_RING_AK_IN_USE, 
                                    0xcUL); // 0xc = umask for down ring (even and odd)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 2),
                                    HORZ_RING_AK_IN_USE, 
                                    0x3UL); // 0x3 = umask for left ring (even and odd)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 3),
                                    HORZ_RING_AK_IN_USE, 
                                    0xcUL); // 0xc = umask for right ring (even and odd)
}
/**
 * Sets all 4 counters on a core to measure the horizontal direction of the AK ring.
 */
void set_ak_horz_ring_monitoring(int msr_fd, int core) {
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 0), 
                                    HORZ_RING_AK_IN_USE, 
                                    0x1UL); // 0x1 = umask for left even
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 1),
                                    HORZ_RING_AK_IN_USE, 
                                    0x2UL); // 0xc = umask for left odd
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 2),
                                    HORZ_RING_AK_IN_USE, 
                                    0x4UL); // 0x3 = umask for right even
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 3),
                                    HORZ_RING_AK_IN_USE, 
                                    0x8UL); // 0xc = umask for right odd
}

/**
 * Sets all 4 counters on a core to measure the vertical direction of the AK ring.
 */
void set_ak_vert_ring_monitoring(int msr_fd, int core) {
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 0), 
                                    VERT_RING_AK_IN_USE, 
                                    0x1UL); // 0x1 = umask for up even
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 1),
                                    VERT_RING_AK_IN_USE, 
                                    0x2UL); // 0xc = umask for up odd
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 2),
                                    VERT_RING_AK_IN_USE, 
                                    0x4UL); // 0x3 = umask for down even
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 3),
                                    VERT_RING_AK_IN_USE, 
                                    0x8UL); // 0xc = umask for down odd
}

/**
 * Sets all 4 counters on a core to measure the four directions of the BL ring.
 */
void set_bl_ring_monitoring(int msr_fd, int core) {
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 0), 
                                    VERT_RING_BL_IN_USE, 
                                    0x3UL); // 0x3 = umask for up ring (even and odd)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 1),
                                    VERT_RING_BL_IN_USE, 
                                    0xcUL); // 0xc = umask for down ring (even and odd)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 2),
                                    HORZ_RING_BL_IN_USE, 
                                    0x3UL); // 0x3 = umask for left ring (even and odd)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 3),
                                    HORZ_RING_BL_IN_USE, 
                                    0xcUL); // 0xc = umask for right ring (even and odd)
}

/**
 * Sets all 4 counters on a core to measure the vertical directions of the BL ring.
 */
void set_bl_vert_ring_monitoring(int msr_fd, int core) {
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 0), 
                                    VERT_RING_BL_IN_USE, 
                                    0x1UL); // 0x1 = umask for up ring (even)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 1),
                                    VERT_RING_BL_IN_USE, 
                                    0x2UL); // 0x2 = umask for up ring (odd)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 2),
                                    VERT_RING_BL_IN_USE, 
                                    0x4UL); // 0x4 = umask for down ring (even)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 3),
                                    VERT_RING_BL_IN_USE, 
                                    0x8UL); // 0x8 = umask for down ring (odd)
}
/**
 * Sets all 4 counters on a core to measure the vert and horiz AD and BL traffic
 */
void set_ad_bl_ring_monitoring(int msr_fd, int core) {
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 0), 
                                    VERT_RING_AD_IN_USE, 
                                    0xfUL); // 0xf = umask for up and down ring
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 1),
                                    HORZ_RING_AD_IN_USE, 
                                    0xfUL); // 0xf = umask for left and right ring
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 2),
                                    VERT_RING_BL_IN_USE, 
                                    0xfUL); // 0xf = umask for up and down ring
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 3),
                                    HORZ_RING_BL_IN_USE, 
                                    0xfUL); // 0xf = umask for left and right ring
}

/**
 * Sets all 4 counters on a core to measure the horizontal directions of the BL ring.
 */
void set_bl_horz_ring_monitoring(int msr_fd, int core) {
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 0), 
                                    HORZ_RING_BL_IN_USE, 
                                    0x1UL); // 0x1 = umask for left ring (even)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 1),
                                    HORZ_RING_BL_IN_USE, 
                                    0x2UL); // 0x2 = umask for left ring (odd)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 2),
                                    HORZ_RING_BL_IN_USE, 
                                    0x4UL); // 0x4 = umask for right ring (even)
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 3),
                                    HORZ_RING_BL_IN_USE, 
                                    0x8UL); // 0x8 = umask for right ring (odd)
}

/**
 * Sets all 4 counters on a core to measure the vert and horiz AK and IV traffic
 */
void set_ak_iv_ring_monitoring(int msr_fd, int core) {
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 0), 
                                    VERT_RING_AK_IN_USE, 
                                    0xfUL); // 0xf = umask for up and down ring
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 1),
                                    HORZ_RING_AK_IN_USE, 
                                    0xfUL); // 0xf = umask for left and right ring
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 2),
                                    VERT_RING_IV_IN_USE, 
                                    0x5UL); // 0x5 = umask for up and down ring
    set_pmon_cha_msr_ctr_ctrl_reg(msr_fd, 
                                    CHA_MSR_PMON_CTRL(core, 3),
                                    HORZ_RING_IV_IN_USE, 
                                    0x5UL); // 0x5 = umask for left and right ring
}
