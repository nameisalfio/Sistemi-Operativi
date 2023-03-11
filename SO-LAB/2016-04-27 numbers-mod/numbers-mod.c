#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define NUM_DIM 1024
#define BUFF_DIM 10
#define SHM_DIM sizeof(shm) * BUFF_DIM

typedef enum {S_MUTEX, S_EMPTY, S_FULL_NUM, S_FULL_MOD} S_TYPE;

typedef enum {T_EMPTY, T_NUM, T_MOD, T_DONE} T_TYPE;

typedef struct {
	long number;
	char type;
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

	if ((sem_des = semget(IPC_PRIVATE, 4, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("semget");
		exit(1);
	}

	if (semctl(sem_des, S_MUTEX, SETVAL, 1) == -1) {
		perror("semctl S_MUTEX");
		exit(1);
	}

	if (semctl(sem_des, S_EMPTY, SETVAL, BUFF_DIM) == -1) {
		perror("semctl S_EMPTY");
		exit(1);
	}

	if (semctl(sem_des, S_FULL_NUM, SETVAL, 0) == -1) {
		perror("semctl S_FULL_NUM");
		exit(1);
	}

	if (semctl(sem_des, S_FULL_MOD, SETVAL, 0) == -1) {
		perror("semctl S_FULL_MOD");
		exit(1);
	}

	return sem_des;
}

void m_child(shm *data, int sem_des, long mod) {
	char done = 0;

	while (1) {
		WAIT(sem_des, S_FULL_NUM);
		WAIT(sem_des, S_MUTEX);

		for (int i = 0; i < BUFF_DIM; i++) {
			if (data[i].type == T_DONE) {
				done = 1;
			}
			else if (data[i].type == T_NUM) {
				data[i].number %= mod;
				data[i].type = T_MOD;
				break; // non necessario
			}
		}

		SIGNAL(sem_des, S_MUTEX);
		SIGNAL(sem_des, S_FULL_MOD);

		if (done) {
			break;
		}
	}

	exit(0);
}

void o_child(shm *data, int sem_des) {
	char done = 0;

	while (1) {
		WAIT(sem_des, S_FULL_MOD);
		WAIT(sem_des, S_MUTEX);

		for (int i = 0; i < BUFF_DIM; i++) {
			if (data[i].type == T_DONE) {
				done = 1;
			}
			else if (data[i].type == T_MOD) {
				printf("%ld\n", data[i].number);
				data[i].type = T_EMPTY;
				break; // non necessario
			}
		}

		SIGNAL(sem_des, S_MUTEX);
		SIGNAL(sem_des, S_EMPTY);

		if (done) {
			break;
		}
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <input file> <modulus number>\n", argv[0]);
		exit(1);
	}

	int shm_des = init_shm();
	int sem_des = init_sem();
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}

	for (int i = 0; i < BUFF_DIM; i++) {
		data[i].type = T_EMPTY;
	}
	// Mod
	if (!fork()) {
		m_child(data, sem_des, atol(argv[2]));
	}
	// Out
	if (!fork()) {
		o_child(data, sem_des);
	}

	FILE *fd;

	if ((fd = fopen(argv[1], "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	char number[NUM_DIM];

	while(fgets(number, NUM_DIM, fd)) {
		if (number[strlen(number) - 1] == '\n') {
			number[strlen(number) - 1] = '\0';
		}

		WAIT(sem_des, S_EMPTY);
		WAIT(sem_des, S_MUTEX);

		for (int i = 0; i < BUFF_DIM; i++) {
			if (data[i].type == T_EMPTY) {
				data[i].number = atol(number);
				data[i].type = T_NUM;
				break;
			}
		}

		SIGNAL(sem_des, S_MUTEX);
		SIGNAL(sem_des, S_FULL_NUM);
	}

	WAIT(sem_des, S_EMPTY);
	WAIT(sem_des, S_MUTEX);

	for (int i = 0; i < BUFF_DIM; i++) {
		if (data[i].type == T_EMPTY) {
			data[i].type = T_DONE;
			break;
		}
	}

	SIGNAL(sem_des, S_MUTEX);
	SIGNAL(sem_des, S_FULL_NUM);

	for (int i = 0; i < 2; i++) {
		wait(NULL);
	}

	shmctl(shm_des, IPC_RMID, NULL);
	semctl(sem_des, 0, IPC_RMID, 0);
	fclose(fd);

	exit(0);
}
