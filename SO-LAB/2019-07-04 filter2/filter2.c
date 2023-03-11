#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <ctype.h>

#define MAX_LEN 1024
#define SHM_DIM sizeof(shm)
#define S_P 0

typedef struct {
	char line[MAX_LEN];
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

int init_sem(int argc) {
	int sem_des;

	if ((sem_des = semget(IPC_PRIVATE, argc, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("semget");
		exit(1);
	}

	if (semctl(sem_des, S_P, SETVAL, 1) == -1) {
		perror("semctl S_P");
		exit(1);
	}

	for (int i = 0; i < argc - 1; i++) {
		if (semctl(sem_des, i + 1, SETVAL, 0) == -1) {
			fprintf(stderr, "semctl S_F%d\n", i + 1);
			exit(1);
		}
	}

	return sem_des;
}

void f_child(shm *data, int sem_des, int id, char *word, int argc) {
	char filter = word[0], *word1, *word2;
	strcpy(word, word + 1);

	if (filter == '%') {
		word1 = strtok(word, "|");
		word2 = strtok(NULL, "|");
	}

	while (1) {
		WAIT(sem_des, id);
		
		if (data->done) {
			break;
		}

		if (filter == '^') {
			char *substr;

			while ((substr = strstr(data->line, word)) != NULL) {
				for (int i = 0; i < strlen(word); i++) {
					substr[i] = toupper(word[i]);
				}
			}
		}
		else if (filter == '_') {
			char *substr;

			while ((substr = strstr(data->line, word)) != NULL) {
				for (int i = 0; i < strlen(word); i++) {
					substr[i] = tolower(word[i]);
				}
			}
		}
		else if (filter == '%') {
			char *substr, next_substr[MAX_LEN];

			while ((substr = strcasestr(data->line, word1)) != NULL) {
				strcpy(next_substr, substr + strlen(word1));
				strcpy(substr, "");
				strcpy(substr, word2);
				strcat(substr, next_substr);
			}
		}

		if (id != argc) {
			SIGNAL(sem_des, id + 1);
		}
		else {
			printf("%s\n", data->line);	
			SIGNAL(sem_des, S_P);
		}
	}

	SIGNAL(sem_des, id + 1);

	exit(0);
}

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <file.txt> <filter-1> [filter-2] [...]\n", argv[0]);
		exit(1);
	}

	int shm_des = init_shm();
	int sem_des = init_sem(argc - 1);
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}
	// F
	for (int i = 0; i < argc - 2; i++) {
		if (!fork()) {
			f_child(data, sem_des, i + 1, argv[i + 2], argc - 2);
		}
	}

	FILE *fd;

	if ((fd = fopen(argv[1], "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	data->done = 0;
	char line[MAX_LEN];

	while (fgets(line, MAX_LEN, fd)) {
		if (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		WAIT(sem_des, S_P);
		strcpy(data->line, line);
		SIGNAL(sem_des, 1);
	}

	WAIT(sem_des, S_P);
	data->done = 1;
	SIGNAL(sem_des, 1);

	for (int i = 0; i < argc - 2; i++) {
		wait(NULL);
	}

	fclose(fd);
	shmctl(shm_des, IPC_RMID, NULL);

	for (int i = 0; i < argc - 1; i++) {
		shmctl(sem_des, i, IPC_RMID);
	}

	semctl(sem_des, 0, IPC_RMID, 0);

	exit(0);
}
