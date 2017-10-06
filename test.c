#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>
#include <errno.h>
 

typedef unsigned int ulli;

int main(int argc, char *argv[])
{
	int size;
	scanf("%d", &size);
	key_t key = ftok("/tmp", 'm'); // Please don't conflict
	if(key < 0)
		printf("[0] error: %s\n", strerror(errno));
		//std::cerr << "[0] ftok failed, err: " << std::strerror(errno) << std::endl;
	printf("%d\n", key);
	int shm_id = shmget( key, size * size * sizeof(ulli)/* matrix */, IPC_CREAT | 0666);
	if(shm_id < 0)
		printf("[1] error: %s\n", strerror(errno));
//		std::cerr << "[0] shm_id dead, err: " << std::strerror(errno) << std::endl;

	void * shm = shmat(shm_id, NULL, 0);
	if(shm == (void *) -1)
		printf("[2] error: %s\n", strerror(errno));
//		std::cerr << "[0] shm failed, err: " << std::strerror(errno) << std::endl;
	shmdt(shm);
    return 0;
}
