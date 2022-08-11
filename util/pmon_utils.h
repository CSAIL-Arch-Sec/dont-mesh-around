/**
 * pmon_utils.hpp
 * 
 * Utility functions for working with Intel's uncore performance monitors.
 * Detailed information can be found in the Intel Xeon Processor Scalable Family
 * Uncore Performance Monitoring Reference Manual
*/

#ifndef PMON_UTILS_H_
#define PMON_UTILS_H_

#include <stdint.h>         // uint64_t
#include <unistd.h>         // pwrite, pread
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "pmon_reg_defs.h"
#include "machine_const.h"

#define MAX_FILENAME_LEN 100

#define WRITE_MSR(msr_fd, offset, value) pwrite(msr_fd, &value, sizeof(value), offset)
#define READ_MSR(msr_fd, offset, value) pread(msr_fd, &value, sizeof(value), offset)


void set_pmon_cha_msr_ctr_ctrl_reg(int msr_fd, uint64_t msr_addr, 
                            uint64_t event_code, uint64_t umask);
uint64_t read_pmon_cha_msr_ctr_reg(int msr_fd, int core, int n);

int cpu_to_core(int cpu);
int core_to_cpu(int core);
int get_active_cpus(void);
int open_msr_fd(int cpu);
void close_msr_fd(int fd);
void open_msr_interface(int num_cpus, int msr_fd[]);
void close_msr_interface(int num_cpus, int msr_fd[]);

void freeze_all_counters(int msr_fd);
void unfreeze_all_counters(int msr_fd);

/**
 * Reset pmon counter registers in a particular box (i.e. on a specific core)
 */
static inline void reset_counters(int msr_fd, int core) {
    // 1.9.2.d Reset Counters in a box
    uint64_t msr_val = 0x3;
    if (WRITE_MSR(msr_fd, CHA_MSR_PMON_UNIT_CTRL(core), msr_val) == -1) {
        perror("Error resetting counters");
        exit(EXIT_FAILURE);
    }
}

int get_corresponding_cha_no_msr(void *virtual_address, int msr_fd[NUM_LOG_CORES_PER_SOCKET]);
int get_corresponding_cha(void *virtual_address);
void *get_addr_in_core(int core, void *buf, long buf_size);
enum Ring {AD, IV, AK, BL};
void set_ad_ring_monitoring(int msr_fd, int core);
void set_ad_vert_ring_monitoring(int msr_fd, int core);
void set_ad_horz_ring_monitoring(int msr_fd, int core);
void set_iv_ring_monitoring(int msr_fd, int core);
void set_ak_ring_monitoring(int msr_fd, int core);
void set_ak_horz_ring_monitoring(int msr_fd, int core);
void set_ak_vert_ring_monitoring(int msr_fd, int core);
void set_bl_ring_monitoring(int msr_fd, int core);
void set_bl_vert_ring_monitoring(int msr_fd, int core);
void set_bl_horz_ring_monitoring(int msr_fd, int core);
void set_ad_bl_ring_monitoring(int msr_fd, int core);
void set_ak_iv_ring_monitoring(int msr_fd, int core);

#endif // PMON_UTILS_H_
