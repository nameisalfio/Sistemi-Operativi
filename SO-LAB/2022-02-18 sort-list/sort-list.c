#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_WORD_LEN 50
#define MSG_DIM sizeof(msg) - sizeof(long)

typedef struct {
	long type;
	char word1[MAX_WORD_LEN];
	char word2[MAX_WORD_LEN];
	char compare_check;
	char done;
} msg;

int init_msgq() {
	int msgq_des;

	if((msgq_des = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600)) == -1) {
		perror("msgget");
		exit(1);
	}

	return msgq_des;
}

int get_words_n(FILE *fd) {
	int word_n = 0;
	char word[MAX_WORD_LEN];

	while (fgets(word, MAX_WORD_LEN, fd)) {
		word_n++;
	}

	if (fseek(fd, 0, SEEK_SET)) {
		perror("fseek");
		exit(1);
	}

	return word_n;
}

void selection_sort(int msgq_des, msg mess_s, char **words, int words_n) {
	for (int i = 0; i < words_n - 1; i++) {
		int min_pos = i;

		for (int j = i + 1; j < words_n; j++) {
			mess_s.type = 1;
			strcpy(mess_s.word1, words[min_pos]);
			strcpy(mess_s.word2, words[j]);

			if (msgsnd(msgq_des, &mess_s, MSG_DIM, 0) == -1) {
				perror("msgsnd Sorter");
				exit(1);
			}

			if (msgrcv(msgq_des, &mess_s, MSG_DIM, 2, 0) == -1) {
				perror("msgrcv Sorter");
				exit(1);
			}

			if (mess_s.compare_check > 0) {
				min_pos = j;
			}
		}

		if (min_pos != i) {
			char *temp = words[i];
			words[i] = words[min_pos];
			words[min_pos] = temp;
		}
	}
}

void sorter_child(int msgq_des, char *argv) {
	FILE *fd;

	if ((fd = fopen(argv, "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	int words_n = get_words_n(fd);
	char **words = malloc(words_n * sizeof(char *));
	char line[MAX_WORD_LEN];

	for (int i = 0; i < words_n; i++) {
		fgets(line, MAX_WORD_LEN, fd);

		if (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		words[i] = malloc(strlen(line) * sizeof(char));
		strcpy(words[i], line);
	}

	msg mess_s;
	mess_s.done = 0;
	selection_sort(msgq_des, mess_s, words, words_n);
	mess_s.type = 1;
	mess_s.done = 1;

	if (msgsnd(msgq_des, &mess_s, MSG_DIM, 0) == -1) {
		perror("msgsnd Sorter");
		exit(1);
	}

	mess_s.type = 3;
	mess_s.done = 0;

	for (int i = 0; i < words_n - 1; i++) {
		strcpy(mess_s.word1, words[i]);

		if (msgsnd(msgq_des, &mess_s, MSG_DIM, 0) == -1) {
			perror("msgsnd Sorter");
			exit(1);
		}
	}

	mess_s.done = 1;

	if (msgsnd(msgq_des, &mess_s, MSG_DIM, 0) == -1) {
		perror("msgsnd Sorter");
		exit(1);
	}

	exit(0);
}


void comparer_child(int msgq_des) {
	msg mess_c;

	while (1) {
		if (msgrcv(msgq_des, &mess_c, MSG_DIM, 1, 0) == -1) {
			perror("msgrcv Comparer");
			exit(1);
		}

		if (mess_c.done) {
			break;
		}

		mess_c.type = 2;
		mess_c.compare_check = strcasecmp(mess_c.word1, mess_c.word2);

		if (msgsnd(msgq_des, &mess_c, MSG_DIM, 0) == -1) {
			perror("msgsnd Comparer");
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
	// Sorter
	if (!fork()) {
		sorter_child(msgq_des, argv[1]);
	}
	// Comparer
	if (!fork()) {
		comparer_child(msgq_des);
	}

	msg mess_p;

	while (1) {
		if (msgrcv(msgq_des, &mess_p, MSG_DIM, 3, 0) == -1) {
			perror("msgrcv Father");
			exit(1);
		}

		if (mess_p.done) {
			break;
		}

		printf("%s\n", mess_p.word1);
	}

	msgctl(msgq_des, IPC_RMID, NULL);

	exit(0);
}
