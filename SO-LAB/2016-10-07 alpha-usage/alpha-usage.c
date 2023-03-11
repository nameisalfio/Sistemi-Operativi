#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <string.h>
#include <linux/limits.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <ctype.h>

#define MSG_DIM sizeof(msg) - sizeof(long)

typedef enum {T_Base, T_S, T_A, T_P} T_MSG;

typedef struct {
	long type;
	char path[PATH_MAX];
	int total;
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

void s_child(int msgq_des, char *path, char base) {
	DIR *d;

	if ((d = opendir(path)) == NULL) {
		perror("open");
		exit(1);
	}

	struct dirent *dirent;
	msg mess_s;
	mess_s.done = 0;

	while (dirent = readdir(d)) {
		if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, "..")) {
			continue;
		}
		else if (dirent->d_type == DT_REG) {
			mess_s.type = T_A;
			sprintf(mess_s.path, "%s/%s", path, dirent->d_name);

			if (msgsnd(msgq_des, &mess_s, MSG_DIM, 0) == -1) {
				perror("msgsnd Scanner->Analyzer");
				exit(1);
			}

			printf("Scanner: %s/%s\n", path, dirent->d_name);

			if (msgrcv(msgq_des, &mess_s, MSG_DIM, T_S, 0) == -1) {
				perror("msgrcv Scanner<-Father");
				exit(1);
			}
		}
		else if (dirent->d_type == DT_DIR) {
			char tmp_path[PATH_MAX];
			sprintf(tmp_path, "%s/%s", path, dirent->d_name);
			s_child(msgq_des, tmp_path, 0);
		}
	}

	closedir(d);

	if (base) {
		mess_s.type = T_A;
		mess_s.done = 1;

		if (msgsnd(msgq_des, &mess_s, MSG_DIM, 0) == -1) {
			perror("msgsnd Scanner->Analyzer");
			exit(1);
		}

		exit(0);
	}
}

void a_child(int msgq_des) {
	msg mess_a;
	struct stat sb;

	while (1) {
		if (msgrcv(msgq_des, &mess_a, MSG_DIM, T_A, 0) == -1) {
			perror("msgrcv Analyzer<-Scanner");
			exit(1);
		}

		if (mess_a.done) {
			break;
		}

		int fd;

		if ((fd = open(mess_a.path, O_RDONLY)) == -1) {
			perror("open");
			exit(1);
		}

		if (fstat(fd, &sb) == -1) {
			perror("fstat");
			exit(1);
		}

		if (!S_ISREG(sb.st_mode)) {
			fprintf(stderr, "%s non è un file regolare\n", mess_a.path);
			exit(1);
		}

		char *data;

		if ((data = (char *)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
			perror("mmap");
			exit(1);
		}

		int total = 0;

		for (int i = 0; i < sb.st_size; i++) {
			if (tolower(data[i]) >= 'a' && tolower(data[i]) <= 'z') {
				total++;
			}
		}

		munmap(data, sb.st_size);
		mess_a.type = T_P;
		mess_a.total = total;

		if (msgsnd(msgq_des, &mess_a, MSG_DIM, 0) == -1) {
			perror("msgsnd Analyzer->Father");
			exit(1);
		}

		printf("Analyzer: %s %d\n", mess_a.path, total);
	}

	mess_a.type = T_P;

	if (msgsnd(msgq_des, &mess_a, MSG_DIM, 0) == -1) {
		perror("msgsnd Analyzer->Father");
		exit(1);
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		argc = 2;
		argv[1] = ".";
	}
	else if (argc > 2) {
		fprintf(stderr, "Usage: %s [directory]\n", argv[0]);
		exit(1);
	}

	int msgq_des = init_msgq();
	// Scanner
	if (!fork()) {
		s_child(msgq_des, argv[1], 1);
	}
	// Analyzer
	if (!fork()) {
		a_child(msgq_des);
	}

	msg mess_p;
	int total = 0;

	while (1) {
		if (msgrcv(msgq_des, &mess_p, MSG_DIM, T_P, 0) == -1) {
			perror("msgrcv Father<-Analyzer");
			exit(1);
		}

		if (mess_p.done) {
			break;
		}

		mess_p.type = T_S;
		total += mess_p.total;

		if (msgsnd(msgq_des, &mess_p, MSG_DIM, 0) == -1) {
			perror("msgsnd Father->Scanner");
			exit(1);
		}
	}

	printf("Padre: totale di %d caratteri alfabetici\n", total);

	for (int i = 0; i < 2; i++) {
		wait(NULL);
	}

	msgctl(msgq_des, IPC_RMID, NULL);

	exit(0);
}
