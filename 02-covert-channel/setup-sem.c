#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char const *argv[])
{
	if (sem_open("setup_sem", O_CREAT | O_EXCL, 0600, 1) == SEM_FAILED) {
		perror("Opening setup_sem");
		return -1;
	}

	if (sem_open("tx_ready", O_CREAT | O_EXCL, 0600, 0) == SEM_FAILED) {
		perror("Opening tx_ready");
		return -1;
	}

	if (sem_open("rx_ready", O_CREAT | O_EXCL, 0600, 0) == SEM_FAILED) {
		perror("Opening rx_ready");
		return -1;
	}

	return 0;
}
