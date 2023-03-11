#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>

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

void r_child(int pipefd, char *argv) {
	int fd;

	if ((fd = open(argv, O_RDONLY | 0600)) == -1) {
		perror("open");
		exit(1);
	}

	struct stat sb;

	if (fstat(fd, &sb) == -1) {
		perror("fstat");
		exit(1);
	}

	if (!S_ISREG(sb.st_mode)) {
		fprintf(stderr, "File %s is not valid!\n", argv);
		exit(1);
	}

	char *data;

	if ((data = (char *)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	close(fd);
	char line[LINE_DIM];

	for (int i = 0, j = 0; i < sb.st_size; i++) {
		line[j++] = data[i];

		if (data[i] == '\n') {
			line[j - 1] = '\0';
			dprintf(pipefd, "%s\n", line);
			strcpy(line, "");
			j = 0;
		}
	}

	dprintf(pipefd, "-1");
	munmap(data, sb.st_size);

	exit(0);
}

void w_child(int msgq_des) {
	msg mess_w;

	while (1) {
		if (msgrcv(msgq_des, &mess_w, MSG_DIM, 0, 0) == -1) {
			perror("msgrcv");
			exit(1);
		}

		if (mess_w.done) {
			break;
		}

		printf("%s", mess_w.line);
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <parola> <file>\n", argv[0]);
		exit(1);
	}

	int pipefd[2];

	if (pipe(pipefd) == -1) {
		perror("pipe");
		exit(1);
	}
	// R
	if (!fork()) {
		close(pipefd[0]);
		r_child(pipefd[1], argv[2]);
	}

	int msgq_des = init_msgq();
	// W
	if (!fork()) {
		close(pipefd[0]);
		close(pipefd[1]);
		w_child(msgq_des);
	}

	close(pipefd[1]);
	FILE *fd;

	if ((fd = fdopen(pipefd[0], "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	msg mess_p;
	mess_p.type = 1;
	mess_p.done = 0;
	char line[LINE_DIM];

	while (1) {
		fgets(line, LINE_DIM, fd);

		if (!strcmp(line, "-1")) {
			break;
		}

		if (strcasestr(line, argv[1])) {
			strcpy(mess_p.line, line);

			if (msgsnd(msgq_des, &mess_p, MSG_DIM, 0) == -1) {
				perror("msgsnd");
				exit(1);
			}
		}
	}

	mess_p.done = 1;

	if (msgsnd(msgq_des, &mess_p, MSG_DIM, 0) == -1) {
		perror("msgsnd");
		exit(1);
	}

	for (int i = 0; i < 2; i++) {
		wait(NULL);
	}

	close(pipefd[0]);
	fclose(fd);
	msgctl(msgq_des, IPC_RMID, NULL);

	exit(0);
}
