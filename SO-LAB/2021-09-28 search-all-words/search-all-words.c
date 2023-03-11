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

#define LINE_DIM 2048
#define SHM_DIM sizeof(shm)

typedef enum {S_R, S_W} S_TYPE;

typedef struct {
	char line[LINE_DIM];
	char checked[50];
	char counter;
	char print;
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

	if ((sem_des = semget(IPC_PRIVATE, 2, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("semget");
		exit(1);
	}

	if (semctl(sem_des, S_R, SETVAL, 1) == -1) {
		perror("semctl S_R");
		exit(1);
	}

	if (semctl(sem_des, S_W, SETVAL, 0) == -1) {
		perror("semctl S_W");
		exit(1);
	}

	return sem_des;
}

void w_child(int shm_des, int sem_des, int id, char *word, char argc) {
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		fprintf(stderr, "shmat W-%d", id + 1);
		exit(1);
	}

	while (1) {
		WAIT(sem_des, S_W);

		if (data->done) {
			break;
		}
		else if (data->checked[id]) {
			data->counter == argc ? SIGNAL(sem_des, S_R) : SIGNAL(sem_des, S_W);
			continue;
		}

		data->checked[id] = 1;
		data->counter++;

		if (strcasestr(data->line, word) == NULL) {
			data->print = 0;
			SIGNAL(sem_des, S_R);
		}
		else {
			SIGNAL(sem_des, S_W);
		}
	}

	SIGNAL(sem_des, S_W);

	exit(0);
}

void r_child(int shm_des, int sem_des, int pipefd, char *argv, int argc) {
	FILE *fd;

	if ((fd = fopen(argv, "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}

	data->done = 0;
	char line[LINE_DIM];

	while (fgets(line, LINE_DIM, fd)) {
		if (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		WAIT(sem_des, S_R);

		if (data->print) {
			dprintf(pipefd, "%s\n\n", data->line);
		}

		strcpy(data->line, line);
		data->counter = 0;
		data->print = 1;

		for (int i = 0; i < argc; i++) {
			data->checked[i] = 0;
		}

		SIGNAL(sem_des, S_W);
	}

	WAIT(sem_des, S_R);
	data->done = 1;
	SIGNAL(sem_des, S_W);
	dprintf(pipefd, "-1\n");
	fclose(fd);
	close(pipefd);

	exit(0);
}

void o_child(int pipefd) {
	FILE *pfd;

	if ((pfd = fdopen(pipefd, "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	char line[LINE_DIM];

	while (1) {
		fgets(line, LINE_DIM, pfd);

		if (!strcmp(line, "-1\n")) {
			break;
		}

		printf("%s", line);
	}

	fclose(pfd);
	close(pipefd);

	exit(0);
}

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <text-file> <word-1> [<word-2>] ... [<word-n>]\n", argv[0]);
		exit(1);
	}

	int shm_des = init_shm();
	int sem_des = init_sem();
	int pipefd[2];

	if (pipe(pipefd) == -1) {
		perror("pipe");
		exit(1);
	}
	// W
	for (int i = 0; i < argc - 2; i++) {
		if (!fork()) {
			close(pipefd[0]);
			close(pipefd[1]);
			w_child(shm_des, sem_des, i, argv[i + 2], argc - 2);
		}
	}
	// R
	if (!fork()) {
		close(pipefd[0]);
		r_child(shm_des, sem_des, pipefd[1], argv[1], argc - 2);
	}
	// O
	if (!fork()) {
		close(pipefd[1]);
		o_child(pipefd[0]);
	}

	for (int i = 0; i < argc; i++) {
		wait(NULL);
	}

	shmctl(shm_des, IPC_RMID, NULL);
	semctl(sem_des, 0, IPC_RMID, 0);

	for (int i = 0; i < 1; i++) {
		close(pipefd[i]);
	}

	exit(0);
}
