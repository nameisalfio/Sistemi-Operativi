#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define MAX_LEN 64
#define SHM_DIM sizeof(shm)

typedef enum {S_R, S_P, S_W} S_TYPE;

typedef struct {
	char word[MAX_LEN];
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

	if ((shm_des = shmget(IPC_PRIVATE, SHM_DIM, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
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

	if (semctl(sem_des, S_R, SETVAL, 1)) {
		perror("semget S_R");
		exit(1);
	}

	if (semctl(sem_des, S_P, SETVAL, 0)) {
		perror("semget S_P");
		exit(1);
	}

	if (semctl(sem_des, S_W, SETVAL, 0)) {
		perror("semget S_W");
		exit(1);
	}

	return sem_des;
}

void r_child(int shm_des, int sem_des, char *argv) {
	FILE *fd;

	if ((fd = fopen(argv, "r")) == NULL) {
		perror("fopen R");
		exit(1);
	}

	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}

	data->done = 0;
	char word[MAX_LEN];

	while (fgets(word, MAX_LEN, fd)) {
		if (word[strlen(word) - 1] == '\n') {
			word[strlen(word) - 1] = '\0';
		}

		WAIT(sem_des, S_R);
		strcpy(data->word, word);
		SIGNAL(sem_des, S_P);
	}

	WAIT(sem_des, S_R);
	data->done = 1;
	SIGNAL(sem_des, S_P);
	SIGNAL(sem_des, S_W);
	fclose(fd);

	exit(0);
}

void w_child(int shm_des, int sem_des, char *argv) {
	FILE *fd;

	if ((fd = fopen(argv, "w")) == NULL) {
		perror("fopen W");
		exit(0);
	}

	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}

	while (1) {
		WAIT(sem_des, S_W);

		if (data->done) {
			break;
		}

		printf("%s\n", data->word);
		strcat(data->word, "\n");
		fputs(data->word, fd);
		SIGNAL(sem_des, S_R);
	}

	fclose(fd);

	exit(0);
}

void p_father(int shm_des, int sem_des) {
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}

	while (1) {
		WAIT(sem_des, S_P);

		if (data->done) {
			break;
		}

		char palindrome = 1;

		for (int i = 0, j = strlen(data->word) - 1; i < strlen(data->word) / 2; i++, j--) {
			if (data->word[i] != data->word[j]) {
				palindrome = 0;
				break;
			}
		}

		if (palindrome) {
			SIGNAL(sem_des, S_W);
		}
		else {
			SIGNAL(sem_des, S_R);
		}
	}
}

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "Usage: %s [input file] [output file]\n", argv[0]);
		exit(1);
	}

	int shm_des = init_shm();
	int sem_des = init_sem();

	// R
	if (!fork()) {
		r_child(shm_des, sem_des, argv[1]);
	}
	// W
	if (!fork()) {
		w_child(shm_des, sem_des, argv[2]);
	}

	p_father(shm_des, sem_des);

	for (int i = 0; i < 2; i++) {
		wait(NULL);
	}

	shmctl(shm_des, IPC_RMID, NULL);
	semctl(sem_des, 0, IPC_RMID, 0);

	exit(0);
}
