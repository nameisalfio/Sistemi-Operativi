#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/sem.h>

#define WORD_DIM 1024
#define SHM_DIM sizeof(shm)

typedef enum {S_R, S_P, S_W} S_TYPE;

typedef struct {
	char word[WORD_DIM];
	char done;
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

	if ((shm_des = shmget(IPC_PRIVATE, SHM_DIM, IPC_CREAT | 0600)) == -1) {
		perror("shmget");
		exit(1);
	}

	return shm_des;
}

int init_sem() {
	int sem_des;

	if ((sem_des = semget(IPC_PRIVATE, 3, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("semget");
		exit(1);
	}

	if (semctl(sem_des, S_R, SETVAL, 1) == -1) {
		perror("semctl S_R");
		exit(1);
	}

	if (semctl(sem_des, S_P, SETVAL, 0) == -1) {
		perror("semctl S_P");
		exit(1);
	}

	if (semctl(sem_des, S_W, SETVAL, 0) == -1) {
		perror("semctl S_W");
		exit(1);
	}

	return sem_des;
}

void r_child(shm *data, int shm_des, int sem_des, char *argv) {
	int fd;

	if ((fd = open(argv, O_RDONLY)) == -1) {
		perror("open");
		exit(1);
	}

	struct stat sb;

	if (fstat(fd, &sb) == -1) {
		perror("fstat");
		exit(1);
	}

	if (!S_ISREG(sb.st_mode)) {
		fprintf(stderr, "%s is not a regular file!\n", argv);
		exit(1);
	}

	char *file;

	if ((file = (char *)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	close(fd);

	data->done = 0;
	char word[WORD_DIM];
	int j = 0;

	for (int i = 0; i < sb.st_size; i++) {
		if (file[i] != ' ') {
			word[j++] = file[i];
		}

		if (file[i] == '\n') {
			word[j - 1] = '\0';
			j = 0;
			WAIT(sem_des, S_R);
			strcpy(data->word, word);
			SIGNAL(sem_des, S_P);
		}
	}

	WAIT(sem_des, S_R);
	data->done = 1;
	SIGNAL(sem_des, S_P);
	munmap(file, sb.st_size);

	exit(0);
}

void w_child(shm *data, int sem_des, char argc, char *argv) {
	FILE *fd;

	if (argc == 3) {
		if ((fd = fopen(argv, "w")) == NULL) {
			perror("fopen");
			exit(1);
		}
	}

	while (1) {
		WAIT(sem_des, S_W);

		if (data->done) {
			break;
		}

		printf("%s\n", data->word);

		if (argc == 3) {
			strcat(data->word, "\n");
			fputs(data->word, fd);
		}

		SIGNAL(sem_des, S_R);
	}

	if (argc == 3) {
		fclose(fd);
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc < 2 || argc > 3) {
		fprintf(stderr, "Usage: %s <input file> [output file]\n", argv[0]);
		exit(1);
	}

	int shm_des[2];

	for (int i = 0; i < 2; i++) {
		shm_des[i] = init_shm();
	}

	int sem_des = init_sem();
	shm *data1;

	if ((data1 = (shm *)shmat(shm_des[0], NULL, 0)) == (shm *)-1) {
		perror("shmat P");
		exit(1);
	}
	// R
	if (!fork()) {
		r_child(data1, shm_des[0], sem_des, argv[1]);
	}

	shm *data2;

	if ((data2 = (shm *)shmat(shm_des[1], NULL, 0)) == (shm *)-1) {
		perror("shmat P");
		exit(1);
	}

	data2->done = 0;
	// W
	if (!fork()) {
		w_child(data2, sem_des, argc, argv[2]);
	}

	while (1) {
		WAIT(sem_des, S_P);

		if (data1->done) {
			break;
		}

		char palindrome = 1;

		for (int i = 0, j = strlen(data1->word) - 1; i < strlen(data1->word) / 2; i++, j--) {
			if (data1->word[i] != data1->word[j]) {
				palindrome = 0;
				break;
			}
		}

		if (palindrome) {
			strcpy(data2->word, data1->word);
			SIGNAL(sem_des, S_W);
		}
		else {
			SIGNAL(sem_des, S_R);
		}
	}

	data2->done = 1;
	SIGNAL(sem_des, S_W);

	for (int i = 0; i < 2; i++) {
		wait(NULL);
	}

	for (int i = 0; i < 2; i++) {
		shmctl(shm_des[i], IPC_RMID, NULL);
	}

	semctl(sem_des, 0, IPC_RMID, 0);

	exit(0);
}
