#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <ctype.h>
#include <sys/msg.h>

#define LINE_DIM 2048
#define LETTERS_NUM 26
#define SHM_DIM sizeof(shm)
#define MSG_DIM sizeof(msg) - sizeof(long)

typedef enum {S_L, S_P} S_TYPE;

typedef struct {
	long type;
	char id;
	int occurrence;
	char counter;
	char done;
} msg;

typedef struct {
	char line[LINE_DIM];
	char checked_letters[LETTERS_NUM];
	char counter;
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

int init_msgq() {
	int msgq_des;

	if ((msgq_des = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("msgget");
		exit(1);
	}

	return msgq_des;
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
		perror("shmget");
		exit(1);
	}

	if (semctl(sem_des, S_L, SETVAL, 0) == -1) {
		perror("semctl S_L");
		exit(1);
	}

	if (semctl(sem_des, S_P, SETVAL, 1) == -1) {
		perror("semctl S_P");
		exit(1);
	}

	return sem_des;
}

void l_child(int shm_des, int sem_des, int msgq_des, char id) {
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}

	msg mess_l;
	mess_l.type = 1;
	mess_l.id = id;
	mess_l.done = 0;

	while (1) {
		WAIT(sem_des, S_L);

		if (data->done) {
			break;
		}
		else if (data->checked_letters[id]) {
			if (data->counter != LETTERS_NUM) {
				SIGNAL(sem_des, S_L);
			}

			continue;
		}

		data->checked_letters[id] = 1;
		data->counter++;
		int occurrence = 0;

		for (int i = 0; i < strlen(data->line); i++) {
			if (id + 65 == toupper(data->line[i])) {
				occurrence++;
			}
		}

		mess_l.occurrence = occurrence;
		mess_l.counter = data->counter;

		if (msgsnd(msgq_des, &mess_l, MSG_DIM, 0) == -1) {
			fprintf(stderr, "msgsnd L-%d", id + 1);
			exit(1);
		}

		SIGNAL(sem_des, S_L);
	}

	mess_l.done = 1;

	if (msgsnd(msgq_des, &mess_l, MSG_DIM, 0) == -1) {
		fprintf(stderr, "msgsnd L-%c", id + 65);
		exit(1);
	}

	SIGNAL(sem_des, S_L);

	exit(0);
}

void s_child(int sem_des, int msgq_des) {
	int current_occurrences[LETTERS_NUM];
	int global_occurrences[LETTERS_NUM];

	for (int i = 0; i < LETTERS_NUM; i++) {
		global_occurrences[i] = 0;
	}

	msg mess_s;
	int line_n = 0;

	while (1) {
		if (msgrcv(msgq_des, &mess_s, MSG_DIM, 0, 0) == -1) {
			perror("msgrcv");
			exit(1);
		}

		if (mess_s.done) {
			break;
		}
		
		current_occurrences[mess_s.id] = mess_s.occurrence;
		global_occurrences[mess_s.id] += mess_s.occurrence;

		if (mess_s.counter == LETTERS_NUM) {
			char buff[500] = "";

			for (int i = 0; i < LETTERS_NUM; i++) {
				char s[7];

				sprintf(s, " %c=%d", i + 65, current_occurrences[i]);
				strcat(buff, s);
			}

			printf("[S] riga n.%d:%s\n\n", ++line_n, buff);

			SIGNAL(sem_des, S_P);
		}
	}

	
	char buff[500] = "";

	for (int i = 0; i < LETTERS_NUM; i++) {
		char s[7];

		sprintf(s, " %c=%d", i + 65, global_occurrences[i]);
		strcat(buff, s);
	}

	printf("[S] intero file:%s\n", buff);

	exit(0);
}

void p_father(int shm_des, int sem_des, FILE *fd) {
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}

	data->done = 0;
	char line[LINE_DIM];
	int line_n = 0;

	while (fgets(line, LINE_DIM, fd)) {
		if (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		WAIT(sem_des, S_P);
		strcpy(data->line, line);
		data->counter = 0;

		for (int i = 0; i < LETTERS_NUM; i++) {
			data->checked_letters[i] = 0;
		}

		SIGNAL(sem_des, S_L);

		printf("[P] riga n.%d: %s\n", ++line_n, data->line);
	}

	WAIT(sem_des, S_P);
	data->done = 1;
	SIGNAL(sem_des, S_L);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <text-file>\n", argv[0]);
			exit(1);
	}

	FILE *fd;

	if ((fd = fopen(argv[1], "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	int sem_des = init_sem();
	int msgq_des = init_msgq();
	// P
	if (!fork()) {
		s_child(sem_des, msgq_des);
	}

	int shm_des = init_shm();
	// L
	for (int i = 0; i < LETTERS_NUM; i++) {
		if (!fork()) {
			l_child(shm_des, sem_des, msgq_des, i);
		}
	}
	//P
	p_father(shm_des, sem_des, fd);
	fclose(fd);

	for (int i = 0; i < LETTERS_NUM + 1; i++) {
		wait(NULL);
	}

	shmctl(shm_des, IPC_RMID, NULL);
	semctl(sem_des, 0, IPC_RMID, 0);
	msgctl(msgq_des, IPC_RMID, NULL);

	exit(0);
}
