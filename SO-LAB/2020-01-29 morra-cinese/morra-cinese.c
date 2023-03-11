#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <time.h>

#define SHM_DIM sizeof(shm)

typedef enum {S_P1, S_P2, S_G, S_T} S_TYPE;
typedef enum {S, C, F} M_TYPE;

typedef struct {
	char mossa_p1;
	char mossa_p2;
	char vincitore;
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

	if ((sem_des = semget(IPC_PRIVATE, 4, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("semget");
		exit(1);
	}

	if (semctl(sem_des, S_P1, SETVAL, 1) == -1) {
		perror("semget S_P1");
		exit(1);
	}

	if (semctl(sem_des, S_P2, SETVAL, 1) == -1) {
		perror("semget S_P2");
		exit(1);
	}

	if (semctl(sem_des, S_G, SETVAL, 0) == -1) {
		perror("semget S_G");
		exit(1);
	}

	if (semctl(sem_des, S_T, SETVAL, 0) == -1) {
		perror("semget S_T");
		exit(1);
	}

	return sem_des;
}

void p_child(int shm_des, int sem_des, int id) {
	srand(time(NULL) + id);
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}

	char *move[3] = {"carta", "forbice", "sasso"};

	while (1) {
		if (id == S_P1) {
			WAIT(sem_des, S_P1);

			if (data->done) {
				break;
			}

			data->mossa_p1 = rand() % 3;
			printf("P1: mossa '%s'\n", move[data->mossa_p1]);
		}
		else {
			WAIT(sem_des, S_P2);

			if (data->done) {
				break;
			}

			data->mossa_p2 = rand() % 3;
			printf("P2: mossa '%s'\n", move[data->mossa_p2]);
		}

		SIGNAL(sem_des, S_G);
	}

	SIGNAL(sem_des, S_G);

	exit(0);
}

void g_child(int shm_des, int sem_des) {
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}

	int matches = 1;

	while (1) {
		WAIT(sem_des, S_G);
		WAIT(sem_des, S_G);

		if (data->done) {
			break;
		}

		if (data->mossa_p1 == data->mossa_p2) {
			printf("G: partita n.%d patta e quindi da ripetere\n", matches);
			SIGNAL(sem_des, S_P1);
			SIGNAL(sem_des, S_P2);
			continue;
		}
		else if ((data->mossa_p1 == S && data->mossa_p2 == F) || (data->mossa_p1 == F && data->mossa_p2 == C) || (data->mossa_p1 == C && data->mossa_p2 == S)) {
			data->vincitore = 1;
			printf("G: partita n.%d vita da P1\n", matches);
		}
		else if ((data->mossa_p1 == F && data->mossa_p2 == S) || (data->mossa_p1 == C && data->mossa_p2 == F) || (data->mossa_p1 == S && data->mossa_p2 == C)) {
			data->vincitore = 2;
			printf("G: partita n.%d vita da P2\n", matches);
		}

		matches++;
		SIGNAL(sem_des, S_T);
	}

	exit(0);
}

void t_child(int shm_des, int sem_des, int matches) {
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}

	data->done = 0;

	int match = 1;
	int winners[2] = {0, 0};

	while (match <= matches) {
		WAIT(sem_des, S_T);
		winners[data->vincitore - 1]++;

		if (match == matches) {
			printf("T: classifica finale: P1=%d P2=%d\n", winners[0], winners[1]);
			data->done = 1;
		}
		else {
			printf("T: classifica temporanea: P1=%d P2=%d\n", winners[0], winners[1]);
		}

		match++;
		SIGNAL(sem_des, S_P1);
		SIGNAL(sem_des, S_P2);
	}

	if (winners[0] > winners[1]) {
		printf("T: vincitore del torneo: P1\n");
	}
	else if (winners[0] < winners[1]) {
		printf("T: vincitore del torneo: P2\n");
	}
	else {
		printf("T: nessun vincitore del torneo\n");
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <numero-partite>\n", argv[0]);
		exit(1);
	}

	int matches = atoi(argv[1]);
	int shm_des = init_shm();
	int sem_des = init_sem();
	// P
	for (int i = 0; i < 2; i++) {
		if (!fork()) {
			p_child(shm_des, sem_des, i);
		}
	}
	// G
	if (!fork()) {
		g_child(shm_des, sem_des);
	}
	// T
	if (!fork()) {
		t_child(shm_des, sem_des, matches);
	}

	for (int i = 0; i < 4; i++) {
		wait(NULL);
	}

	shmctl(shm_des, IPC_RMID, NULL);
	semctl(sem_des, 0, IPC_RMID, 0);

	exit(0);
}
