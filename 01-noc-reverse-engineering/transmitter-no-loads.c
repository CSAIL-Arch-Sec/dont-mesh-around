#include "../util/util.h"
#include "../util/machine_const.h"
#include <semaphore.h>
#include <sys/resource.h> 

int main(int argc, char **argv)
{
	int i;

	// Check arguments
	if (argc != 2) {
		fprintf(stderr, "Wrong Input! Enter desired core ID, slice A ID, slice B ID, set ID, and tx_ID!\n");
		printf("Enter: %s <core_ID> <slice_a> <slice_b> <set_ID> <tx_ID>\n", argv[0]);
		exit(1);
	}

	// Parse core ID (CHA)
	int core;
	sscanf(argv[1], "%d", &core);
	if (core > NUM_CHA - 1 || core < 0) {
		fprintf(stderr, "Wrong core! core_ID should be less than %d and more than 0!\n", NUM_CHA);
		exit(1);
	}

	// Set the scheduling priority to high to avoid interruptions
	// (lower priorities cause more favorable scheduling, and -20 is the max)
	setpriority(PRIO_PROCESS, 0, -20);

	// Pin the monitoring program to the desired core
	int cpu = cha_id_to_cpu[core];
	// printf("Pinning to cpu %d\n", cpu);
	pin_cpu(cpu);

	// Barrier for experiment start
	sem_t *tx_ready_sem = sem_open("tx_ready", 0);
	sem_t *rx_ready_sem = sem_open("rx_ready", 0);
	if (tx_ready_sem == SEM_FAILED || rx_ready_sem == SEM_FAILED) {
		perror("sem_open tx_ready or rx_ready in transmitter-no-loads");
		return -1;
	}

	// Signal to rx that tx is ready (and not using the NoC)
	sem_post(tx_ready_sem);
	sem_wait(rx_ready_sem);

	// Spam the ring interconnect (until killed)
	while (1) { }

	return 0;
}
