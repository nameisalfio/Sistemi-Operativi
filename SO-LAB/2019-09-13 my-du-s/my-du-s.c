#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>

#define SHM_DIM sizeof(shm)

typedef enum {S_SC, S_ST, S_P} S_TYPE;

typedef struct {
	char id;
	char path[PATH_MAX];
	int st_blocks;
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

	if (semctl(sem_des, S_SC, SETVAL, 1) == -1) {
		perror("semctl S_SC");
		exit(1);
	}

	if (semctl(sem_des, S_ST, SETVAL, 0) == -1) {
		perror("semctl S_ST");
		exit(1);
	}

	if (semctl(sem_des, S_P, SETVAL, 0) == -1) {
		perror("semctl S_P");
		exit(1);
	}

	return sem_des;
}

void scanner_child(shm *data, int sem_des, int id, char  *path, char base) {
	DIR *d;

	if ((d = opendir(path)) == NULL) {
		fprintf(stderr, "openrdir Stater%d\n", id);
		exit(1);
	}

	struct dirent *dirent;

	while (dirent = readdir(d)) {
		if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, "..")) {
			continue;
		}
		else if (dirent->d_type == DT_REG) {
			WAIT(sem_des, S_SC);
			data->id = id;
			sprintf(data->path, "%s/%s", path, dirent->d_name);
			data->done = 0;
			SIGNAL(sem_des, S_ST);
		}
		else if (dirent->d_type == DT_DIR) {
			char tmp_dir[PATH_MAX];
			sprintf(tmp_dir, "%s/%s", path, dirent->d_name);
			scanner_child(data, sem_des, id, tmp_dir, 0);
		}
	}

	closedir(d);

	if (base) {
		WAIT(sem_des, S_SC);
		data->done = 1;
		SIGNAL(sem_des, S_ST);

		exit(0);
	}
}

void stater_child(shm *data, int sem_des, int argc) {
	struct stat sb;
	char done_counter = 0;

	while (1) {
		WAIT(sem_des, S_ST);

		if (data->done) {
			done_counter++;

			if (done_counter == argc) {
				SIGNAL(sem_des, S_P);
				break;
			}

			SIGNAL(sem_des, S_SC);
			continue;
		}

		if (stat(data->path, &sb) == -1) {
			perror("stat");
			exit(1);
		}

		data->st_blocks = sb.st_blocks;
		SIGNAL(sem_des, S_P);
	}

	SIGNAL(sem_des, S_P);

	exit(0);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		argc = 2;
		argv[1] = ".";
	}

	int shm_des = init_shm();
	int sem_des = init_sem();
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}
	// Scanner
	for (int i = 0; i < argc - 1; i++) {
		if (!fork()) {
			scanner_child(data, sem_des, i + 1, argv[i + 1], 1);
		}
	}
	// Stater
	if (!fork()) {
		stater_child(data, sem_des, argc - 1);
	}

	int blocks[argc - 1];

	for (int i = 0; i < argc - 1; i++) {
		blocks[i] = 0;
	}

	while (1) {
		WAIT(sem_des, S_P);

		if (data->done) {
			break;
		}

		blocks[data->id - 1] += data->st_blocks;
		SIGNAL(sem_des, S_SC);
	}

	for (int i = 0; i < argc - 1; i++) {
		printf("%d\t%s\n", blocks[i], argv[i + 1]);
	}

	for (int i = 0; i < argc; i++) {
		wait(NULL);
	}

	shmctl(shm_des, IPC_RMID, NULL);
	semctl(sem_des, 0, IPC_RMID, 0);

	exit(0);
}
