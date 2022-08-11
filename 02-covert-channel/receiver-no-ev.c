#include "../util/util.h"
#include "../util/machine_const.h"
#include <semaphore.h>
#include <sys/mman.h>
#include <string.h>
#include <x86intrin.h>

#define BUF_SIZE 400 * 1024UL * 1024 /* Buffer Size -> 400*1MB */

int main(int argc, char **argv)
{
	int i;

	// Check arguments
	if (argc != 5) {
		fprintf(stderr, "Wrong Input! Enter desired core ID, slice ID, output filename, and channel interval!\n");
		fprintf(stderr, "Enter: %s <core_ID> <slice_ID> <output_filename> <interval>\n", argv[0]);
		exit(1);
	}

	// Parse core ID
	int core_ID;
	sscanf(argv[1], "%d", &core_ID);
	if (core_ID >= NUM_CHA || core_ID < 0) {
		fprintf(stderr, "Wrong core! core_ID should be in the range [0, %d]!\n", NUM_CHA - 1);
		exit(1);
	}

	// Parse slice number
	int slice_ID;
	sscanf(argv[2], "%d", &slice_ID);
	if (slice_ID >= NUM_CHA || slice_ID < 0) {
		fprintf(stderr, "Wrong slice! slice_ID should be in the range [0, %d]!\n", NUM_CHA - 1);
		exit(1);
	}

	// For this experiment we can use a fixed cache set
	int set_ID = 33;

	// Prepare output filename
	FILE *output_file = fopen(argv[3], "w");

	// Parse channel interval
	uint32_t interval = 1; // C does not like this if not initialized
	sscanf(argv[4], "%" PRIu32, &interval);
	if (interval <= 0) {
		printf("Wrong interval! interval should be greater than 0!\n");
		exit(1);
	}

	// Pin the monitoring program to the desired core
	//
	// This time we do not set the priority like in the RE because
	// doing so would use root and we want our actual attack
	// to be realistic for a user space process
	int cpu = cha_id_to_cpu[core_ID];
	pin_cpu(cpu);

	//////////////////////////////////////////////////////////////////////
	// Set up memory
	//////////////////////////////////////////////////////////////////////

	// Mutex to avoid colliding with tx when creating EVs
	// This is unnecessary when using the hash function
	sem_t *setup_sem = sem_open("setup_sem", 0);
	if (setup_sem == SEM_FAILED) {
		perror("rx sem_open cha_pmon_sem");
		return -1;
	}
	sem_wait(setup_sem);

	// Allocate large buffer (pool of addresses)
	void *buffer = mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
	if (buffer == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	// Write data to the buffer so that any copy-on-write
	// mechanisms will give us our own copies of the pages.
	memset(buffer, 0, BUF_SIZE);

	// Prepare monitoring set
	printf("Rx: starting setup\n");
	int monitoring_set_size = 24;
	struct Node *monitoring_set = NULL;
	struct Node *curr_node = NULL;
	uint64_t index1, index2, offset;

	// Find first address in our desired slice and given set
	offset = find_next_address_on_slice_and_set(buffer, slice_ID, set_ID);

	// Save this address in the monitoring set
	append_string_to_linked_list(&monitoring_set, (void *)((uint64_t)buffer + offset));

	// Get the L1 and L2 cache set indexes of the monitoring set
	index2 = get_cache_set_index((uint64_t)monitoring_set->address, 2);
	index1 = get_cache_set_index((uint64_t)monitoring_set->address, 1);

	// Find next addresses which are residing in the desired slice and the same sets in L2/L1
	// These addresses will distribute across 2 LLC sets
	curr_node = monitoring_set;
	for (i = 1; i < monitoring_set_size; i++) {
		offset = L2_INDEX_STRIDE; // skip to the next address with the same L2 cache set index
		while (index1 != get_cache_set_index((uint64_t)curr_node->address + offset, 1) ||
			   index2 != get_cache_set_index((uint64_t)curr_node->address + offset, 2) ||
			   slice_ID != get_cache_slice_index((void *)((uint64_t)curr_node->address + offset))) {
			offset += L2_INDEX_STRIDE;
		}

		append_string_to_linked_list(&monitoring_set, (void *)((uint64_t)curr_node->address + offset));
		curr_node = curr_node->next;
	}

	curr_node->next = monitoring_set; // Loop back to the beginning

	// Flush monitoring set
	curr_node = monitoring_set;
	for (i = 0; i < monitoring_set_size; i++) {
		_mm_clflush(curr_node->address);
		curr_node = curr_node->next;
	}

	// Prepare samples array
	const int repetitions = 4000000;
	uint32_t *result_x = (uint32_t *)malloc(sizeof(*result_x) * repetitions);
	uint32_t *result_y = (uint32_t *)malloc(sizeof(*result_y) * repetitions);

	printf("Rx: Done with setup\n");

	// Release setup mutex
	sem_post(setup_sem);
	sem_close(setup_sem);
	// Barrier for experiment start
	sem_t *tx_ready = sem_open("tx_ready", 0);
	sem_t *rx_ready = sem_open("rx_ready", 0);
	if (tx_ready == SEM_FAILED || rx_ready == SEM_FAILED) {
		perror("Rx failed to open barrier");
		exit(-1);
	}
	// Signal to tx that rx is ready
	sem_post(rx_ready);
	sem_wait(tx_ready);

	// Wait a bit (give time to the transmitter to warm up)
	wait_cycles(500000);

	// Read monitoring set from memory into cache
	// The addresses should all fit in the LLC
	curr_node = monitoring_set;
	for (i = 0; i < 1000000 * monitoring_set_size; i++) {
		// _mm_lfence();
		maccess(curr_node->address);
		curr_node = curr_node->next;
	}

	// Synchronize
	uint64_t cycles;
	do {
		cycles = get_time();
	} while ((cycles % interval) > 10);

	// Time LLC loads
	for (i = 0; i < repetitions; i++) {

		// Access the addresses sequentially.
		asm volatile(
			".align 16\n\t"
			"lfence\n\t"
			"rdtsc\n\t"					/* eax = TSC (timestamp counter) */
			"movl %%eax, %%r8d\n\t"		/* r8d = eax; this is to back up eax into another register */

			"movq (%2), %%r9\n\t"	    /* r9 = *(curr_node->address); LOAD */
			"movq (%3), %%r9\n\t"	    /* r9 = *(curr_node->next->address); LOAD */
			"movq (%4), %%r9\n\t"	    /* r9 = *(curr_node->next->next->address); LOAD */
			"movq (%5), %%r9\n\t"	    /* r9 = *(curr_node->next->next->next->address); LOAD */

			"rdtscp\n\t"				/* eax = TSC (timestamp counter) */
			"sub %%r8d, %%eax\n\t"		/* eax = eax - r8d; get timing difference between the second timestamp and the first one */

			"movl %%r8d, %0\n\t" 		/* result_x[i] = r8d */
			"movl %%eax, %1\n\t" 		/* result_y[i] = eax */

			: "=rm"(result_x[i]), "=rm"(result_y[i]) /*output*/
			: "r"(curr_node->address), "r"(curr_node->next->address) , "r"(curr_node->next->next->address), "r"(curr_node->next->next->next->address)
			: "rax", "rcx", "rdx", "r8", "r9", "memory");

		curr_node = curr_node->next->next->next->next;
	}

	// Store the samples to disk
	for (i = 0; i < repetitions; i++) {
		fprintf(output_file, "%" PRIu32 " %" PRIu32 "\n", result_x[i] - result_x[0], result_y[i]);
	}

	// Free the buffers and file
	munmap(buffer, BUF_SIZE);
	fclose(output_file);
	sem_close(tx_ready);
	sem_close(rx_ready);
	free(result_x);
	free(result_y);

	return 0;
}
