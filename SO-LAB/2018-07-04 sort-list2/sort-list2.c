#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MAX_WORD_LEN 1024
#define MSG_DIM sizeof(msg) - sizeof(long)

typedef struct {
	long type;
	char word1[MAX_WORD_LEN];
	char word2[MAX_WORD_LEN];
	int result;
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

void s_child(int msgq_des, int pipefd, char *argv) {
	FILE *fd;

	if ((fd = fopen(argv, "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	char word[MAX_WORD_LEN];
	int words_n = 0;

	while (fgets(word, MAX_WORD_LEN, fd)) {
		words_n++;
	}

	if (fseek(fd, 0, SEEK_SET) == -1) {
		perror("fseek");
		exit(1);
	}

	char **words_arr = malloc(sizeof(char *) * words_n);

	for (int i = 0; i < words_n; i++) {
		fgets(word, MAX_WORD_LEN, fd);

		if (word[strlen(word) - 1] == '\n') {
			word[strlen(word) - 1] = '\0';
		}

		words_arr[i] = malloc(sizeof(char) * MAX_WORD_LEN);
		strcpy(words_arr[i], word);
	}

	msg mess_s;
	mess_s.done = 0;
	int min;

	for (int i = 0; i < words_n - 1; i++) {
		min = i;

		for (int j = i + 1; j < words_n; j++) {
			mess_s.type = 1;
			strcpy(mess_s.word1, words_arr[min]);
			strcpy(mess_s.word2, words_arr[j]);

			if (msgsnd(msgq_des, &mess_s, MSG_DIM, 0) == -1) {
				perror("msgsnd S->C");
				exit(1);
			}

			if (msgrcv(msgq_des, &mess_s, MSG_DIM, 2, 0) == -1) {
				perror("msgrcv S<-C");
				exit(1);
			}

			if (mess_s.result > 0) {
				min = j;
			}
		}

		if (min != i) {
			char tmp[MAX_WORD_LEN];
			strcpy(tmp, words_arr[min]);
			strcpy(words_arr[min], words_arr[i]);
			strcpy(words_arr[i], tmp);
		}
	}

	mess_s.type = 1;
	mess_s.done = 1;

	if (msgsnd(msgq_des, &mess_s, MSG_DIM, 0) == -1) {
		perror("msgsnd S->C");
		exit(1);
	}

	for (int i = 0; i < words_n; i++) {
		dprintf(pipefd, "%s\n", words_arr[i]);
		free(words_arr[i]);
	}

	dprintf(pipefd, "-1\n");
	free(words_arr);
	fclose(fd);
	close(pipefd);

	exit(0);
}

void c_child(int msgq_des) {
	msg mess_c;

	while (1) {
		if (msgrcv(msgq_des, &mess_c, MSG_DIM, 1, 0) == -1) {
			perror("msgrcv C<-S");
			exit(1);
		}

		if (mess_c.done) {
			break;
		}

		mess_c.type = 2;
		mess_c.result = strcasecmp(mess_c.word1, mess_c.word2);

		if (msgsnd(msgq_des, &mess_c, MSG_DIM, 0) == -1) {
			perror("msgsnd C->S");
			exit(1);
		}
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <file>\n", argv[0]);
		exit(1);
	}

	int msgq_des = init_msgq();
	int pipefd[2];

	if (pipe(pipefd) == -1) {
		perror("pipe");
		exit(1);
	}
	// Sorter
	if (!fork()) {
		close(pipefd[0]);
		s_child(msgq_des, pipefd[1], argv[1]);
	}
	// Comparer
	if (!fork()) {
		close(pipefd[0]);
		close(pipefd[1]);
		c_child(msgq_des);
	}

	close(pipefd[1]);
	FILE *pfd;

	if ((pfd = fdopen(pipefd[0], "r")) == NULL) {
		perror("fdopen");
		exit(1);
	}

	char word[MAX_WORD_LEN];

	while (1) {
		fgets(word, MAX_WORD_LEN, pfd);

		if (!strcmp(word, "-1\n")) {
			break;
		}

		printf("%s", word);
	}

	for (int i = 0; i < 2; i++) {
		wait(NULL);
	}

	msgctl(msgq_des, IPC_RMID, NULL);
	fclose(pfd);
	close(pipefd[0]);

	exit(0);
}
