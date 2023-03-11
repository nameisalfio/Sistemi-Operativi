#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define MAX_WORD_LEN 50
#define SHM_DIM sizeof(shm)

typedef enum {S_S, S_C, S_P} S_TYPE;

typedef struct {
	char word1[MAX_WORD_LEN];
	char word2[MAX_WORD_LEN];
	int result;
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
		perror("sheget");
		exit(1);
	}

	if (semctl(sem_des, S_S, SETVAL, 0) == -1) {
		perror("shmctl S_S");
		exit(1);
	}

	if (semctl(sem_des, S_C, SETVAL, 0) == -1) {
		perror("shmctl S_C");
		exit(1);
	}

	if (semctl(sem_des, S_P, SETVAL, 0) == -1) {
		perror("shmctl S_P");
		exit(1);
	}

	return sem_des;
}

void s_child(shm *data, int sem_des, char *argv) {
	FILE *fd;

	if ((fd = fopen(argv, "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	char word[MAX_WORD_LEN];
	int words_n = 0;

	while (fgets(word, MAX_WORD_LEN, fd)) {
		words_n++;
	}

	if (fseek(fd, 0, SEEK_SET) == -1) {
		perror("fseek");
		exit(1);
	}

	char **words = malloc(sizeof (char *) * words_n);

	for (int i = 0; i < words_n; i++) {
		fgets(word, MAX_WORD_LEN, fd);

		if (word[strlen(word) - 1] == '\n') {
			word[strlen(word) - 1] = '\0';
		}

		words[i] = malloc(sizeof(char) * MAX_WORD_LEN);
		strcpy(words[i], word);
	}

	fclose(fd);
	data->done = 0;

	for (int i = 0; i < words_n - 1; i++) {
		int min = i;

		for (int j = i + 1; j < words_n; j++) {
			strcpy(data->word1, words[min]);
			strcpy(data->word2, words[j]);
			SIGNAL(sem_des, S_C);
			WAIT(sem_des, S_S);

			if (data->result > 0) {
				min = j;
			}
		}

		if (min != i) {
			char tmp[MAX_WORD_LEN];
			strcpy(tmp, words[i]);
			strcpy(words[i], words[min]);
			strcpy(words[min], tmp);
		}
	}

	data->done = 1;
	SIGNAL(sem_des, S_C);

	for (int i = 0; i < words_n; i++) {
		WAIT(sem_des, S_S);
		data->done = 0;
		strcpy(data->word1, words[i]);
		SIGNAL(sem_des, S_P);
	}

	WAIT(sem_des, S_S);
	data->done = 1;
	SIGNAL(sem_des, S_P);

	for (int i = 0; i < words_n; i++) {
		free(words[i]);
	}

	free(words);

	exit(0);
}

void c_child(shm *data, int sem_des) {
	while (1) {
		WAIT(sem_des, S_C);

		if (data->done) {
			SIGNAL(sem_des, S_S);
			break;
		}

		data->result = strcasecmp(data->word1, data->word2);
		SIGNAL(sem_des, S_S);
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <file>\n", argv[0]);
		exit(1);
	}

	int shm_des = init_shm();
	int sem_des = init_sem();
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}
	// Sorter
	if (!fork()) {
		s_child(data, sem_des, argv[1]);
	}
	// Comparer
	if (!fork()) {
		c_child(data, sem_des);
	}

	char word[MAX_WORD_LEN];

	while (1) {
		WAIT(sem_des, S_P);

		if (data->done) {
			break;
		}

		printf("%s\n", data->word1);
		SIGNAL(sem_des, S_S);
	}

	for (int i = 0; i < 2; i++) {
		wait(NULL);
	}

	shmctl(shm_des, IPC_RMID, NULL);
	semctl(sem_des, 0, IPC_RMID, 0);

	exit(0);
}
