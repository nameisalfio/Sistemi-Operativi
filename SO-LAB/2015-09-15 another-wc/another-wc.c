#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <ctype.h>

#define SHM_DIM sizeof(shm)

typedef enum {S_F, S_P} S_TYPE;

typedef struct {
	char byte;
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

	if (semctl(sem_des, S_F, SETVAL, 1) == -1) {
		perror("semctl S_F");
		exit(1);
	}

	if (semctl(sem_des, S_P, SETVAL, 0) == -1) {
		perror("semctl S_P");
		exit(1);
	}

	return sem_des;
}

void f_child(shm *data, int sem_des, char *argv, char input) {
	FILE *fd;

	if (input) {
		if ((fd = fopen(argv, "r")) == NULL) {
			perror("fopen");
			exit(1);
		}
	}
	else {
		fd = stdin;
	}

	data->done = 0;
	char byte;

	while ((byte = fgetc(fd)) != -1) {
		WAIT(sem_des, S_F);

		if (byte == '*') {
			break;
		}

		data->byte = byte;
		//printf("%c", data->byte);
		SIGNAL(sem_des, S_P);
	}

	if (input) {
		WAIT(sem_des, S_F);
	}

	data->done = 1;
	SIGNAL(sem_des, S_P);
	fclose(fd);

	exit(0);
}

int main(int argc, char **argv) {
	char input = 0;

	if (argc > 2) {
		fprintf(stderr, "Usage: %s [file di testo]\n", argv[0]);
		exit(1);
	}
	else if (argc == 2) {
		input = 1;
	}
	else {
		argv[1] = "";
	}

	int shm_des = init_shm();
	int sem_des = init_sem();
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}

	if (!fork()) {
		f_child(data, sem_des, argv[1], input);
	}

	int lines = 0;
	int words = 0;
	int characters = 0;
	char back_counter = 0;

	while (1) {
		WAIT(sem_des, S_P);

		if (data->done) {
			if (!input) {
				lines++;
				words++;
			}

			break;
		}

		if (data->byte == '\n') {
			if (input) {
				lines++;
				words++;
				characters++;
			}
			else {
				back_counter++;

				if (back_counter > 1) {
					lines++;
					words++;
					characters++;
					back_counter = 0;
				}
			}
		}
		else if (data->byte == ' ') {
			words++;
			characters++;

			if (!input) {
				back_counter = 0;
			}
		}
		else if ((tolower(data->byte) >= 'a' && tolower(data->byte) <= 'z') || (data->byte >= '0' && tolower(data->byte) <= '9')
				|| data->byte == '\t' || data->byte == '.' || data->byte == ',' || data->byte == ';' || data->byte == ':' || data->byte == '!'
				|| data->byte == '/' || data->byte == '-' || data->byte == '_' || data->byte == '(' || data->byte == ')') {
			characters++;

			if (!input) {
				back_counter = 0;
			}
		}

		SIGNAL(sem_des, S_F);
	}

	printf("  %d   %d %d %s\n", lines, words, characters, argv[1]);

	wait(NULL);
	shmctl(shm_des, IPC_RMID, NULL);
	semctl(sem_des, 0, IPC_RMID, 0);

	exit(0);
}
