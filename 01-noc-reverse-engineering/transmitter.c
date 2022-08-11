#include "../util/util.h"
#include "../util/machine_const.h"
#include <semaphore.h>
#include <sys/resource.h> 
#include <sys/mman.h>
#include <string.h>
#include <x86intrin.h>
#include <fcntl.h>

// Uncomment to print out the generated EV
// #define PRINT_EV_DEBUG

#define BUF_SIZE 400 * 1024UL * 1024 /* Buffer Size -> 400*1MB */

struct Node* generate_ev(struct Node **start_ptr, int ev_size, int llc_slice, int llc_set, void *buffer);
struct Node *merge_ev_arrays(uint64_t *ev_list[], int num_evs, int ev_size);
void generate_ev_array(uint64_t *ev, int ev_size, int llc_slice, int llc_set, void *buffer);

int main(int argc, char **argv)
{
	int i;

	// Check arguments
	if (argc != 4) {
		fprintf(stderr, "Wrong Input! Enter desired core ID, slice A ID, slice B ID!\n");
		printf("Enter: %s <core_ID> <slice_a> <slice_b>\n", argv[0]);
		exit(1);
	}

	// Parse core ID (CHA)
	int core;
	sscanf(argv[1], "%d", &core);
	if (core > NUM_CHA - 1 || core < 0) {
		fprintf(stderr, "Wrong core! core_ID should be less than %d and more than 0!\n", NUM_CORES_PER_SOCKET);
		exit(1);
	}

	// Parse slice A number
	int slice_a;
	sscanf(argv[2], "%d", &slice_a);
	if (slice_a > NUM_CHA - 1 || slice_a < 0) {
		fprintf(stderr, "Wrong slice a! slice_ID should be less than %d and more than 0!\n", LLC_CACHE_SLICES);
		exit(1);
	}

	// Parse slice B number
	int slice_b;
	sscanf(argv[3], "%d", &slice_b);
	if (slice_b > NUM_CHA - 1 || slice_b < 0) {
		fprintf(stderr, "Wrong slice b! slice_ID should be less than %d and more than 0!\n", LLC_CACHE_SLICES);
		exit(1);
	}

	uint64_t index1, index2, index3, offset;

	// Allocate large buffer (pool of addresses)
	void *buffer = mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
	if (buffer == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	// Write data to the buffer so that any copy-on-write
	// mechanisms will give us our own copies of the pages.
	memset(buffer, 0, BUF_SIZE);

	// Set the scheduling priority to high to avoid interruptions
	// (lower priorities cause more favorable scheduling, and -20 is the max)
	setpriority(PRIO_PROCESS, 0, -20);

	// Pin the monitoring program to the desired core
	int cpu = cha_id_to_cpu[core];
	pin_cpu(cpu);

	// Mutex to avoid colliding with tx when creating EVs
	sem_t *setup_sem = sem_open("setup_sem", 0);
	if (setup_sem == SEM_FAILED) {
		perror("rx sem_open cha_pmon_sem");
		return -1;
	}
	sem_wait(setup_sem);

	// Prepare each EV
	int ev_size = 20;
	int llc_set_1 = 10;
	int llc_set_2 = (llc_set_1 + L2_CACHE_SETS) % LLC_CACHE_SETS_PER_SLICE; // The set 0 in L2 maps to sets 0 and 1024 in LLC

	// Multi-set parameters
	int second_set_offset = 9;
	int num_l2_ev_sets = 2;
	uint64_t *ev_list[num_l2_ev_sets];

	if (num_l2_ev_sets > 4) {
		printf("Error: num_l2_ev_sets > 4 is not supported\n");
		exit(1);
	}

	struct Node *ev_a = NULL;
	struct Node *current = NULL;

	uint64_t ev1[ev_size];
	generate_ev_array(ev1, (ev_size + 1) / 2, slice_a, llc_set_1, buffer); // use (ev_size + 1)/2 to force rounding up on odd nums
	generate_ev_array(&ev1[(ev_size + 1) / 2], ev_size / 2, slice_a, llc_set_2, buffer);
	ev_list[0] = ev1;

	uint64_t ev2[ev_size];
	if (num_l2_ev_sets > 1) {
		generate_ev_array(ev2, (ev_size + 1) / 2, slice_a, llc_set_1 + second_set_offset, buffer);
		generate_ev_array(&ev2[(ev_size + 1) / 2], ev_size / 2, slice_a, llc_set_2 + second_set_offset, buffer);
		ev_list[1] = ev2;
	}

	uint64_t ev3[ev_size];
	if (num_l2_ev_sets > 2) {
		generate_ev_array(ev3, (ev_size + 1) / 2, slice_a, llc_set_1 + 2 * second_set_offset, buffer);
		generate_ev_array(&ev3[(ev_size + 1) / 2], ev_size / 2, slice_a, llc_set_2 + 2 * second_set_offset, buffer); 
		ev_list[2] = ev3;
	}

	uint64_t ev4[ev_size];
	if (num_l2_ev_sets > 3) {
		generate_ev_array(ev4, (ev_size + 1) / 2, slice_a, llc_set_1 + 3 * second_set_offset, buffer);
		generate_ev_array(&ev4[(ev_size + 1) / 2], ev_size / 2, slice_a, llc_set_2 + 3 * second_set_offset, buffer); 
		ev_list[3] = ev4;
	}

	ev_a = merge_ev_arrays(ev_list, num_l2_ev_sets, ev_size);

	// Flush 
	current = ev_a;
	while (current != NULL) {

#ifdef PRINT_EV_DEBUG
		// Debug
		printf("EV A address: %p, l1 index: %ld, l2 index: %ld, l3 index: %ld, l3 slice: %ld\n",
			(void *)current->address,
			get_cache_set_index((uint64_t)current->address, 1),
			get_cache_set_index((uint64_t)current->address, 2),
			get_cache_set_index((uint64_t)current->address, 3),
			get_cache_slice_index(current->address)
		);
#endif

		_mm_clflush(current->address);
		current = current->next;
	}

	// Repeat for ev_b
	struct Node *ev_b = NULL;
	generate_ev_array(ev1, (ev_size + 1) / 2, slice_b, llc_set_1, buffer);
	generate_ev_array(&ev1[(ev_size + 1) / 2], ev_size / 2, slice_b, llc_set_2, buffer);

	if (num_l2_ev_sets > 1) {
		generate_ev_array(ev2, (ev_size + 1) / 2, slice_b, llc_set_1 + second_set_offset, buffer);
		generate_ev_array(&ev2[(ev_size + 1) / 2], ev_size / 2, slice_b, llc_set_2 + second_set_offset, buffer);
	}

	if (num_l2_ev_sets > 2) {
		generate_ev_array(ev3, (ev_size + 1) / 2, slice_b, llc_set_1 + 2 * second_set_offset, buffer);
		generate_ev_array(&ev3[(ev_size + 1) / 2], ev_size / 2, slice_b, llc_set_2 + 2 * second_set_offset, buffer);
	}

	if (num_l2_ev_sets > 3) {
		generate_ev_array(ev4, (ev_size + 1) / 2, slice_b, llc_set_1 + 3 * second_set_offset, buffer);
		generate_ev_array(&ev4[(ev_size + 1) / 2], ev_size / 2, slice_b, llc_set_2 + 3 * second_set_offset, buffer);
	}

	ev_b = merge_ev_arrays(ev_list, num_l2_ev_sets, ev_size);

	// Flush monitoring set
	current = ev_b;
	while (current != NULL) {
#ifdef PRINT_EV_DEBUG
		// Debug
		printf("EV B address: %p, l1 index: %ld, l2 index: %ld, l3 index: %ld, l3 slice: %ld\n",
			(void *)current->address,
			get_cache_set_index((uint64_t)current->address, 1),
			get_cache_set_index((uint64_t)current->address, 2),
			get_cache_set_index((uint64_t)current->address, 3),
			get_cache_slice_index(current->address)
		);
#endif

		_mm_clflush(current->address);
		current = current->next;
	}

	// Read both ev_a and ev_b from memory
	current = ev_a;
	while (current != NULL) {
		_mm_lfence();
		asm volatile("movq (%0), %%rax" ::"r"(current->address)
					: "rax");
		current = current->next;
	}

	current = ev_b;
	while (current != NULL) {
		_mm_lfence();
		asm volatile("movq (%0), %%rax" ::"r"(current->address)
					: "rax");
		current = current->next;
	}

	_mm_lfence();

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
	sem_post(tx_ready);
	sem_wait(rx_ready);

	// Spam the ring interconnect (until killed)
	while (1) {
		// Load each eviction set alternately
		// Send all loads concurrently (no serialization)
		current = ev_a;
		while (current != NULL) {
			asm volatile("movq (%0), %%rax" ::"r"(current->address)
						: "rax");
			current = current->next;
		}
		current = ev_b;
		while (current != NULL) {
			asm volatile("movq (%0), %%rax" ::"r"(current->address)
						: "rax");
			current = current->next;
		}
	}

	// Free the buffer
	munmap(buffer, BUF_SIZE);

	sem_close(tx_ready);
	sem_close(rx_ready);

	// Clean up lists
	struct Node *tmp = NULL;
	for (current = ev_a; current != NULL; tmp = current, current = current->next, free(tmp));
	for (current = ev_b; current != NULL; tmp = current, current = current->next, free(tmp));

	return 0;
}

/**
 * Produces linked list EV with given parameters starting at start_ptr. start_ptr should point to the last node of a existing list or NULL if it's the first node.
 * 
 * Returns address of last node of the EV
 */
struct Node* generate_ev(struct Node **start_ptr, int ev_size, int llc_slice, int llc_set, void *buffer) {
	struct Node *current = NULL;
	int i;

	// Find first address in our desired slice and given set 
	uint64_t offset = find_next_address_on_slice_and_set(buffer, llc_slice, llc_set);
	// Save this address in the monitoring set
	append_string_to_linked_list(start_ptr, buffer + offset);

	current = *start_ptr;
	while (current->next != NULL) {
		current = current->next;
	}
	// Get the L1, L2 and L3 cache set indexes of the monitoring set
	uint64_t index3 = get_cache_set_index((uint64_t)current->address, 3);
	uint64_t index2 = get_cache_set_index((uint64_t)current->address, 2);
	uint64_t index1 = get_cache_set_index((uint64_t)current->address, 1);

	// Find next addresses which are residing in the desired slice and the same sets in L3/L2/L1
	for (i = 1; i < ev_size; i++) {
		offset = LLC_INDEX_STRIDE; // skip to next address with the same LLC cache set index
		while (index1 != get_cache_set_index((uint64_t)current->address + offset, 1) ||
			   index2 != get_cache_set_index((uint64_t)current->address + offset, 2) ||
			   index3 != get_cache_set_index((uint64_t)current->address + offset, 3) ||
			   llc_slice != get_cache_slice_index(current->address + offset)) {
			offset += LLC_INDEX_STRIDE;
		}
		append_string_to_linked_list(start_ptr, current->address + offset);
		current = current->next;
	}
	return current;
}

void generate_ev_array(uint64_t *ev, int ev_size, int llc_slice, int llc_set, void *buffer) {
	int i;

	// Find first address in our desired slice and given set 
	uint64_t offset = find_next_address_on_slice_and_set(buffer, llc_slice, llc_set);
	ev[0] = (uint64_t)buffer + offset;

	// Get the L1, L2 and L3 cache set indexes of the monitoring set
	uint64_t index3 = get_cache_set_index(ev[0], 3);
	uint64_t index2 = get_cache_set_index(ev[0], 2);
	uint64_t index1 = get_cache_set_index(ev[0], 1);

	// Find next addresses which are residing in the desired slice and the same sets in L3/L2/L1
	for (i = 1; i < ev_size; i++) {
		offset = LLC_INDEX_STRIDE; // skip to next address with the same LLC cache set index
		uint64_t candidate_addr = ev[i - 1] + offset;
		while (index1 != get_cache_set_index(candidate_addr, 1) ||
			   index2 != get_cache_set_index(candidate_addr, 2) ||
			   index3 != get_cache_set_index(candidate_addr, 3) ||
			   llc_slice != get_cache_slice_index((void *)candidate_addr)) {
			candidate_addr += LLC_INDEX_STRIDE;
		}
		ev[i] = candidate_addr;
	}
}

struct Node *merge_ev_arrays(uint64_t *ev_list[], int num_evs, int ev_size) {
	struct Node *ev = NULL;
	for (int ev_element = 0; ev_element < ev_size; ev_element++) {
		for (int ev_num = 0; ev_num < num_evs; ev_num++) {
			append_string_to_linked_list(&ev, (void *)ev_list[ev_num][ev_element]);
		}
	}
	return ev;
}
