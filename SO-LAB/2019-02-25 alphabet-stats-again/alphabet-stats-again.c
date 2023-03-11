#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <ctype.h>

#define LINE_DIM 2048
#define LETTERS_NUM 26
#define MSG1_DIM sizeof(msg1) - sizeof(long)
#define MSG2_DIM sizeof(msg2) - sizeof(long)

typedef struct {
	long type;
	char line[LINE_DIM];
	int occurrences[LETTERS_NUM];
	char done;
} msg1;

typedef struct {
	long type;
	int occurrences[LETTERS_NUM];
} msg2;

int init_msgq() {
	int msgq_des;

	if ((msgq_des = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
		perror("msgget");
		exit(1);
	}

	return msgq_des;
}

void r_child(int msgq_des, int msgq_des2, int id, char *argv, int argc) {
	FILE *fd;

	if ((fd = fopen(argv, "r")) == NULL) {
		fprintf(stderr, "fopen Reader%d %s\n", id, argv);
		exit(1);
	}

	msg1 mess_r;
	int global_occurrences[LETTERS_NUM];
	char line[LINE_DIM];

	for (int i = 0; i < LETTERS_NUM; i++) {
		global_occurrences[i] = 0;
	}

	while (fgets(line, LINE_DIM, fd)) {
		if (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		for (int i = 0; i < LETTERS_NUM; i++) {
			mess_r.occurrences[i] = 0;
		}

		mess_r.type = id;
		strcpy(mess_r.line, line);
		mess_r.done = 0;

		if (msgsnd(msgq_des, &mess_r, MSG1_DIM, 0) == -1) {
			fprintf(stderr, "msgsnd Reader%d->Counter\n", id);
			exit(1);
		}

		if (msgrcv(msgq_des, &mess_r, MSG1_DIM, id + argc, 0) == -1) {
			perror("msgrcv Reader<-Counter");
			exit(1);
		}

		for (int i = 0; i < LETTERS_NUM; i++) {
			global_occurrences[i] += mess_r.occurrences[i];
		}
	}

	mess_r.type = id;
	mess_r.done = 1;

	if (msgsnd(msgq_des, &mess_r, MSG1_DIM, 0) == -1) {
		fprintf(stderr, "msgsnd Reader%d->Counter\n", id);
		exit(1);
	}

	msg2 mess_r2;
	mess_r2.type = id;

	for (int i = 0; i < LETTERS_NUM; i++) {
		mess_r2.occurrences[i] = global_occurrences[i];
	}

	if (msgsnd(msgq_des2, &mess_r2, MSG2_DIM, 0) == -1) {
		fprintf(stderr, "msgsnd Reader%d->Father\n", id);
		exit(1);
	}

	fclose(fd);
	
	exit(0);
}

void c_child(int msgq_des, int argc) {
	msg1 mess_c;
	char done_counter = 0;

	while (1) {
		if (msgrcv(msgq_des, &mess_c, MSG1_DIM, -argc, 0) == -1) {
			perror("msgrcv Counter<-Reader");
			exit(1);
		}

		if (mess_c.done) {
			done_counter++;

			if (done_counter == argc) {
				break;
			}

			continue;
		}

		mess_c.type = mess_c.type + argc;

		for (int i = 0; i < strlen(mess_c.line); i++) {
			if (tolower(mess_c.line[i]) >= 'a' && tolower(mess_c.line[i]) <= 'z') {
				mess_c.occurrences[tolower(mess_c.line[i]) - 'a']++;
			}
		}

		if (msgsnd(msgq_des, &mess_c, MSG1_DIM, 0) == -1) {
			fprintf(stderr, "msgsnd Counter->Reader%ld\n", mess_c.type - argc);
			exit(1);
		}
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <file-1> <file-2> ... <file-n>\n", argv[0]);
		exit(1);
	}

	int msgq_des[2];

	for (int i = 0; i < 2; i++) {
		msgq_des[i] = init_msgq();
	}
	// Reader
	for (int i = 0; i < argc - 1; i++) {
		if (!fork()) {
			r_child(msgq_des[0], msgq_des[1], i + 1, argv[i + 1], argc - 1);
		}
	}
	// Counter
	if (!fork()) {
		c_child(msgq_des[0], argc - 1);
	}

	msg2 mess_p;

	for (int i = 0; i < argc - 1; i++) {
		if (msgrcv(msgq_des[1], &mess_p, MSG2_DIM, 0, 0) == -1) {
			perror("msgrcv Father<-Reader");
			exit(1);
		}

		for (int j = 0; j < LETTERS_NUM; j++) {
			printf("%c=%d ", j + 'a', mess_p.occurrences[j]);
		}

		printf("\n");
	}

	for (int i = 0; i < argc; i++) {
		wait(NULL);
	}

	for (int i = 0; i < 2; i++) {
		msgctl(msgq_des[i], IPC_RMID, NULL);
	}

	exit(0);
}
