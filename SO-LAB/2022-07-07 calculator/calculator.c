#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define LINE_DIM 6
#define SHM_DIM sizeof(shm)

typedef enum {S_MNG, S_ADD, S_MUL, S_SUB} S_TYPE;

typedef struct {
	long partial_result;
	long operand;
	char done;
} shm;

int WAIT(int sem_des, int num_semaforo){
	struct sembuf ops[1] = {{num_semaforo, -1, 0}};
	return semop(sem_des, ops, 1);
}

int SIGNAL(int sem_des, int num_semaforo){
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

	if ((sem_des = semget(IPC_PRIVATE, 4, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("semget");
		exit(1);
	}

	if (semctl(sem_des, S_MNG, SETVAL, 1) == -1) {
		perror("semctl MNG");
		exit(1);
	}

	if (semctl(sem_des, S_ADD, SETVAL, 0) == -1) {
		perror("semctl ADD");
		exit(1);
	}

	if (semctl(sem_des, S_MUL, SETVAL, 0) == -1) {
		perror("semctl MUL");
		exit(1);
	}

	if (semctl(sem_des, S_SUB, SETVAL, 0) == -1) {
		perror("semctl SUB");
		exit(1);
	}

	return sem_des;
}

void mng_child(int shm_des, int sem_des, char *argv) {
	FILE *fd;

	if ((fd = fopen(argv, "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat MNG");
		exit(1);
	}

	data->partial_result = 0;
	data->done = 0;
	char line[LINE_DIM];

	while (fgets(line, LINE_DIM, fd)) {
		if (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		char operator = line[0];
		strcpy(line, line + 1);
		WAIT(sem_des, S_MNG);
		data->operand = atol(line);
		printf("MNG: risultato intermedio: %ld; letto \"%c%ld\"\n", data->partial_result, operator, data->operand);

		if (operator == '+') {
			SIGNAL(sem_des, S_ADD);
		}
		else if (operator == '*') {
			SIGNAL(sem_des, S_MUL);
		}
		else {
			SIGNAL(sem_des, S_SUB);
		}
	}

	WAIT(sem_des, S_MNG);
	data->done = 1;
	SIGNAL(sem_des, S_ADD);
	SIGNAL(sem_des, S_MUL);
	SIGNAL(sem_des, S_SUB);
	printf("MNG: risultato finale: %ld\n", data->partial_result);
	fclose(fd);

	exit(0);
}

void add_child(int shm_des, int sem_des) {
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat ADD");
		exit(1);
	}

	while (1) {
		WAIT(sem_des, S_ADD);

		if (data->done) {
			break;
		}

		printf("ADD: %ld+%ld=%ld\n", data->partial_result, data->operand, data->partial_result + data->operand);
		data->partial_result += data->operand;
		SIGNAL(sem_des, S_MNG);
	}

	exit(0);
}

void mul_child(int shm_des, int sem_des) {
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat MUL");
		exit(1);
	}

	while (1) {
		WAIT(sem_des, S_MUL);

		if (data->done) {
			break;
		}

		printf("MUL: %ld*%ld=%ld\n", data->partial_result, data->operand, data->partial_result * data->operand);
		data->partial_result *= data->operand;
		SIGNAL(sem_des, S_MNG);
	}

	exit(0);
}

void sub_child(int shm_des, int sem_des) {
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat SUB");
		exit(1);
	}

	while (1) {
		WAIT(sem_des, S_SUB);

		if (data->done) {
			break;
		}

		printf("SUB: %ld-%ld=%ld\n", data->partial_result, data->operand, data->partial_result - data->operand);
		data->partial_result -= data->operand;
		SIGNAL(sem_des, S_MNG);
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <list.txt>\n", argv[0]);
	}

	int shm_des = init_shm();
	int sem_des = init_sem();
	// MNG
	if (!fork()) {
		mng_child(shm_des, sem_des, argv[1]);
	}
	// ADD
	if (!fork()) {
		add_child(shm_des, sem_des);
	}
	// MUL
	if (!fork()) {
		mul_child(shm_des, sem_des);
	}
	// SUB
	if (!fork()) {
		sub_child(shm_des, sem_des);
	}

	for (int i = 0; i < 4; i++) {
		wait(NULL);
	}

	shmctl(shm_des, IPC_RMID, NULL);
	semctl(sem_des, 0, IPC_RMID, 0);

	exit(0);
}
