#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <errno.h>
#include <stdio.h>

int main(int argc, char const *argv[])
{
	if (sem_unlink("setup_sem") != 0) {
		perror("Unlink setup_sem");
	}

	if (sem_unlink("tx_ready") != 0) {
		perror("Unlink tx_ready");
	}

	if (sem_unlink("rx_ready") != 0) {
		perror("Unlink rx_ready");
	}

	return 0;
}
