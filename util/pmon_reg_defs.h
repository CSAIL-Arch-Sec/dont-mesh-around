/**
 * pmon_ref_defs.hpp
 * 
 * Register values come from the Intel Xeon Procesor Scalable Memory Family 
 * Uncore Performance Monitoring Reference Manual, July 2017
 * (Reference Number 336274-001US)
 * Referenced as UPMRM in the rest of the file
*/

#ifndef PMON_REG_DEFS_H_
#define PMON_REG_DEFS_H_

// PMON Global Control Block
// UPMRM Section 1.3.2
#define U_MSR_PMON_GLOBAL_CTL       0x0700L
#define U_MSR_PMON_GLOBAL_STATUS    0x0701L      // shows overflows
// Bit Offsets, UPMRM 1.3.2.1
#define U_MSR_PMON_GLOBAL_CTL_frz_all   63L      // freeze all counters
#define U_MSR_PMON_GLOBAL_CTL_unfrz_all 61L      // unfreeze all counters

// PMON Counter Control Register Bit Offsets
// UPMRM Section 1.4.1
#define PMON_CTL_en     22L     // enable counter
#define PMON_CTL_rst    17L     // reset counter
#define PMON_CTL_umask  8L      // umask to select subevent within selected event
#define PMON_CTL_ev_sel 0L      // ev_sel to select event to be counted
// Note: event codes and umask values found in UPMRM Section 2

// CHA counters are MSR-based.
//   The starting MSR address is 0x0E00 + 0x10*CHA
//   	Offset 0 is Unit Control -- mostly un-needed
//   	Offsets 1-4 are the Counter PerfEvtSel registers
//   	Offset 5 is Filter0	-- selects state for LLC lookup event (and TID, if enabled by bit 19 of PerfEvtSel)
//   	Offset 6 is Filter1 -- lots of filter bits, including opcode -- default if unused should be 0x03b, or 0x------33 if using opcode matching
//   	Offset 7 is Unit Status
//   	Offsets 8,9,A,B are the Counter count registers
#define CHA_MSR_PMON_BASE 0x0E00L
#define CHA_MSR_PMON_CTL_BASE 0x0E01L
#define CHA_MSR_PMON_FILTER0_BASE 0x0E05L
#define CHA_MSR_PMON_FILTER1_BASE 0x0E06L
#define CHA_MSR_PMON_STATUS_BASE 0x0E07L
#define CHA_MSR_PMON_CTR_BASE 0x0E08L

// Calculate addr of the unit control register for a CHA (core) pmon unit (UPMRM 1.8.1)
#define CHA_MSR_PMON_UNIT_CTRL(core) CHA_MSR_PMON_BASE + core * 0x10UL 
// Calculate addr of the n-th control register within a CHA (core) pmon unit (UPMRM 1.8.1)
#define CHA_MSR_PMON_CTRL(core, n) CHA_MSR_PMON_UNIT_CTRL(core) + 1UL + n 
// Calculate addr of the n-th counter register within a CHA (core) pmon unit (UPMRM 1.8.1)
#define CHA_MSR_PMON_CTR(core, n) CHA_MSR_PMON_CTR_BASE + core * 0x10UL + n

// Bit offsets
#define CHA_MSR_PMON_FILTER0_state 17L      // choose LLC_LOOKUP events to filter by state

// PMON Mesh Performance Monitoring Events, UPMRM 2.2.3
#define VERT_RING_AD_IN_USE 0xa6UL  // vertical address ring in use
#define HORZ_RING_AD_IN_USE 0xa7UL  // horizontal address ring in use
#define VERT_RING_AK_IN_USE 0xa8UL  // vertical acknowledge ring in use
#define HORZ_RING_AK_IN_USE 0xa9UL  // horizontal acknowledge ring in use
#define VERT_RING_BL_IN_USE 0xaaUL  // vertical block/data ring in use
#define HORZ_RING_BL_IN_USE 0xabUL  // horizontal block/data ring in use
#define VERT_RING_IV_IN_USE 0xacUL  // vertical invalidate ring in use
#define HORZ_RING_IV_IN_USE 0xadUL  // horizontal invalidate ring in use
#define CMS_CLOCKTICKS 0xc0         // CMS clock ticks

// PMON CHA Performance Monitoring Events, UPMRM 2.2.8
#define LLC_LOOKUP 0x34UL
#define BYPASS_CHA_IMC 0x57UL
#define IMC_READS_COUNT 0x59UL

#endif // PMON_REG_DEFS_H_
