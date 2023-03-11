#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/sem.h>
#include <ctype.h>
#include <sys/msg.h>

#define LINE_DIM 1024
#define SHM_DIM sizeof(shm)
#define MSG_DIM sizeof(msg) - sizeof(long)
#define LETTERS_NUM 26

typedef enum {S_R, S_C} S_TYPE;

typedef struct {
	char word[LINE_DIM];
	char id;
	int line_n;
	char done;
} shm;

typedef struct {
	long type;
	int stats[LETTERS_NUM];
	char id;
	int line_n;
	char done;
} msg;

typedef struct {
	int stats[LETTERS_NUM];
	int lines_n;
} global_stats;

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

	if (semctl(sem_des, S_R, SETVAL, 1) == -1) {
		perror("semclt R");
		exit(1);
	}

	if (semctl(sem_des, S_C, SETVAL, 0) == -1) {
		perror("semclt C");
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

void r_child(int shm_des, int sem_des, int id, char *argv) {
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

	char line[LINE_DIM];
	int line_n = 0;

	while (fgets(line, LINE_DIM, fd)) {
		if (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		WAIT(sem_des, S_R);
		strcpy(data->word, line);
		data->id = id;
		data->line_n = ++line_n;
		data->done = 0;
		SIGNAL(sem_des, S_C);

		printf("[R%d] riga-%d: %s\n", data->id, data->line_n, data->word);
	}

	WAIT(sem_des, S_R);
	data->done = 1;
	SIGNAL(sem_des, S_C);
	fclose(fd);

	exit(0);
}

void c_child(int shm_des, int sem_des, int msgq_des, int argc) {
	shm *data;

	if ((data = (shm *)shmat(shm_des, NULL, 0)) == (shm *)-1) {
		perror("shmat");
		exit(1);
	}

	msg mess_c;
	mess_c.type = 1;
	mess_c.done = 0;
	char done_counter = 0;

	while (1) {
		WAIT(sem_des, S_C);

		if (data->done) {
			done_counter++;

			if (done_counter == argc - 1) {
				break;
			}
			else {
				SIGNAL(sem_des, S_R);
				continue;
			}
		}

		for (int i = 0; i < LETTERS_NUM; i++) {
			mess_c.stats[i] = 0;
		}

		for (int i = 0; i < strlen(data->word); i++) {
			if (tolower(data->word[i]) - 97 >= 0 && tolower(data->word[i]) - 97 <= 25) {
				mess_c.stats[tolower(data->word[i]) - 97]++;
			}
		}

		mess_c.line_n = data->line_n;
		mess_c.id = data->id;
		SIGNAL(sem_des, S_R);

		if (msgsnd(msgq_des, &mess_c, MSG_DIM, 0) == -1) {
			perror("msgsnd");
			exit(1);
		}

		printf("[C] analizzata riga-%d per R%d\n", data->line_n, data->id);
	}

	mess_c.done = 1;

	if (msgsnd(msgq_des, &mess_c, MSG_DIM, 0) == -1) {
		perror("msgsnd");
		exit(1);
	}

	exit(0);
}

int main (int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "%s file-1> <file-2> ... <file-n>\n", argv[0]);
	}

	int shm_des = init_shm();
	int sem_des = init_sem();
	// Reader
	for (int i = 1; i < argc; i++) {
		if (!fork()) {
			r_child(shm_des, sem_des, i, argv[i]);
		}
	}

	int msgq_des = init_msgq();
	// Counter
	if (!fork()) {
		c_child(shm_des, sem_des, msgq_des, argc);
	}

	msg mess_p;
	global_stats g_stats[argc - 1];

	for (int i = 0; i < argc - 1; i++) {
		g_stats[i].lines_n = 0;

		for (int j = 0; j < LETTERS_NUM; j++) {
			g_stats[i].stats[j] = 0;
		}
	}

	while (1) {
		if (msgrcv(msgq_des, &mess_p, MSG_DIM, 0, 0) == -1) {
			perror("msgrcv");
			exit(1);
		}

		if (mess_p.done) {
			break;
		}

		g_stats[mess_p.id - 1].lines_n = mess_p.line_n;

		char buff[500] = "";

		for (int i = 0; i < LETTERS_NUM; i++) {
			char s[5];

			if (mess_p.stats[i] != 0) {
				g_stats[mess_p.id - 1].stats[i] += mess_p.stats[i];
				sprintf(s, " %c:%d", i + 97, mess_p.stats[i]);
				strcat(buff, s);
			}
		}

		printf("[P] statistica della riga-%d di R%d:%s\n", g_stats[mess_p.id - 1].lines_n, mess_p.id, buff);
	}

	for (int i = 0; i < argc - 1; i++) {
		char buff[500] = "";

		for (int j = 0; j < LETTERS_NUM; j++) {
			char s[7];

			if (g_stats[i].stats[j] != 0) {
				sprintf(s, " %c:%d", j + 97, g_stats[i].stats[j]);
				strcat(buff, s);
			}
		}

		printf("[P] statistiche finali su %d righe analizzate per R%d:%s\n", g_stats[i].lines_n, i + 1, buff);
	}

	for (int i = 0; i < argc; i++) {
		wait(NULL);
	}

	shmctl(shm_des, IPC_RMID, NULL);
	semctl(sem_des, 0, IPC_RMID, 0);
	msgctl(msgq_des, IPC_RMID, NULL);

	exit(0);
}
