#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>

#define NAME_DIM 1024
#define MSG_DIM sizeof(msg) - sizeof(long)

typedef enum {C_LIST, C_SIZE, C_SEARCH} C_TYPE;

typedef struct {
	long type;
	char name[NAME_DIM / 3 - 1];
	char word[NAME_DIM / 3 - 1];
	char string[NAME_DIM / 3];
	char command;
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

void d_child(int msgq_des, int id, char *path, int argc) {
	DIR *d;

	if ((d = opendir(path)) == NULL) {
		perror("opendir");
		exit(1);
	}

	struct dirent *dirent;
	msg mess_d;

	while (1) {
		if (msgrcv(msgq_des, &mess_d, MSG_DIM, id + 1, 0) == -1) {
			fprintf(stderr, "msgrcv D-%d<-P\n", id);
			exit(1);
		}

		if (mess_d.done) {
			break;
		}

		if (mess_d.command == C_LIST) {
			while (dirent = readdir(d)) {
				if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, "..")) {
					continue;
				}
				else if (dirent->d_type == DT_REG) {
					mess_d.type = 1;
					strcpy(mess_d.string, dirent->d_name);

					if (msgsnd(msgq_des, &mess_d, MSG_DIM, 0) == -1) {
						fprintf(stderr, "msgrcv D-%d->P\n", id);
						exit(1);
					}
				}
			}

			seekdir(d, 0);
			mess_d.type = 1;
			mess_d.done = 1;

			if (msgsnd(msgq_des, &mess_d, MSG_DIM, 0) == -1) {
				fprintf(stderr, "msgrcv D-%d->D-%d\n", id, id + 1);
				exit(1);
			}
		}
		else if (mess_d.command == C_SIZE) {
			struct stat sb;
			char tmp_path[NAME_DIM];
			sprintf(tmp_path, "%s/%s", path, mess_d.name);

			if (stat(tmp_path, &sb) == -1) {
				perror("stat");
				exit(1);
			}

			sprintf(mess_d.string, "%ld byte", sb.st_size);
			mess_d.type = 1;

			if (msgsnd(msgq_des, &mess_d, MSG_DIM, 0) == -1) {
				fprintf(stderr, "msgrcv D-%d->P\n", id);
				exit(1);
			}
		}
		else if (mess_d.command == C_SEARCH) {
			char tmp_dir[NAME_DIM];
			sprintf(tmp_dir, "%s/%s", path, mess_d.name);
			FILE *fd;

			if ((fd = fopen(tmp_dir, "r")) == NULL) {
				perror("fopen");
				exit(1);
			}

			char line[NAME_DIM];
			char occurrences = 0;

			while (fgets(line, NAME_DIM, fd)) {
				if (strstr(line, mess_d.word)) {
					occurrences++;
				}
			}

			mess_d.type = 1;
			sprintf(mess_d.string, "%d", occurrences);

			if (msgsnd(msgq_des, &mess_d, MSG_DIM, 0) == -1) {
				fprintf(stderr, "msgrcv D-%d->P\n", id);
				exit(1);
			}

			fclose(fd);
		}
	}

	closedir(d);

	exit(0);
}

int main(int argc, char **argv) {
	if (argc == 1) {
		argc = 2;
		argv[1] = ".";
	}

	int msgq_des = init_msgq();
	// D
	for (int i = 0; i < argc - 1; i++) {
		if (!fork()) {
			d_child(msgq_des, i + 1, argv[i + 1], argc - 1);
		}
	}

	msg mess_p;

	while (1) {
		printf("file-shell> ");
		char string[NAME_DIM] = "";
		fgets(string, NAME_DIM, stdin);

		if (string[strlen(string) - 1] == '\n') {
			string[strlen(string) - 1] = '\0';
		}

		if (!strcmp(string, "0")) {
			break;
		}

		char *command = "", *dir_n = "", *name = "", *word = "";

		if ((command = strtok(string, " ")) != NULL) {
			if ((dir_n = strtok(NULL, " ")) != NULL) {
				if ((name = strtok(NULL, " ")) != NULL) {
					word = strtok(NULL, " ");
				}
			}
		}

		mess_p.type = atol(dir_n) + 1;
		mess_p.done = 0;

		if (!strcmp(command, "list")) {
			mess_p.command = C_LIST;

			if (msgsnd(msgq_des, &mess_p, MSG_DIM, 0) == -1) {
				perror("msgsnd P->D");
				exit(1);
			}

			while (1) {
				if (msgrcv(msgq_des, &mess_p, MSG_DIM, 1, 0) == -1) {
					perror("msgrcv P<-D");
					exit(1);
				}

				if (mess_p.done) {
					break;
				}

				printf("%s\n", mess_p.string);
			}

			printf("\n");
		}
		else if (!strcmp(command, "size")) {
			mess_p.command = C_SIZE;
			strcpy(mess_p.name, name);

			if (msgsnd(msgq_des, &mess_p, MSG_DIM, 0) == -1) {
				perror("msgsnd P->D");
				exit(1);
			}

			if (msgrcv(msgq_des, &mess_p, MSG_DIM, 1, 0) == -1) {
				perror("msgrcv P<-D");
				exit(1);
			}

			printf("%s\n", mess_p.string);
			printf("\n");
		}
		else if (!strcmp(command, "search")) {
			mess_p.command = C_SEARCH;
			strcpy(mess_p.name, name);
			strcpy(mess_p.word, word);

			if (msgsnd(msgq_des, &mess_p, MSG_DIM, 0) == -1) {
				perror("msgsnd P->D");
				exit(1);
			}

			if (msgrcv(msgq_des, &mess_p, MSG_DIM, 1, 0) == -1) {
				perror("msgrcv P<-D");
				exit(1);
			}

			printf("%s\n", mess_p.string);
			printf("\n");
		}
	}

	mess_p.done = 1;

	for (int i = 0; i < argc - 1; i++) {
		mess_p.type = i + 2;

		if (msgsnd(msgq_des, &mess_p, MSG_DIM, 0) == -1) {
			perror("msgsnd P->D");
			exit(1);
		}

		wait(NULL);
	}

	msgctl(msgq_des, IPC_RMID, NULL);

	exit(0);
}
