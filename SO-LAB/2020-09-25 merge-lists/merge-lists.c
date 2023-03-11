#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define WORD_DIM 256
#define MAX_WORDS 200
#define MSG_DIM sizeof(msg) - sizeof(long)

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

void r_child(int msgq_des, int id, char *argv) {
	FILE *fd;

	if ((fd = fopen(argv, "r")) == NULL) {
		fprintf(stderr, "fopen R%d\n", id);
		exit(1); 
	}

	msg mess_r;
	mess_r.type = 1;
	char word[WORD_DIM];

	while (fgets(word, WORD_DIM, fd)) {
		int j = 0;
		mess_r.done = 0;

		for (int i = 0; i < strlen(word); i++) {
			if (word[i] == '\n') {
				word[j++] = '\0';
			}
			else if (!isspace(word[i])) {
				word[j++] = word[i];
			}
		}

		strcpy(mess_r.word, word);

		if (msgsnd(msgq_des, &mess_r, MSG_DIM, 0) == -1) {
			fprintf(stderr, "msgsnd R%d\n", id);
			exit(1);
		}
	}

	mess_r.done = 1;

	if (msgsnd(msgq_des, &mess_r, MSG_DIM, 0) == -1) {
		fprintf(stderr, "msgsnd R%d\n", id);
		exit(1);
	}

	fclose(fd);

	exit(0);
}

void w_child(int pipefd) {
	FILE *pfd;

	if ((pfd = fdopen(pipefd, "r")) == NULL) {
		perror("fdopen");
		exit(1);
	}
	
	char word[WORD_DIM];

	while (1) {
		fgets(word, WORD_DIM, pfd);

		if (!strcmp(word, "-1\n")) {
			break;
		}

		if (word[strlen(word) - 1] == '\n') {
			word[strlen(word) - 1] = '\0';
		}

		printf("%s\n", word);
	}

	fclose(pfd);

	exit(0);
}

void p_father(int msgq_des, int pipefd) {
	char **words = malloc(sizeof(char *) * MAX_WORDS);
	msg mess_p;
	char done_counter = 0;
	int words_n = 0;

	for (int i = 0; i < MAX_WORDS; i++) {
		words[i] = malloc(sizeof(char) * WORD_DIM);
		strcpy(words[i], "");
	}

	while (1) {
		if (msgrcv(msgq_des, &mess_p, MSG_DIM, 0, 0) == -1) {
			perror("msgrcv");
			exit(1);
		}

		if (mess_p.done) {
			done_counter++;

			if (done_counter == 2) {
				break;
			}

			continue;
		}

		int found = 0;

		for (int i = 0; i < MAX_WORDS; i++) {
			if (!strcasecmp(words[i], mess_p.word)) {
				found = 1;
				break;
			}
		}

		if (!found) {
			strcpy(words[words_n++], mess_p.word);
			dprintf(pipefd, "%s\n", mess_p.word);
		}
	}

	dprintf(pipefd, "-1\n");

	for (int i = 0; i < MAX_WORDS; i++) {
		free(words[i]);
	}

	free(words);
}

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <file-1> <file-2>\n", argv[0]);
	}

	int msgq_des = init_msgq();
	int pipefd[2];

	if (pipe(pipefd) == -1) {
		perror("pipe");
		exit(1);
	}
	// R
	for (int i = 0; i < 2; i++) {
		if (!fork()) {
			close(pipefd[0]);
			close(pipefd[1]);
			r_child(msgq_des, i + 1, argv[i + 1]);
		}
	}
	// W
	if (!fork()) {
		close(pipefd[1]);
		w_child(pipefd[0]);
	}

	close(pipefd[0]);
	// P
	p_father(msgq_des, pipefd[1]);

	for (int i = 0; i < 3; i++) {
		wait(NULL);
	}

	msgctl(msgq_des, IPC_RMID, NULL);
	close(pipefd[1]);

	exit(0);
}
