#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/limits.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/msg.h>

#define SHM_DIM sizeof(shm)
#define MSG_DIM sizeof(msg) - sizeof(long)

typedef enum {S_SC, S_ST} S_TYPE;

typedef struct {
	char path[PATH_MAX];
	char id;
	char done;
} shm;

typedef struct {
	long type;
	char value;
	char id;
	char done;
} msg;

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

	if (semctl(sem_des, S_SC, SETVAL, 1) == -1) {
		perror("semctl S_SC");
		exit(1);
	}

	if (semctl(sem_des, S_ST, SETVAL, 0) == -1) {
		perror("semctl S_ST");
		exit(1);
	}

	return sem_des;
}

int init_msgq() {
	int msgq_des;

	if ((msgq_des = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("msgget");
		exit(1);
	}

	return msgq_des;
}

void scanner_child(int shm_des, int sem_des, int id, char *path, char base) {
	DIR *d;

	if ((d = opendir(path)) == NULL) {
		fprintf(stderr, "opendir Scanner%d\n", id);
		exit(1);
	}

	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		fprintf(stderr, "shmat Scanner%d\n", id);
		exit(1);
	}

	struct dirent *dirent;

	while (dirent = readdir(d)) {
		if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, "..")) {
			continue;
		}
		else if (dirent->d_type == DT_REG) {
			WAIT(sem_des, S_SC);
			sprintf(data->path, "%s/%s", path, dirent->d_name);
			data->id = id;
			data->done = 0;
			SIGNAL(sem_des, S_ST);
		}
		else if (dirent->d_type == DT_DIR) {
			char tmp_dir[PATH_MAX];
			sprintf(tmp_dir, "%s/%s", path, dirent->d_name);
			scanner_child(shm_des, sem_des, id, tmp_dir, 0);
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

void stater_child(int shm_des, int sem_des, int msgq_des, int argc) {
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat Stater");
		exit(1);
	}

	struct stat sb;
	msg mess_st;
	mess_st.type = 1;
	mess_st.done = 0;
	char done_counter = 0;

	while (1) {
		WAIT(sem_des, S_ST);

		if (data->done) {
			done_counter++;

			if (done_counter == argc) {
				break;
			}

			SIGNAL(sem_des, S_SC);
			continue;
		}

		if (stat(data->path, &sb) == -1) {
			fprintf(stderr, "stat %s\n", data->path);
			exit(1);
		}

		mess_st.value = sb.st_blocks;
		mess_st.id = data->id;

		if (msgsnd(msgq_des, &mess_st, MSG_DIM, 0) == -1) {
			perror("msgsnd");
			exit(1);
		}

		SIGNAL(sem_des, S_SC);
	}

	mess_st.done = 1;

	if (msgsnd(msgq_des, &mess_st, MSG_DIM, 0) == -1) {
		perror("msgsnd");
		exit(1);
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		argc = 2;
		argv[1] = ".";
	}

	int shm_des = init_shm();
	int sem_des = init_sem();
	// Scanner
	for (int i = 0; i < argc - 1; i++) {
		if (!fork()) {
			scanner_child(shm_des, sem_des, i + 1, argv[i + 1], 1);
		}
	}

	int msgq_des = init_msgq();
	// Stater
	if (!fork()) {
		stater_child(shm_des, sem_des, msgq_des, argc - 1);
	}

	int blocks[argc - 1];

	for (int i = 0; i < argc - 1; i++) {
		blocks[i] = 0;
	}

	msg mess_p;

	while (1) {
		if (msgrcv(msgq_des, &mess_p, MSG_DIM, 0, 0) == -1) {
			perror("msgrcv");
			exit(1);
		}

		if (mess_p.done) {
			break;
		}

		blocks[mess_p.id - 1] += mess_p.value;
	}

	for (int i = 0; i < argc - 1; i++) {
		printf("%d\t%s\n", blocks[i], argv[i + 1]);
	}

	for (int i = 0; i < argc; i++) {
		wait(NULL);
	}

	shmctl(shm_des, IPC_RMID, NULL);
	semctl(sem_des, 0, IPC_RMID, 0);
	msgctl(msgq_des, IPC_RMID, NULL);

	exit(0);
}
