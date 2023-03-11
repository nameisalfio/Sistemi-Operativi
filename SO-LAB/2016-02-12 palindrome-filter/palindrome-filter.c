#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define WORD_DIM 1024
#define MSG_DIM sizeof(msg) - sizeof(long)

typedef enum {T_Base, T_P, T_W} M_TYPE;

typedef struct {
	long type;
	char word[WORD_DIM];
	char done;
} msg;

int init_msgq() {
	int msgq_des;

	if ((msgq_des = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("msgget");
		exit(1);
	}

	return msgq_des;
}

void r_child(int msgq_des, char *argv, char input) {
	FILE *fd;

	if (input) {
		if ((fd = fopen(argv, "r")) == NULL) {
			perror("fopen R");
			exit(1);
		}
	}
	else {
		fd = stdin;
	}

	msg mess_r;
	char word[WORD_DIM];

	while(fgets(word, WORD_DIM, fd)) {
		if (word[strlen(word) - 1] == '\n') {
			word[strlen(word) - 1] = '\0';
		}

		if (!input && !strcmp(word, "-1")) {
			break;
		}

		mess_r.type = T_P;
		strcpy(mess_r.word, word);
		mess_r.done = 0;

		if (msgsnd(msgq_des, &mess_r, MSG_DIM, 0) == -1) {
			perror("msgsnd R->P");
			exit(1);
		}
	}

	mess_r.type = T_P;
	mess_r.done = 1;

	if (msgsnd(msgq_des, &mess_r, MSG_DIM, 0) == -1) {
		perror("msgsnd R->P");
		exit(1);
	}

	if (input) {
		fclose(fd);
	}

	exit(0);
}

void w_child(int msgq_des, char *file_out, char output) {
	FILE *fd;

	if (output) {
		if ((fd = fopen(file_out, "w")) == NULL) {
			perror("fopen W");
			exit(1);
		}
	}

	msg mess_w;

	while (1) {
		if (msgrcv(msgq_des, &mess_w, MSG_DIM, T_W, 0) == -1) {
			perror("msgrcv W<-P");
			exit(1);
		}

		if (mess_w.done) {
			break;
		}

		if (output) {
			strcat(mess_w.word, "\n");
			fputs(mess_w.word, fd);
		}
		else {
			printf("%s\n", mess_w.word);
		}
	}

	if (output) {
		fclose(fd);
	}

	exit(0);
}

void p_father(int msgq_des) {
	msg mess_p;

	while (1) {
		if (msgrcv(msgq_des, &mess_p, MSG_DIM, T_P, 0) == -1) {
			perror("msgrcv P<-R");
			exit(1);
		}

		if (mess_p.done) {
			break;
		}

		char palindrome = 1;

		for (int i = 0, j = strlen(mess_p.word) - 1; i < strlen(mess_p.word); i++, j--) {
			if (mess_p.word[i] != mess_p.word[j]) {
				palindrome = 0;
				break;
			}
		}

		if (palindrome) {
			mess_p.type = T_W;
			mess_p.done = 0;

			if (msgsnd(msgq_des, &mess_p, MSG_DIM, 0) == -1) {
				perror("msgsnd P->W");
				exit(1);
			}
		}
	}

	mess_p.type = T_W;
	mess_p.done = 1;

	if (msgsnd(msgq_des, &mess_p, MSG_DIM, 0) == -1) {
		perror("msgsnd P->W");
		exit(1);
	}
}

int main(int argc, char **argv) {
	char input = 1, output = 0;
	char *file_out = "";

	if (argc > 3) {
		fprintf(stderr, "Usage: %s [input file] [output file]\n", argv[0]);
		exit(1);
	}
	else if (argc == 1) {
		input = 0;
	}
	else if (argc == 3) {
		output = 1;
		file_out = argv[2];
	}

	int msgq_des = init_msgq();
	// R
	if(!fork()) {
		r_child(msgq_des, argv[1], input);
	}
	// W
	if(!fork()) {
		w_child(msgq_des, file_out, output);
	}

	p_father(msgq_des);

	for (int i = 0; i < 2; i++) {
		wait(NULL);
	}

	msgctl(msgq_des, IPC_RMID, NULL);

	exit(0);
}
