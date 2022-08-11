#include "../util/machine_const.h"
#include "../util/util.h"
#include <semaphore.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <string.h>

#define BUF_SIZE 400 * 1024UL * 1024 /* Buffer Size -> 400*1MB */

// Uncomment to print out the generated EV
// #define PRINT_DEBUG

static inline void access_ev(struct Node *ev)
{
	// Access EV multiple times
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
	int i;

	// Check arguments
	if (argc != 4) {
		fprintf(stderr, "Wrong Input! Enter desired core ID, ms slice ID, ev slice ID\n");
		fprintf(stderr, "Enter: %s <core_ID> <ms_slice> <ev_slice> \n", argv[0]);
		exit(1);
	}

	// Parse core ID
	int core_ID;
	sscanf(argv[1], "%d", &core_ID);
	if (core_ID > NUM_CHA - 1 || core_ID < 0) {
		fprintf(stderr, "Wrong core! core_ID should be less than %d and more than 0!\n", NUM_CORES_PER_SOCKET);
		exit(1);
	}

	// Parse MS slice number
	int ms_slice;
	sscanf(argv[2], "%d", &ms_slice);
	if (ms_slice > LLC_CACHE_SLICES - 1 || ms_slice < 0) {
		fprintf(stderr, "Wrong slice! slice_ID should be less than %d and more than 0!\n", LLC_CACHE_SLICES);
		exit(1);
	}

	// Parse EV slice number
	int ev_slice;
	sscanf(argv[3], "%d", &ev_slice);
	if (ev_slice > LLC_CACHE_SLICES - 1 || ev_slice < 0) {
		fprintf(stderr, "Wrong slice! slice_ID should be less than %d and more than 0!\n", LLC_CACHE_SLICES);
		exit(1);
	}

	// Using fixed cache sets for this experiments
	int ev_llc_set_1 = 5;
	int ev_llc_set_2 = (ev_llc_set_1 + L2_CACHE_SETS) % LLC_CACHE_SETS_PER_SLICE; // The set 0 in L2 maps to sets 0 and 1024 in LLC
	int ms_llc_set = ev_llc_set_1;

	// Prepare output filename
	FILE *output_file = fopen("rx_out.log", "w");

	// Allocate large buffer (pool of addresses)
	void *buffer = mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
	if (buffer == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	// Write data to the buffer so that any copy-on-write
	// mechanisms will give us our own copies of the pages.
	memset(buffer, 0, BUF_SIZE);

	// Pin the monitoring program to the desired core
	int cpu = cha_id_to_cpu[core_ID];

	pin_cpu(cpu);

	// Set the scheduling priority to high to avoid interruptions
	// (lower priorities cause more favorable scheduling, and -20 is the max)
	setpriority(PRIO_PROCESS, 0, -20);

	// Mutex to avoid colliding with tx when creating EVs
	sem_t *setup_sem = sem_open("setup_sem", 0);
	if (setup_sem == SEM_FAILED) {
		perror("rx sem_open cha_pmon_sem");
		return -1;
	}
	sem_wait(setup_sem);

	// Prepare EV
	int ev_size = 16;
	struct Node *ev = NULL;
	struct Node *curr_node = NULL;

	// Prepare monitoring set
	int monitoring_set_size = 16;
	void **monitoring_set = NULL;
	void **current = NULL, **previous = NULL;

	uint64_t index1, index2, offset, candidate_addr;

	// Find first address in our desired slice and given set
	offset = find_next_address_on_slice_and_set(buffer, ev_slice, ev_llc_set_1);

	// Save this address in the EV set
	append_string_to_linked_list(&ev, (void *)((uint64_t)buffer + offset));

	// Get the L1, L2 and L3 cache set indexes of the EV set
	index2 = get_cache_set_index((uint64_t)ev->address, 2);
	index1 = get_cache_set_index((uint64_t)ev->address, 1);

	// Find next addresses which are residing in the desired slice and the same sets in L2/L1
	curr_node = ev;
	for (i = 1; i < ev_size; i++) {
		offset = L2_INDEX_STRIDE; // skip to the next address with the same LLC cache set index
		candidate_addr = (uint64_t)curr_node->address + offset;
		while (index1 != get_cache_set_index(candidate_addr, 1) ||
			   index2 != get_cache_set_index(candidate_addr, 2) ||
			   ev_slice != get_cache_slice_index((void *)candidate_addr)) {
			candidate_addr += L2_INDEX_STRIDE;
		}

		append_string_to_linked_list(&ev, (void *)candidate_addr);
		curr_node = curr_node->next;
	}

#ifdef PRINT_DEBUG
	curr_node = ev;
	while (curr_node != NULL) {
		printf("Rx EV: %p: (%ld, %ld, %ld, %ld)\n", curr_node->address,
			   get_cache_set_index((uint64_t)(curr_node->address), 1),
			   get_cache_set_index((uint64_t)(curr_node->address), 2),
			   get_cache_set_index((uint64_t)(curr_node->address), 3),
			   get_cache_slice_index(curr_node->address));
		curr_node = curr_node->next;
	}
#endif

	// Find first address in our desired slice and given set
	offset = find_next_address_on_slice_and_set(buffer, ms_slice, ms_llc_set);

	// Save this address in the monitoring set
	monitoring_set = (void **)((uint64_t)buffer + offset);

	// Get the L1, L2 and L3 cache set indexes of the monitoring set
	index2 = get_cache_set_index((uint64_t)monitoring_set, 2);
	index1 = get_cache_set_index((uint64_t)monitoring_set, 1);

	// Find next addresses which are residing in the desired slice and the same sets in L3/L2/L1
	current = monitoring_set;
	for (i = 1; i < monitoring_set_size; i++) {
		offset = L2_INDEX_STRIDE; // skip to the next address with the same L2 cache set index
		candidate_addr = (uint64_t)current + offset;
		while (index1 != get_cache_set_index(candidate_addr, 1) ||
			   index2 != get_cache_set_index(candidate_addr, 2) ||
			   ms_slice != get_cache_slice_index((void *)candidate_addr)) {
			
#ifdef PRINT_DEBUG
			printf("Testing addr %p: index1=%2ld, index2=%2ld, index3=%4ld, ms_slice=%2ld\n",
					(void *)candidate_addr,
					get_cache_set_index(candidate_addr, 1),
					get_cache_set_index(candidate_addr, 2),
					get_cache_set_index(candidate_addr, 3),
					get_cache_slice_index((void *)candidate_addr));
			#endif

			candidate_addr += L2_INDEX_STRIDE;
		}

#ifdef PRINT_DEBUG
		printf("Add addr %p: index1=%2ld, index2=%2ld, index3=%2ld, ms_slice=%2ld\n",
					(void *)(candidate_addr),
					get_cache_set_index(candidate_addr, 1),
					get_cache_set_index(candidate_addr, 2),
					get_cache_set_index(candidate_addr, 3),
					get_cache_slice_index((void *) candidate_addr));
#endif

		// Set up pointer chasing. The idea is: *addr1 = addr2; *addr2 = addr3; and so on.
		*current = (void *)(candidate_addr);
		current = *current;
	}

	// Make last item point back to the first one (useful for the loop)
	*current = monitoring_set;

#ifdef PRINT_DEBUG
	// Print debug if needed
	current = monitoring_set;
	for (i = 0; i < monitoring_set_size; i++) {
		printf("MS: %p: (%ld, %ld, %ld, %ld)\n", current,
			   get_cache_set_index((uint64_t)current, 1),
			   get_cache_set_index((uint64_t)current, 2),
			   get_cache_set_index((uint64_t)current, 3),
			   get_cache_slice_index(current));
		current = *current;
	}
#endif

	// Allocate buffers for results
	const int repetitions = 10000;
	uint64_t *samples_x = (uint64_t *)malloc(sizeof(*samples_x) * repetitions);
	uint32_t *samples_y = (uint32_t *)malloc(sizeof(*samples_y) * repetitions);

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
	uint64_t cycles, end;
	cycles = get_time();
	end = cycles + (uint64_t)500000;
	while (cycles < end) {
		cycles = get_time();
	}

	// Read monitoring set from memory into cache
	// The addresses should all fit in the LLC
	current = monitoring_set;
	for (i = 0; i < monitoring_set_size; i++) {
		asm volatile("movq (%0), %0"
					 : "+rm"(current) /* output */ ::"memory");
	}

	// Time LLC loads
	current = monitoring_set;
	for (i = 0; i < repetitions; i++) {

		if (i % (monitoring_set_size/1) == 0) { // evict on every repetition right now
			access_ev(ev);
		}

		// Time accesses to the monitoring set
		asm volatile(
			".align 32\n\t"
			"lfence\n\t"
			"rdtsc\n\t" /* eax = TSC (timestamp counter)*/
			"shl $32, %%rdx\n\t"
			"or %%rdx, %%rax\n\t"

			"movq %%rax, %%r8\n\t" /* r8 = rax; this is to back up rax into another register */

			"movq (%2), %2\n\t" /* current = *current; LOAD */

			"rdtscp\n\t" /* eax = TSC (timestamp counter) */
			"shl $32, %%rdx\n\t"
			"or %%rdx, %%rax\n\t"

			"sub %%r8, %%rax\n\t" /* rax = rax - r8; get timing difference between the second timestamp and the first one */

			"movq %%r8, %0\n\t"										   /* result_x[i] = r8 */
			"movl %%eax, %1\n\t"									   /* result_y[i] = eax */
			: "=rm"(samples_x[i]), "=rm"(samples_y[i]), "+rm"(current) /*output*/
			:
			: "rax", "rcx", "rdx", "r8", "memory");
	}

	printf("Starting file write\n");
	// Store the samples to disk
	for (i = 0; i < repetitions; i++) {
		fprintf(output_file, "%" PRIu64 " %" PRIu32 "\n", samples_x[i], samples_y[i]);
	}
	printf("Ending file write\n");

	// Free the buffers and file
	munmap(buffer, BUF_SIZE);
	fclose(output_file); 
	free(samples_x);
	free(samples_y);

	sem_close(tx_ready);
	sem_close(rx_ready);

	return 0;
}
