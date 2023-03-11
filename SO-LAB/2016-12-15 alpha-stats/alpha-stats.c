#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <ctype.h>
#include <time.h>

#define LETTERS_NUM 26
#define SHM_DIM sizeof(shm)
#define S_F 0

typedef struct {
	float occurrences[LETTERS_NUM];
} shm;

int WAIT(int sem_des, int num_semaforo) {
	struct sembuf ops[1] = {{num_semaforo, -1, 0}};
	return semop(sem_des, ops, 1);
}

int SIGNAL(int sem_des, int num_semaforo) {
	struct sembuf ops[1] = {{num_semaforo, +1, 0}};
	return semop(sem_des, ops, 1);
}

int init_shm() {
	int shm_des;

	if ((shm_des = shmget(IPC_PRIVATE, SHM_DIM, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("shmget");
		exit(1);
	}

	return shm_des;
}

int init_sem() {
	int sem_des;

	if ((sem_des = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("semget");
		exit(1);
	}

	if (semctl(sem_des, S_F, SETVAL, 1) == -1) {
		perror("semctl S_F");
		exit(1);
	}

	return sem_des;
}

void f_child(shm *data, int sem_des, int id, char *argv) {
	srand(time(NULL) + id);
	int fd;
	struct stat sb;

	if ((fd = open(argv, O_RDONLY)) == -1) {
		fprintf(stderr, "open F%d %s\n", id, argv);
		exit(1);
	}

	if (fstat(fd, &sb) == -1) {
		fprintf(stderr, "fstat F%d %s\n", id, argv);
		exit(1);
	}

	if (!S_ISREG(sb.st_mode)) {
		fprintf(stderr, "%s non è un file regolare\n", argv);
		exit(1);
	}

	char *file;

	if ((file = (char *)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		fprintf(stderr, "mmap F%d\n", id);
		exit(1);
	}

	close(fd);

	for (int i = 0; i < sb.st_size; i++) {
		if (tolower(file[i]) >= 'a' && tolower(file[i]) <= 'z') {
			WAIT(sem_des, S_F);
			data->occurrences[tolower(file[i]) - 'a']++;
			SIGNAL(sem_des, S_F);
			usleep(((rand() % 10) + 1) / 10);
		}
	}

	munmap(file, sb.st_size);

	exit(0);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <file-1> [file-2] [file-3] [...]\n", argv[0]);
		exit(1);
	}

	int shm_des = init_shm();
	int sem_des = init_sem();
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}
	// File
	for (int i = 0; i < argc - 1; i++) {
		if (!fork()) {
			f_child(data, sem_des, i + 1, argv[i + 1]);
		}
	}

	for (int i = 0; i < argc - 1; i++) {
		wait(NULL);
	}

	float total = 0;

	for (int i = 0; i < LETTERS_NUM; i++) {
		total += data->occurrences[i];
	}

	printf("frequenze:\n");

	for (int i = 0; i < LETTERS_NUM; i++) {
		printf("%c: %.2f%%\n", i + 'a', (data->occurrences[i] / total) * 100);
	}

	shmctl(shm_des, IPC_RMID, NULL);
	semctl(sem_des, 0, IPC_RMID, 0);

	exit(0);
}
