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
	if (argc != 4) {
		fprintf(stderr, "Wrong Input! Enter desired core ID, slice ID, and channel interval!\n");
		printf("Enter: %s <core_ID> <slice_ID> <interval>\n", argv[0]);
		exit(1);
	}

	// Parse core ID
	int core_ID;
	sscanf(argv[1], "%d", &core_ID);
	if (core_ID > NUM_CORES_PER_SOCKET - 1 || core_ID < 0) {
		fprintf(stderr, "Wrong core! core_ID should be less than %d and more than 0!\n", NUM_CORES_PER_SOCKET);
		exit(1);
	}

	// Parse slice number
	int slice_ID;
	sscanf(argv[2], "%d", &slice_ID);
	if (slice_ID > NUM_CHA - 1 || slice_ID < 0) {
		fprintf(stderr, "Wrong slice! slice_ID should be less than %d and more than 0!\n", LLC_CACHE_SLICES);
		exit(1);
	}

	// Parse channel interval
	uint32_t interval = 1; // C does not like this if not initialized
	sscanf(argv[3], "%" PRIu32, &interval);
	if (interval <= 0) {
		printf("Wrong interval! interval should be greater than 0!\n");
		exit(1);
	}

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

	// EV preparation variables
	int l2_set_1 = 0;
	int l2_set_2 = 165;
	int n_of_l2_sets_per_ev = 2;
	int n_of_ev_addresses_per_l2_set = 20;
	struct Node *ev_1 = NULL, *ev_2 = NULL;
	struct Node *current = NULL;
	uint64_t index1, index2, offset;

	//////////////////////////////////////////////////////////////////////
	// Prepare first EV (remote)
	//////////////////////////////////////////////////////////////////////

	// Find first address in our desired slice and the first set of the EV
	offset = find_next_address_on_slice_and_set(buffer, slice_ID, l2_set_1);

	// Save this address in the monitoring set
	append_string_to_linked_list(&ev_1, (void *)((uint64_t)buffer + offset));

	// Get the L1 and L2 cache set indexes of the monitoring set
	index2 = get_cache_set_index((uint64_t)ev_1->address, 2);
	index1 = get_cache_set_index((uint64_t)ev_1->address, 1);

	// Find next addresses which are residing in the desired slice and the same sets in L2/L1
	// These addresses will distribute across 2 LLC sets
	current = ev_1;
	for (i = 1; i < n_of_ev_addresses_per_l2_set; i++) {
		offset = L2_INDEX_STRIDE; // skip to the next address with the same L2 cache set index
		while (index1 != get_cache_set_index((uint64_t)current->address + offset, 1) ||
			   index2 != get_cache_set_index((uint64_t)current->address + offset, 2) ||
			   slice_ID != get_cache_slice_index((void *)((uint64_t)current->address + offset))) {
			offset += L2_INDEX_STRIDE;
		}

		append_string_to_linked_list(&ev_1, (void *)((uint64_t)current->address + offset));
		current = current->next;
	}

	// Find first address in our desired slice and the second set of the EV
	offset = find_next_address_on_slice_and_set(buffer, slice_ID, l2_set_2);

	// Save this address in the monitoring set
	append_string_to_linked_list(&ev_2, (void *)((uint64_t)buffer + offset));

	// Get the L1 and L2 cache set indexes of the monitoring set
	index2 = get_cache_set_index((uint64_t)ev_2->address, 2);
	index1 = get_cache_set_index((uint64_t)ev_2->address, 1);

	// Find next addresses which are residing in the desired slice and the same sets in L2/L1
	// These addresses will distribute across 2 LLC sets
	current = ev_2;
	for (i = 1; i < n_of_ev_addresses_per_l2_set; i++) {
		offset = L2_INDEX_STRIDE; // skip to the next address with the same L2 cache set index
		while (index1 != get_cache_set_index((uint64_t)current->address + offset, 1) ||
			   index2 != get_cache_set_index((uint64_t)current->address + offset, 2) ||
			   slice_ID != get_cache_slice_index((void *)((uint64_t)current->address + offset))) {
			offset += L2_INDEX_STRIDE;
		}

		append_string_to_linked_list(&ev_2, (void *)((uint64_t)current->address + offset));
		current = current->next;
	}

	// Merge ev_1 and ev_2
	struct Node *ev = NULL;
	struct Node *head_1 = ev_1;
	struct Node *head_2 = ev_2;
	for (i = 0; i < n_of_ev_addresses_per_l2_set; i++) {
		append_string_to_linked_list(&ev, head_1->address);
		append_string_to_linked_list(&ev, head_2->address);

		head_1 = head_1->next;
		head_2 = head_2->next;
	}

	//////////////////////////////////////////////////////////////////////
	// Prepare second EV (local)
	//////////////////////////////////////////////////////////////////////

	ev_1 = NULL;
	ev_2 = NULL;

	// Find first address in our desired slice and the first set of the EV
	offset = find_next_address_on_slice_and_set(buffer, core_ID, l2_set_1);

	// Save this address in the monitoring set
	append_string_to_linked_list(&ev_1, (void *)((uint64_t)buffer + offset));

	// Get the L1 and L2 cache set indexes of the monitoring set
	index2 = get_cache_set_index((uint64_t)ev_1->address, 2);
	index1 = get_cache_set_index((uint64_t)ev_1->address, 1);

	// Find next addresses which are residing in the desired slice and the same sets in L2/L1
	// These addresses will distribute across 2 LLC sets
	current = ev_1;
	for (i = 1; i < n_of_ev_addresses_per_l2_set; i++) {
		offset = L2_INDEX_STRIDE; // skip to the next address with the same L2 cache set index
		while (index1 != get_cache_set_index((uint64_t)current->address + offset, 1) ||
			   index2 != get_cache_set_index((uint64_t)current->address + offset, 2) ||
			   core_ID != get_cache_slice_index((void *)((uint64_t)current->address + offset))) {
			offset += L2_INDEX_STRIDE;
		}

		append_string_to_linked_list(&ev_1, (void *)((uint64_t)current->address + offset));
		current = current->next;
	}

	// Find first address in our desired slice and given set
	offset = find_next_address_on_slice_and_set(buffer, core_ID, l2_set_2);

	// Save this address in the monitoring set
	append_string_to_linked_list(&ev_2, (void *)((uint64_t)buffer + offset));

	// Get the L1 and L2 cache set indexes of the monitoring set
	index2 = get_cache_set_index((uint64_t)ev_2->address, 2);
	index1 = get_cache_set_index((uint64_t)ev_2->address, 1);

	// Find next addresses which are residing in the desired slice and the same sets in L2/L1
	// These addresses will distribute across 2 LLC sets
	current = ev_2;
	for (i = 1; i < n_of_ev_addresses_per_l2_set; i++) {
		offset = L2_INDEX_STRIDE; // skip to the next address with the same L2 cache set index
		while (index1 != get_cache_set_index((uint64_t)current->address + offset, 1) ||
			   index2 != get_cache_set_index((uint64_t)current->address + offset, 2) ||
			   core_ID != get_cache_slice_index((void *)((uint64_t)current->address + offset))) {
			offset += L2_INDEX_STRIDE;
		}

		append_string_to_linked_list(&ev_2, (void *)((uint64_t)current->address + offset));
		current = current->next;
	}

	// Merge ev_1 and ev_2
	struct Node *ev_local = NULL;
	head_1 = ev_1;
	head_2 = ev_2;
	for (i = 0; i < n_of_ev_addresses_per_l2_set; i++) {
		append_string_to_linked_list(&ev_local, head_1->address);
		append_string_to_linked_list(&ev_local, head_2->address);

		head_1 = head_1->next;
		head_2 = head_2->next;
	}

	//////////////////////////////////////////////////////////////////////
	// Done setting up EVs
	//////////////////////////////////////////////////////////////////////

#ifdef RANDOM_PATTERN
	char *pattern = "1110011001010000110111110101011110001001111001010001100011110011100100011101110010010100100100011001110101111010111110100010000100100111001111100111011010110110011011011000011010001010000101110010010110001010110000001001111111001111010111111001111111000100000000100011011101011000001100010000000110000011001101101111010100011000100110100011001000100011011000010100100011100101011010011000110011001100001101101001101111011110000100001010001100001100000010111111110111110100110011100000011101001100110011001010101011011101000101110111101000001000110101110000100100010110110100101101001100110101110011000010111011010111111100001101011000000000011101011101000111101111110110010100010000101001100110000010000011111100101101010110001111011111100000001110110100011000011010101100111010100100101100000011000100101011000101111001011011111101101011010100111000101000101111101000111101001111100101100010011111111000100011010101101010100001110000011101011000001101100010100100001110000000100100000000000010100000";
	int patternlen = strlen(pattern);
#endif

	// Read both ev and ev_local from memory
	current = ev;
	i = 0;
	while (current != NULL) {
		#ifdef DEBUG
		printf("Tx EV %2d: %p: (%ld, %ld, %ld, %ld)\n", i, current->address,
			   get_cache_set_index((uint64_t)(current->address), 1),
			   get_cache_set_index((uint64_t)(current->address), 2),
			   get_cache_set_index((uint64_t)(current->address), 3),
			   get_cache_slice_index(current->address));
		#endif
		_mm_lfence();
		maccess(current->address);
		current = current->next;
		i++;
	}

	i = 0;
	current = ev_local;
	while (current != NULL) {
		#ifdef DEBUG
		printf("Tx EV_Local %2d: %p: (%ld, %ld, %ld, %ld)\n", i, current->address,
			   get_cache_set_index((uint64_t)(current->address), 1),
			   get_cache_set_index((uint64_t)(current->address), 2),
			   get_cache_set_index((uint64_t)(current->address), 3),
			   get_cache_slice_index(current->address));
		#endif
		_mm_lfence();
		maccess(current->address);
		current = current->next;
		i++;
	}

	_mm_lfence();

	//////////////////////////////////////////////////////////////////////
	// Start CC
	//////////////////////////////////////////////////////////////////////

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

	uint64_t start_t;
	uint32_t time;

	// Synchronize
	do {
		start_t = get_time();
	} while ((start_t % interval) > 10);

	// Send
	for (time = 0; time < UINT32_MAX; time++) {

		#ifdef RANDOM_PATTERN
		if (pattern[time % patternlen] == '1') {
		#else
		if (time % 2 == 0) {
		#endif
			// Send 1 by spamming
			while ((get_time() - start_t) < (interval * time)) {
				current = ev;
				while (current != NULL) {
					maccess(current->address);
					current = current->next;
				}
				current = ev_local;
				while (current != NULL) {
					maccess(current->address);
					current = current->next;
				}
			}
		} else {
			// Send 0 by doing nothing
			while ((get_time() - start_t) < (interval * time)) {}
		}
	}

	// Free the buffer
	munmap(buffer, BUF_SIZE);

	sem_close(tx_ready);
	sem_close(rx_ready);

	// Clean up lists
	struct Node *tmp = NULL;
	for (current = ev; current != NULL; tmp = current, current = current->next, free(tmp));
	for (current = ev_local; current != NULL; tmp = current, current = current->next, free(tmp));

	return 0;
}
