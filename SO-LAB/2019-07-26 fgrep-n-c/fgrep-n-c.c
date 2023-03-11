#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define LINE_DIM 2048
#define WORD_DIM 50
#define MSGF_DIM sizeof(msg_f) - sizeof(long)
#define MSGP_DIM sizeof(msg_p) - sizeof(long)

typedef struct {
	long type;
	char line[LINE_DIM];
	char nome_file[WORD_DIM];
	int line_n;
	char id;
	char done;
} msg_f;

typedef struct {
	long type;
	char word[WORD_DIM];
	char done;
} msg_p;

int init_msgq() {
	int msgq_des;

	if ((msgq_des = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
		perror("msget");
		exit(1);
	}

	return msgq_des;
}

void f_child(int msgq_des[2], int id, char *argv) {
	FILE *fd;

	if ((fd = fopen(argv, "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	char line[LINE_DIM];
	int line_n = 0;

	while (fgets(line, LINE_DIM, fd)) {
		line_n++;
	}

	if (fseek(fd, 0, SEEK_SET) == -1) {
		perror("fseek");
		exit(1);
	}

	char **arr = malloc(sizeof(char *) * line_n);

	for (int i = 0; i < line_n; i++) {
		fgets(line, LINE_DIM, fd);

		if (line[strlen(line) - 1] == '\n') {
			line[strlen(line) - 1] = '\0';
		}

		arr[i] = malloc(sizeof(char) * LINE_DIM);
		strcpy(arr[i], line);
	}

	msg_p mess_p;
	msg_f mess_f;
	mess_f.type = 1;

	while (1) {
		if (msgrcv(msgq_des[0], &mess_p, MSGP_DIM, id, 0) == -1) {
			fprintf(stderr, "msgrcv F%d\n", id);
			exit(1);
		}

		if (mess_p.done) {
			break;
		}

		for (int i = 0; i < line_n; i++) {
			if (strstr(arr[i], mess_p.word)) {
				strcpy(mess_f.line, arr[i]);
				strcpy(mess_f.nome_file, argv);
				mess_f.line_n = i + 1;
				mess_f.id = id - 1;
				mess_f.done = 0;

				if (msgsnd(msgq_des[1], &mess_f, MSGF_DIM, 0) == -1) {
					perror("msgsnd F->P");
					exit(1);
				}
			}
		}

		mess_f.done = 1;

		if (msgsnd(msgq_des[1], &mess_f, MSGF_DIM, 0) == -1) {
			perror("msgsnd F->P");
			exit(1);
		}
	}

	exit(0);
}

void p_father(int msgq_des[2], int words_n, int files_n, char **argv) {
	int stats[files_n];

	for (int i = 0; i < files_n; i++) {
		stats[i] = 0;
	}

	msg_p mess_p;
	mess_p.done = 0;
	msg_f mess_f;

	for (int i = 0; i < words_n; i++) {
		strcpy(mess_p.word, argv[i + 1]);

		for (int i = 0; i < files_n; i++) {
			mess_p.type = (i + 1);

			if (msgsnd(msgq_des[0], &mess_p, MSGP_DIM, 0) == -1) {
				perror("msgsnd P->F");
				exit(1);
			}
		}

		char done_counter = 0;

		while (1) {
			if (msgrcv(msgq_des[1], &mess_f, MSGF_DIM, 0, 0) == -1) {
				perror("msgrcv P");
				exit(1);
			}

			if (mess_f.done) {
				done_counter++;

				if (done_counter == files_n) {
					break;
				}

				continue;
			}

			stats[mess_f.id]++;
			printf("%s@%s:%d:%s\n", argv[i + 1], mess_f.nome_file, mess_f.line_n, mess_f.line);
		}
	}

	mess_p.done = 1;

	for (int i = 0; i < files_n; i++) {
		mess_p.type = (i + 1);

		if (msgsnd(msgq_des[0], &mess_p, MSGP_DIM, 0) == -1) {
			perror("msgsnd P->F");
			exit(1);
		}
	}

	for (int i = 0; i < files_n; i++) {
		printf("%s:%d\n", argv[i + words_n + 2], stats[i]);
	}
}

int main(int argc, char **argv) {
	char nail = 0, words_n, files_n;

	for (int i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "@") && i != argc - 1) {
			nail = 1;
			words_n = i - 1;
			files_n = (argc - 2) - words_n;
		}
	}

	if (!nail) {
		fprintf(stderr, "Usage: %s <word-1> [word-2] [...] @ <file-1> [file-2] [...]\n", argv[0]);
	}

	int msgq_des[2];

	for (int i = 0; i < 2; i++) {
		msgq_des[i] = init_msgq();
	}
	// F
	for (int i = 0; i < files_n; i++) {
		if (!fork()) {
			f_child(msgq_des, i + 1, argv[i + words_n + 2]);
		}
	}

	p_father(msgq_des, words_n, files_n, argv);

	for (int i = 0; i < files_n; i++) {
		wait(NULL);
	}

	for (int i = 0; i < 2; i++) {
		msgctl(msgq_des[i], IPC_RMID, NULL);
	}

	exit(0);
}
