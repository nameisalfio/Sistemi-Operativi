#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <ctype.h>
#include <sys/wait.h>

#define LETTERS_NUM 26
#define CHAR_DIM sizeof(CHAR)
#define STATS_DIM sizeof(STATS)

typedef enum {S_P, S_AL, S_MZ} S_TYPE;

typedef struct {
	char letter;
	char done;
} CHAR;

typedef struct {
	int stats[LETTERS_NUM];
} STATS;

int WAIT(int sem_des, int num_semaforo){
	struct sembuf ops[1] = {{num_semaforo, -1, 0}};
	return semop(sem_des, ops, 1);
}
int SIGNAL(int sem_des, int num_semaforo){
	struct sembuf ops[1] = {{num_semaforo, +1, 0}};
	return semop(sem_des, ops, 1);
}

int init_shm(int i) {
	int shm_des;
	size_t SHM_DIM;

	if (i == 0) {
		SHM_DIM = CHAR_DIM;
	}
	else {
		SHM_DIM = STATS_DIM;
	}

	if ((shm_des = shmget(IPC_PRIVATE, SHM_DIM, IPC_CREAT | 0600)) == -1) {
		perror("shmget");
		exit(1);
	}

	return shm_des;
}

int init_sem() {
	int sem_des;

	if ((sem_des = semget(IPC_PRIVATE, 3, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("shmget");
		exit(1);
	}

	if (semctl(sem_des, S_P, SETVAL, 1) == -1) {
		perror("semctl S_P");
		exit(1);
	}

	if (semctl(sem_des, S_AL, SETVAL, 0) == -1) {
		perror("semctl S_AL");
		exit(1);
	}

	if (semctl(sem_des, S_MZ, SETVAL, 0) == -1) {
		perror("semctl S_MZ");
		exit(1);
	}

	return sem_des;
}

void al_child(CHAR *data_letter, STATS *data_stats, int sem_des) {
	while (1) {
		WAIT(sem_des, S_AL);

		if (data_letter->done) {
			break;
		}

		data_stats->stats[data_letter->letter]++;
		SIGNAL(sem_des, S_P);
	}

	exit(0);
}

void mz_child(CHAR *data_letter, STATS *data_stats, int sem_des) {
	while (1) {
		WAIT(sem_des, S_MZ);

		if (data_letter->done) {
			break;
		}

		data_stats->stats[data_letter->letter]++;
		SIGNAL(sem_des, S_P);
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <file.txt>\n", argv[0]);
	}

	int fd;

	if ((fd = open(argv[1], O_RDONLY | 0600)) == -1) {
		perror("open");
		exit(1);
	}

	struct stat sb;

	if (fstat(fd, &sb) == -1) {
		perror("fstat");
		exit(1);
	}

	if (!S_ISREG(sb.st_mode)) {
		fprintf(stderr, "Il file %s non è regolare!\n", argv[1]);
		exit(1);
	}

	char *data;

	if ((data = (char *)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	close(fd);

	int shm_des[2];

	for (int i = 0; i < 2; i++) {
		shm_des[i] = init_shm(i);
	}

	int sem_des = init_sem();

	CHAR *data_letter;
	STATS *data_stats;

	if ((data_letter = (CHAR *)shmat(shm_des[0], NULL, 0)) == (CHAR *)-1) {
		perror("shmat CHAR");
		exit(1);
	}

	if ((data_stats = (STATS *)shmat(shm_des[1], NULL, 0)) == (STATS *)-1) {
		perror("shmat STATS");
		exit(1);
	}

	data_letter->done = 0;

	for (int i = 0; i < LETTERS_NUM; i++) {
		data_stats->stats[i] = 0;
	}
	// AL
	if (!fork()) {
		al_child(data_letter, data_stats, sem_des);
	}
	// MZ
	if (!fork()) {
		mz_child(data_letter, data_stats, sem_des);
	}

	for (int i = 0; i < sb.st_size; i++) {
		char letter = tolower(data[i]);
		WAIT(sem_des, S_P);
		data_letter->letter = letter - 'a';

		if (letter >= 'a' && letter <= 'l') {
			SIGNAL(sem_des, S_AL);
		}
		else if (letter >= 'm' && letter <= 'z') {
			SIGNAL(sem_des, S_MZ);
		}
		else {
			SIGNAL(sem_des, S_P);
		}
	}

	WAIT(sem_des, S_P);
	data_letter->done = 1;
	SIGNAL(sem_des, S_AL);
	SIGNAL(sem_des, S_MZ);

	for (int i = 0; i < LETTERS_NUM; i++) {
		printf("%c:%d ", i + 'a', data_stats->stats[i]);
	}

	printf("\n");

	for (int i = 0; i < 2; i++) {
		wait(NULL);
	}

	munmap(data, sb.st_size);

	for (int i = 0; i < 2; i++) {
		shmctl(shm_des[i], IPC_RMID, NULL);
	}

	semctl(sem_des, 0, IPC_RMID, 0);

	exit(0);
}
