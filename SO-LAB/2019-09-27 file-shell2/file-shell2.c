#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <linux/limits.h>

#define SIZE 100
#define MSG_DIM sizeof(msg) - sizeof(long)

typedef struct {
	long type;
	char parameter[SIZE];
	char file[SIZE];
	char letter[1];
	char result[SIZE];
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

void d_child(int msgq_des, int id, char *path) {
	DIR *d;

	if ((d = opendir(path)) == NULL) {
		fprintf(stderr, "opendir D%d\n", id);
		exit(1);
	}

	struct dirent *dirent;
	struct stat sb;
	msg mess_d;

	while (1) {
		if (msgrcv(msgq_des, &mess_d, MSG_DIM, id + 1, 0) == -1) {
			perror("msgrcv");
			exit(1);
		}

		if (mess_d.done) {
			break;
		}

		if (!strcmp(mess_d.parameter, "num-files")) {
			int files_counter = 0;

			while (dirent = readdir(d)) {
				if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, "..")) {
					continue;
				}
				else if (dirent->d_type == DT_REG) {
					files_counter++;
				}
			}

			seekdir(d, 0);
			mess_d.type = 1;
			sprintf(mess_d.result, "%d file\n", files_counter);

			if (msgsnd(msgq_des, &mess_d, MSG_DIM, 0) == -1) {
				fprintf(stderr, "msgsnd D%d->P\n", id);
				exit(1);
			}
		}
		else if (!strcmp(mess_d.parameter, "total-size")) {
			int total_size = 0;

			while (dirent = readdir(d)) {
				if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, "..")) {
					continue;
				}
				else if (dirent->d_type == DT_REG) {
					char tmp_dir[PATH_MAX];
					sprintf(tmp_dir, "%s/%s", path, dirent->d_name);

					if (stat(tmp_dir, &sb)) {
						fprintf(stderr, "stat D%d %s\n", id, dirent->d_name);
						exit(1);
					}

					total_size += sb.st_size;
				}
			}

			seekdir(d, 0);
			mess_d.type = 1;
			sprintf(mess_d.result, "%d byte\n", total_size);

			if (msgsnd(msgq_des, &mess_d, MSG_DIM, 0) == -1) {
				fprintf(stderr, "msgsnd D%d->P\n", id);
				exit(1);
			}
		}
		else if (!strcmp(mess_d.parameter, "search-char")) {
			char tmp_dir[PATH_MAX];
			sprintf(tmp_dir, "%s/%s", path, mess_d.file);
			int fd;

			if ((fd = open(tmp_dir, O_RDONLY)) == -1) {
				perror("open");
				exit(1);
			}

			if (fstat(fd, &sb) == -1) {
				perror("fstat");
				exit(1);
			}

			if (!S_ISREG(sb.st_mode)) {
				fprintf(stderr, "%s non è un file regolare\n", mess_d.file);
				exit(1);
			}

			char *data;

			if ((data = (char *)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
				perror("mmap");
				exit(1);
			}

			close(fd);
			int letter_counter = 0;

			for (int i = 0; i < sb.st_size; i++) {
				if (data[i] == mess_d.letter[0]) {
					letter_counter++;
				}
			}

			mess_d.type = 1;
			sprintf(mess_d.result, "%d\n", letter_counter);

			if (msgsnd(msgq_des, &mess_d, MSG_DIM, 0) == -1) {
				fprintf(stderr, "msgsnd D%d->P\n", id);
				exit(1);
			}

			munmap(data, sb.st_size);
		}
	}

	closedir(d);

	exit(0);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		argc = 2;
		argv[1] = ".";
	}

	int msgq_des = init_msgq();
	// D
	for (int i = 0; i < argc - 1; i++) {
		if (!fork()) {
			d_child(msgq_des, i + 1, argv[i + 1]);
		}
	}

	msg mess_p;
	mess_p.done = 0;

	while (1) {
		printf("file-shell> ");
		char input[SIZE] = "";
		fgets(input, SIZE, stdin);

		if (string[strlen(string) - 1] == '\n') {
			string[strlen(string) - 1] = '\0';
		}

		if (input[0] == '0') {
			break;
		}

		char *option1 = "", *option2 = "", *option3 = "", *option4 = "";

		if ((option1 = strtok(input, " ")) != NULL) {
			if ((option2 = strtok(NULL, " ")) != NULL) {
				if ((option3 = strtok(NULL, " ")) != NULL) {
					option4 = strtok(NULL, " ");
				}
			}
		}

		mess_p.type = atoi(option2) + 1;
		strcpy(mess_p.parameter, option1);

		if (option3 != NULL) {
			strcpy(mess_p.file, option3);
			strcpy(mess_p.letter, option4);
		}

		if (msgsnd(msgq_des, &mess_p, MSG_DIM, 0) == -1) {
			perror("msgsnd P->D");
			exit(1);
		}

		if (msgrcv(msgq_des, &mess_p, MSG_DIM, 1, 0) == -1) {
			perror("msgrcv");
			exit(1);
		}

		printf("%s\n", mess_p.result);
	}

	for (int i = 1; i < argc; i++) {
		mess_p.type = i + 1;
		mess_p.done = 1;

		if (msgsnd(msgq_des, &mess_p, MSG_DIM, 0) == -1) {
			perror("msgsnd P->D");
			exit(1);
		}

		wait(NULL);
	}

	msgctl(msgq_des, IPC_RMID, NULL);

	exit(0);
}
