#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define LINE_DIM 2048
#define QUERY_DIM sizeof(query)
#define ENTRY_DIM sizeof(entry)

typedef enum {S_IN, S_DB, S_OUT} S_TYPE;

typedef struct {
	char word[LINE_DIM];
	char id;
	char done;
} query;

typedef struct {
	char word[LINE_DIM];
	int value;
	char id;
	char done;
} entry;

typedef struct {
	int lines_n;
	long lines_s;
} global_stats;

int WAIT(int sem_des, int num_semaforo){
	struct sembuf ops[1] = {{num_semaforo, -1, 0}};
	return semop(sem_des, ops, 1);
}

int SIGNAL(int sem_des, int num_semaforo){
	struct sembuf ops[1] = {{num_semaforo, +1, 0}};
	return semop(sem_des, ops, 1);
}

int init_sem() {
	int sem_des;

	if ((sem_des = semget(IPC_PRIVATE, 3, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("semget");
		exit(1);
	}

	if (semctl(sem_des, S_IN, SETVAL, 1) == -1) {
		perror("semctl IN");
		exit(1);
	}

	if (semctl(sem_des, S_DB, SETVAL, 0) == -1) {
		perror("semctl DB");
		exit(1);
	}

	if (semctl(sem_des, S_OUT, SETVAL, 0) == -1) {
		perror("semctl OUT");
		exit(1);
	}

	return sem_des;
}

int init_shm(char id) {
	int shm_des;
	size_t SHM_DIM = id == 1 ? QUERY_DIM : ENTRY_DIM;

	if ((shm_des = shmget(IPC_PRIVATE, SHM_DIM, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		fprintf(stderr, "shmget %d", id);
		exit(1);
	}

	return shm_des;
}

void in_child(int sem_des, int shm_des, char id, char *argv) {
	FILE *fd;

	if ((fd = fopen(argv, "r")) == NULL) {
		fprintf(stderr, "fopen IN%d\n", id);
		exit(1);
	}

	query *query_in;

	if ((query_in = (query *)shmat(shm_des, NULL, 0)) == (query *)-1) {
		fprintf(stderr, "shmat IN%d\n", id);
		exit(1);
	}

	char line[LINE_DIM];
	int line_n = 0;

	while (fgets(line, LINE_DIM, fd)) {
		if (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		WAIT(sem_des, S_IN);
		strcpy(query_in->word, line);
		query_in->id = id;
		query_in->done = 0;
		SIGNAL(sem_des, S_DB);
		printf("IN%d: inviata query n.%d '%s'\n", id, ++line_n, query_in->word);
	}

	WAIT(sem_des, S_IN);
	query_in->done = 1;
	SIGNAL(sem_des, S_DB);
	fclose(fd);

	exit(0);
}

int get_lines_n(FILE *fd) {
	char line[LINE_DIM];
	int line_n = 0;

	while (fgets(line, LINE_DIM, fd)) {
		line_n++;
	}

	printf("DB: letti n. %d record da file\n", line_n);
	// Re-imposta il file pointer a inizio file
	if (fseek(fd, 0, SEEK_SET) == -1) {
		perror("fseek");
		exit(1);
	}

	return line_n;
}

void db_child(int sem_des, int shm_des1, int shm_des2, char *argv) {
	FILE *fd;

	if ((fd = fopen(argv, "r")) == NULL) {
		perror("fopen DB");
		exit(1);
	}

	char line[LINE_DIM];
	int lines_n = get_lines_n(fd);
	entry *db_entry = malloc(ENTRY_DIM * lines_n);
	// Popola il database
	for (int i = 0; i < lines_n; i++) {
		fgets(line, LINE_DIM, fd);

		if (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		char *word;
		char *value;

		if ((word = strtok(line, ":")) != NULL) {
			if ((value = strtok(NULL, ":")) != NULL) {
				strcpy(db_entry[i].word, word);
				db_entry[i].value = atoi(value);
			}
		}
	}

	query *query_in;
	entry *query_out;

	if ((query_in = (query *)shmat(shm_des1, NULL, 0)) == (query *)-1) {
		perror("shmat DB in\n");
		exit(1);
	}

	if ((query_out = (entry *)shmat(shm_des2, NULL, 0)) == (entry *)-1) {
		perror("shmat DB out\n");
		exit(1);
	}

	query_out->done = 0;
	char done_counter = 0;

	while (1) {
		WAIT(sem_des, S_DB);

		if (query_in->done) {
			done_counter++;

			if (done_counter == 2) {
				break;
			}
			else {
				SIGNAL(sem_des, S_IN);
				continue;
			}
		}

		char found = 0;

		for (int i = 0; i < lines_n; i++) {
			if (!strcmp(query_in->word, db_entry[i].word)) {
				strcpy(query_out->word, query_in->word);
				query_out->id = query_in->id;
				query_out->value = db_entry[i].value;
				printf("DB: query '%s' da IN%d trovata con valore %d\n", query_in->word, query_in->id, db_entry[i].value);
				SIGNAL(sem_des, S_OUT);
				found = 1;
				break;
			}
		}

		if (!found) {
			printf("DB: query '%s' da IN%d non trovata\n", query_in->word, query_in->id);
			SIGNAL(sem_des, S_IN);
		}
	}

	query_out->done = 1;
	SIGNAL(sem_des, S_OUT);
	free(db_entry);
	fclose(fd);

	exit(0);
}

void out_child(int sem_des, int shm_des) {
	entry *query_out;

	if ((query_out = (entry *)shmat(shm_des, NULL, 0)) == (entry *)-1) {
		perror("shmat DB out\n");
		exit(1);
	}

	global_stats stats_arr[2];

	for (int i = 0; i < 2; i++) {
		stats_arr[i].lines_n = 0;
		stats_arr[i].lines_s = 0;
	}

	while (1) {
		WAIT(sem_des, S_OUT);

		if (query_out->done) {
			break;
		}

		stats_arr[query_out->id - 1].lines_n++;
		stats_arr[query_out->id - 1].lines_s += query_out->value;
		SIGNAL(sem_des, S_IN);
	}

	for (int i = 0; i < 2; i++) {
		printf("OUT: ricevuti n.%d valori validi per IN%d con totale %ld\n", stats_arr[i].lines_n, i + 1, stats_arr[i].lines_s);
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc != 4) {
		fprintf(stderr, "%s <db-file> <query-file-1> <query-file-2>\n", argv[0]);
	}

	int sem_des = init_sem();
	int shm_des[2];

	for (int i = 0; i < 2; i++) {
		shm_des[i] = init_shm(i + 1);
	}
	// IN
	for (int i = 0; i < 2; i++) {
		if (!fork()) {
			in_child(sem_des, shm_des[0], i + 1, argv[i + 2]);
		}
	}
	// DB
	if (!fork()) {
		db_child(sem_des, shm_des[0], shm_des[1], argv[1]);
	}
	// OUT
	if (!fork()) {
		out_child(sem_des, shm_des[1]);
	}

	for (int i = 0; i < 4; i++) {
		wait(NULL);
	}

	for (int i = 0; i < 2; i++) {
		shmctl(shm_des[i], IPC_RMID, NULL);
	}

	semctl(sem_des, 0, IPC_RMID, 0);

	exit(0);
}
