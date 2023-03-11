#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SHM_DIM sizeof(shm)

typedef enum {S_R, S_F, S_D} S_TYPE;

typedef struct {
	char path[PATH_MAX];
	char name[PATH_MAX];
	off_t bytes;
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
		perror("shmget");
		exit(1);
	}

	if (semctl(sem_des, S_R, SETVAL, 1) == -1) {
		perror("semctl S_R");
		exit(1);
	}

	if (semctl(sem_des, S_F, SETVAL, 0) == -1) {
		perror("semctl S_F");
		exit(1);
	}

	if (semctl(sem_des, S_D, SETVAL, 0) == -1) {
		perror("semctl S_D");
		exit(1);
	}

	return sem_des;
}

void r_child(int shm_des, int sem_des, int id, char *path) {
	DIR *d;

	if ((d = opendir(path)) == NULL) {
		fprintf(stderr, "opendir R%d %s\n", id, path);
		exit(1);
	}

	struct dirent *dirent;
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		fprintf(stderr, "shmat R%d\n", id);
		exit(1);
	}

	while (dirent = readdir(d)) {
		if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, "..")) {
			continue;
		}
		else if (dirent->d_type == DT_REG) {
			struct stat sb;
			WAIT(sem_des, S_R);
			sprintf(data->path, "%s/%s", path, dirent->d_name);
			strcpy(data->name, dirent->d_name);

			if (stat(data->path, &sb) == -1) {
				fprintf(stderr, "stat R%d %s\n", id, data->path);
				exit(1);
			}

			data->bytes = sb.st_size;
			data->done = 0;
			SIGNAL(sem_des, S_F);
		}
		else if (dirent->d_type == DT_DIR) {
			WAIT(sem_des, S_R);
			strcpy(data->path, path);
			strcpy(data->name, dirent->d_name);
			data->done = 0;
			SIGNAL(sem_des, S_D);
		}
	}

	WAIT(sem_des, S_R);
	data->done = 1;
	SIGNAL(sem_des, S_F);
	closedir(d);

	exit(0);
}

void f_child(int shm_des, int sem_des, int argc) {
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat File-Consumer");
		exit(1);
	}

	char done_counter = 0;

	while (1) {
		WAIT(sem_des, S_F);

		if (data->done) {
			done_counter++;
			SIGNAL(sem_des, S_D);

			if (done_counter == argc) {
				break;
			}

			continue;
		}

		printf("%s [file di %ld bytes]\n", data->path, data->bytes);
		SIGNAL(sem_des, S_R);
	}

	exit(0);
}

void d_child(int shm_des, int sem_des, int argc) {
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat Dir-Consumer");
		exit(1);
	}

	char done_counter = 0;

	while (1) {
		WAIT(sem_des, S_D);

		if (data->done) {
			done_counter++;

			if (done_counter == argc) {
				break;
			}

			SIGNAL(sem_des, S_R);
			continue;
		}

		printf("%s [%s]\n", data->path, data->name);
		SIGNAL(sem_des, S_R);
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <dir1> <dir2> <...>\n", argv[0]);
		exit(1);
	}

	int shm_des = init_shm();
	int sem_des = init_sem();
	// Reader
	for (int i = 0; i < argc - 1; i++) {
		if (!fork()) {
			r_child(shm_des, sem_des, i + 1, argv[i + 1]);
		}
	}
	// File-Consumer
	if (!fork()) {
		f_child(shm_des, sem_des, argc - 1);
	}
	// Dir-Consumer
	if (!fork()) {
		d_child(shm_des, sem_des, argc - 1);
	}

	for (int i = 0; i < argc + 1; i++) {
		wait(NULL);
	}

	shmctl(shm_des, IPC_RMID, NULL);
	semctl(sem_des, 0, IPC_RMID, 0);

	exit(0);
}
