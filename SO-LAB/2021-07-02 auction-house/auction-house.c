#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <string.h>
#include <time.h>

#define LINE_DIM 2048
#define SHM_DIM sizeof(shm)
#define MAX_BIDDERS 100

typedef enum {S_B, S_J} S_TYPE;

typedef struct {
	char id;
	char description[LINE_DIM];
	int min;
	int max;
	int offer;
	int auction_n;
	char checked[MAX_BIDDERS];
	int counter;
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

	if ((sem_des = semget(IPC_PRIVATE, 2, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("semget");
		exit(1);
	}

	if (semctl(sem_des, S_B, SETVAL, 0) == -1) {
		perror("semctl S_B");
		exit(1);
	}

	if (semctl(sem_des, S_J, SETVAL, 1) == -1) {
		perror("semctl S_J");
		exit(1);
	}

	return sem_des;
}

void b_child(shm *data, int sem_des, int id, int num_bidders) {
	srand(time(NULL) + id);

	while (1) {
		WAIT(sem_des, S_B);

		if (data->done) {
			break;
		}
		else if (data->checked[id]) {
			if	(data->counter != num_bidders) {
				SIGNAL(sem_des, S_B);
			}
			else {
				SIGNAL(sem_des, S_J);
			}

			continue;
		}

		data->id = id;
		data->offer = rand() % data->max + 1;
		data->checked[id] = 1;
		data->counter++;
		printf("B%d: invio offerta di %d EUR per asta n.%d\n", id + 1, data->offer, data->auction_n);
		SIGNAL(sem_des, S_J);
	}

	SIGNAL(sem_des, S_B);

	exit(0);
}

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <auction-file> <num-bidders>\n", argv[0]);
		exit(1);
	}

	int num_bidders = atoi(argv[2]);
	int shm_des = init_shm();
	int sem_des = init_sem();

	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}
	// B
	for (int i = 0; i < num_bidders; i++) {
		if (!fork()) {
			b_child(data, sem_des, i, num_bidders);
		}
	}

	FILE *fd;

	if ((fd = fopen(argv[1], "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	char line[LINE_DIM];
	data->auction_n = 0;
	data->done = 0;
	int bids[MAX_BIDDERS];
	int arrive_time[MAX_BIDDERS];
	int results[3] = {0, 0, 0};

	while (fgets(line, LINE_DIM, fd)) {
		if (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		WAIT(sem_des, S_J);
		char *description;
		char *min;
		char *max;

		if ((description = strtok(line, ",")) != NULL) {
			if ((min = strtok(NULL, ",")) != NULL) {
				if ((max = strtok(NULL, ",")) != NULL) {
					printf("J: lancio asta n.%d per %s con offerta minima di %s EUR e massima di %s EUR\n", ++data->auction_n, description, min, max);
				}
			}
		}

		for (int i = 0; i < MAX_BIDDERS; i++) {
			data->checked[i] = 0;
			bids[i] = 0;
			arrive_time[i] = 0;
		}

		strcpy(data->description, description);
		data->min = atoi(min);
		data->max = atoi(max);
		data->counter = 0;

		SIGNAL(sem_des, S_B);

		for (int i = 0; i < num_bidders; i++) {
			WAIT(sem_des, S_J);
			bids[data->id] = data->offer;
			arrive_time[data->id] = data->counter;
			printf("J: ricevuta offerta da B%d\n", data->id + 1);
			SIGNAL(sem_des, S_B);
		}

		char valid_bids = 0;
		char max_bid = 0;

		for (int i = 0; i < num_bidders; i++) {
			if (bids[i] >= data->min) {
				valid_bids++;
			}

			if (bids[max_bid] < bids[i]) {
				max_bid = i;
			}
		}

		int max_bid_value = bids[max_bid];

		for (int i = 0; i < num_bidders; i++) {
			if (max_bid_value == bids[i] && arrive_time[max_bid] > arrive_time[i]) {
				max_bid = i;
			}
		}

		if (!valid_bids) {
			results[0]++;
			printf("J: l'asta n.%d per %s si è conclusa senza alcuna offerta valida pertanto l'oggetto non risulta assegnato\n\n", data->auction_n, data->description);
		}
		else {
			results[1]++;
			results[2] += bids[max_bid];
			printf("J: l'asta n.%d per %s si è conclusa con %d offerte valide su %d; il vincitore è B%d che si aggiudica l'oggetto per %d EUR\n\n", data->auction_n, data->description, valid_bids, num_bidders, max_bid + 1, bids[max_bid]);
		}
	}

	WAIT(sem_des, S_J);
	data->done = 1;
	SIGNAL(sem_des, S_B);

	printf("J: sono state svolte %d aste di cui %d andate assegnate e %d andate a vuoto; il totale raccolto è di %d EUR\n", data->auction_n, results[1], results[0], results[2]);

	for (int i = 0; i < num_bidders; i++) {
		wait(NULL);
	}

	shmctl(shm_des, IPC_RMID, NULL);
	semctl(sem_des, 0, IPC_RMID, 0);

	exit(0);
}
