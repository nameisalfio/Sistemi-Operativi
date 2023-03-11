#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/msg.h>

#define NAME_DIM 1024
#define MSG_DIM sizeof(msg) - sizeof(long)

typedef struct {
	long type;
	char name[NAME_DIM];
	char file_name[NAME_DIM];
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
		fprintf(stderr, "fopen Reader%d %s\n", id, argv);
		exit(1);
	}

	msg mess_r;
	mess_r.type = 1;
	char name[NAME_DIM];

	while (fgets(name, NAME_DIM, fd)) {
		if (name[strlen(name) - 1] == '\n') {
			name[strlen(name) - 1] = '\0';
		}

		strcpy(mess_r.name, name);
		strcpy(mess_r.file_name, argv);
		mess_r.done = 0;

		if (msgsnd(msgq_des, &mess_r, MSG_DIM, 0) == -1) {
			fprintf(stderr, "msgsnd Reader%d->Filtere\n", id);
			exit(1);
		}
	}

	mess_r.done = 1;

	if (msgsnd(msgq_des, &mess_r, MSG_DIM, 0) == -1) {
		fprintf(stderr, "msgsnd Reader%d->Filtere\n", id);
		exit(1);
	}

	fclose(fd);

	exit(0);
}

void f_child(int msgq_des, int pipefd, char *word, int files_n, char v, char i) {
	msg mess_f;
	char done_counter = 0;

	while(1) {
		if (msgrcv(msgq_des, &mess_f, MSG_DIM, 0, 0) == -1) {
			perror("msgrcv Filterer<-Reader");
			exit(1);
		}

		if (mess_f.done) {
			done_counter++;

			if (done_counter == files_n) {
				break;
			}

			continue;
		}

		if (!v && !i) {
			if (strstr(mess_f.name, word)) {
				if (files_n > 1) {
					dprintf(pipefd, "%s:%s\n", mess_f.file_name, mess_f.name);
				}
				else {
					dprintf(pipefd, "%s\n", mess_f.name);
				}
			}
		}
		else if (v && !i) {
			if (!strstr(mess_f.name, word)) {
				if (files_n > 1) {
					dprintf(pipefd, "%s:%s\n", mess_f.file_name, mess_f.name);
				}
				else {
					dprintf(pipefd, "%s\n", mess_f.name);
				}
			}
		}
		else if (!v && i) {
			if (strcasestr(mess_f.name, word)) {
				if (files_n > 1) {
					dprintf(pipefd, "%s:%s\n", mess_f.file_name, mess_f.name);
				}
				else {
					dprintf(pipefd, "%s\n", mess_f.name);
				}
			}
		}
		else if (v && i) {
			if (!strcasestr(mess_f.name, word)) {
				if (files_n > 1) {
					dprintf(pipefd, "%s:%s\n", mess_f.file_name, mess_f.name);
				}
				else {
					dprintf(pipefd, "%s\n", mess_f.name);
				}
			}
		}
	}

	dprintf(pipefd, "-1\n");
	close(pipefd);

	exit(0);
}

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s [-v] [-i] <word> <file-1> [file-2] [file-3] [...]\n", argv[0]);
		exit(1);
	}

	int files_n = argc - 2, file_p = 2;
	char v = 0, i = 0;

	if (!strcmp(argv[1], "-i") || !strcmp(argv[1], "-v")) {
		files_n--;
		file_p++;

		if (!strcmp(argv[1], "-i")) {
			i = 1;
		}
		else if (!strcmp(argv[1], "-v")) {
			v = 1;
		}
	}

	if (!strcmp(argv[2], "-i") || !strcmp(argv[2], "-v")) {
		files_n--;
		file_p++;

		if (!strcmp(argv[2], "-i")) {
			i = 1;
		}
		else if (!strcmp(argv[2], "-v")) {
			v = 1;
		}
	}

	int msgq_des = init_msgq();
	int pipefd[2];

	if (pipe(pipefd) == -1) {
		perror("pipe");
		exit(1);
	}
	// Reader
	for (int i = 0; i < files_n; i++) {
		if (!fork()) {
			close(pipefd[0]);
			close(pipefd[1]);
			r_child(msgq_des, i + 1, argv[i + file_p]);
		}
	}
	// Filterer
	if (!fork()) {
		close(pipefd[0]);
		f_child(msgq_des, pipefd[1], argv[file_p - 1], files_n, v, i);
	}

	close(pipefd[1]);
	FILE *pfd;

	if ((pfd = fdopen(pipefd[0], "r")) == NULL) {
		perror("fdopen");
		exit(1);
	}

	char name[NAME_DIM];

	while (1) {
		fgets(name, NAME_DIM * 2, pfd);

		if (!strcmp(name, "-1\n")) {
			break;
		}

		printf("%s", name);
	}

	for (int i = 0; i <= files_n; i++) {
		wait(NULL);
	}

	msgctl(msgq_des, IPC_RMID, NULL);
	fclose(pfd);
	close(pipefd[0]);

	exit(0);
}
