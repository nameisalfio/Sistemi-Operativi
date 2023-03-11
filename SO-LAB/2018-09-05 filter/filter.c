#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <ctype.h>

#define LINE_DIM 2048
#define MSG_DIM sizeof(msg) - sizeof(long)

typedef struct {
	long type;
	char line[LINE_DIM];
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

void filter_child(int msgq_des, int id, char *filter) {
	msg mess_f;
	char filter_type = filter[0], word[20], *word1, *word2;
	strcpy(word, filter + 1);

	if (filter_type == '%') {
		if ((word1 = strtok(word, "|")) != NULL) {
			word2 = strtok(NULL, "|");
		}
	}

	while (1) {
		if (msgrcv(msgq_des, &mess_f, MSG_DIM, id, 0) == -1) {
			perror("msgrcv Filter<-Father");
			exit(1);
		}

		if (mess_f.done) {
			break;
		}

		if (filter_type == '^') {
			char *substr;

			while (substr = strstr(mess_f.line, word)) {
				for (int i = 0; i < strlen(word); i++) {
					substr[i] = toupper(substr[i]);
				}
			}
		}
		else if (filter_type == '_') {
			char *substr;

			while (substr = strstr(mess_f.line, word)) {
				for (int i = 0; i < strlen(word); i++) {
					substr[i] = tolower(substr[i]);
				}
			}
		}
		else if (filter_type == '%') {
			char *substr, next_substr[LINE_DIM];

			while (substr = strstr(mess_f.line, word1)) {
				for (int i = 0; i < strlen(word1); i++) {
					substr[i] = word2[i];
				}
			}
		}

		mess_f.type = id + 1;

		if (msgsnd(msgq_des, &mess_f, MSG_DIM, 0) == -1) {
			fprintf(stderr, "msgsnd Filter%d->Filter%d\n", id, id + 1);
			exit(1);
		}
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s  <file.txt> <filter-1> [filter-2] [...]\n", argv[0]);
		exit(1);
	}

	int msgq_des = init_msgq();
	// Filter
	for (int i = 0; i < argc - 2; i++) {
		if (!fork()) {
			filter_child(msgq_des, i + 1, argv[i + 2]);
		}
	}

	FILE *fd;

	if ((fd = fopen(argv[1], "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	char line[LINE_DIM];
	msg mess_p;
	mess_p.done = 0;

	while (fgets(line, LINE_DIM, fd)) {
		if (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		mess_p.type = 1;
		strcpy(mess_p.line, line);

		if (msgsnd(msgq_des, &mess_p, MSG_DIM, 0) == -1) {
			perror("msgsnd Father->Filter");
			exit(1);
		}

		if (msgrcv(msgq_des, &mess_p, MSG_DIM, argc - 1, 0) == -1) {
			perror("msgrcv Father<-Filter");
			exit(1);
		}

		printf("%s\n", mess_p.line);
	}

	mess_p.done = 1;

	for (int i = 0; i < argc - 2; i++) {
		mess_p.type = i + 1;

		if (msgsnd(msgq_des, &mess_p, MSG_DIM, 0) == -1) {
			perror("msgsnd Father->Filter");
			exit(1);
		}
	}

	for (int i = 0; i < argc - 2; i++) {
		wait(NULL);
	}

	msgctl(msgq_des, IPC_RMID, NULL);
	fclose(fd);

	exit(0);
}
