#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <ctype.h>

#define LETTERS_NUM 26
#define MSG_DIM sizeof(msg) - sizeof(long)

typedef struct {
	long type;
	int occurrences[LETTERS_NUM];
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

void t_child(int msgq_des, int id, char *argv) {
	int fd;

	if ((fd = open(argv, O_RDONLY)) == -1) {
		perror("open");
		exit(1);
	}

	struct stat sb;

	if (fstat(fd, &sb) == -1) {
		perror("fstat");
		exit(1);
	}

	if (!S_ISREG(sb.st_mode)) {
		fprintf(stderr, "%s non è un file regolare\n", argv);
		exit(1);
	}

	char *data;

	if ((data = (char *)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	close(fd);

	msg mess_t;
	mess_t.type = 1;
	mess_t.done = 1;

	for (int i = 0; i < LETTERS_NUM; i++) {
		mess_t.occurrences[i] = 0;
	}

	for (int i = 0; i < sb.st_size; i++) {
		if (tolower(data[i]) >= 'a' && tolower(data[i]) <= 'z') {
			mess_t.occurrences[tolower(data[i]) - 'a']++;
		}
	}

	printf("processo T-%d su file '%s':\n", id, argv);

	if (msgsnd(msgq_des, &mess_t, MSG_DIM, 0) == -1) {
		fprintf(stderr, "msgsnd T-%d\n", id);
		exit(1);
	}

	for (int i = 0; i < LETTERS_NUM; i++) {
		printf("%c:%d ", i + 'a', mess_t.occurrences[i]);
	}

	printf("\n");
	munmap(data, sb.st_size);

	exit(0);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <file-1> <file-2> <...>\n", argv[0]);
		exit(1);
	}

	int msgq_des = init_msgq();
	// T
	for (int i = 0; i < argc - 1; i++) {
		if (!fork()) {
			t_child(msgq_des, i + 1, argv[i + 1]);
		}
	}

	int global_occurrences[LETTERS_NUM];

	for (int i = 0; i < LETTERS_NUM; i++) {
		global_occurrences[i] = 0;
	}

	msg mess_p;
	char done_counter = 0;

	while (1) {
		if (msgrcv(msgq_des, &mess_p, MSG_DIM, 0, 0) == -1) {
			perror("msgrcv");
			exit(1);
		}

		for (int i = 0; i < LETTERS_NUM; i++) {
			global_occurrences[i] += mess_p.occurrences[i];
		}

		if (mess_p.done) {
			done_counter++;

			if (done_counter == argc - 1) {
				break;
			}
		}
	}

	for (int i = 0; i < argc - 1; i++) {
		wait(NULL);
	}

	int max = 0;
	printf("\nprocesso padre P:\n");

	for (int i = 0; i < LETTERS_NUM; i++) {
		if (global_occurrences[i] > global_occurrences[max]) {
			max = i;
		}

		printf("%c:%d ", i + 'a', global_occurrences[i]);
	}

	printf("\nlettera più utilizzata: '%c'\n", max + 'a');
	msgctl(msgq_des, IPC_RMID, NULL);

	exit(0);
}
