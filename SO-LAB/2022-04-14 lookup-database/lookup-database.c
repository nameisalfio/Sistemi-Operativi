#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define WORD_DIM 1024
#define LINE_DIM 2048
#define MSG1_DIM sizeof(msg1) - sizeof(long)
#define MSG2_DIM sizeof(msg2) - sizeof(long)

typedef struct {
	long type;
	char word[WORD_DIM];
	char id;
	char done;
} msg1;

typedef struct {
	long type;
	char word[WORD_DIM];
	int value;
	char id;
	char done;
} msg2;

typedef struct {
	char word[WORD_DIM];
	int value;
} db_entry;

typedef struct {
	int queries;
	int total;
} global_stats;

int init_msgq() {
	int msgq_des;

	if ((msgq_des = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
		perror("msgget");
		exit(1);
	}

	return msgq_des;
}

void in_child(int msgq_des, int id, char *argv) {
	FILE *fd;

	if ((fd = fopen(argv, "r")) == NULL) {
		fprintf(stderr, "fopen IN%d\n", id);
		exit(1);
	}

	msg1 mess_in;
	mess_in.type = 1;
	char word[WORD_DIM];
	int word_n = 0;

	while (fgets(word, WORD_DIM, fd)) {
		if (word[strlen(word) - 1] == '\n') {
			word[strlen(word) - 1] = '\0';
		}

		mess_in.id = id;
		mess_in.done = 0;
		strcpy(mess_in.word, word);

		if (msgsnd(msgq_des, &mess_in, MSG1_DIM, 0) == -1) {
			fprintf(stderr, "msgsnd IN%d->DB\n", id);
			exit(1);
		}

		printf("IN%d: inviata query n.%d '%s'\n", id, ++word_n, word);
	}

	mess_in.done = 1;

	if (msgsnd(msgq_des, &mess_in, MSG1_DIM, 0) == -1) {
		fprintf(stderr, "msgsnd IN%d->DB\n", id);
		exit(1);
	}

	fclose(fd);

	exit(0);
}

void db_child(int msgq_des[2], char *argv) {
	FILE *fd;

	if ((fd = fopen(argv, "r")) == NULL) {
		perror("fopen DB");
		exit(1);
	}

	char line[LINE_DIM];
	int lines_n = 0;

	while (fgets(line, LINE_DIM, fd)) {
		lines_n++;
	}

	printf("DB: letti n. %d record da file\n", lines_n);

	if (fseek(fd, 0, SEEK_SET) == -1) {
		perror("fseek");
		exit(1);
	}

	db_entry *db = malloc(sizeof(db_entry) * lines_n);

	for (int i = 0; i < lines_n; i++) {
		fgets(line, LINE_DIM, fd);
		char *word;
		char *value;

		if ((word = strtok(line, ":")) != NULL) {
			if (value = strtok(NULL, ":")) {
				strcpy(db[i].word, word);
				db[i].value = atoi(value);
			}
		}
	}

	fclose(fd);
	msg1 mess_in;
	msg2 mess_out;
	mess_out.type = 1;
	mess_out.done = 0;
	char done_counter = 0;

	while (1) {
		if (msgrcv(msgq_des[0], &mess_in, MSG1_DIM, 0, 0) == -1) {
			perror("msgrcv DB<-IN");
			exit(1);
		}

		if (mess_in.done) {
			done_counter++;

			if (done_counter == 2) {
				break;
			}

			continue;
		}

		char found = 0;

		for (int i = 0; i < lines_n; i++) {
			if (!strcmp(db[i].word, mess_in.word)) {
				strcpy(mess_out.word, mess_in.word);
				mess_out.value = db[i].value;
				mess_out.id = mess_in.id;
				found = 1;
				printf("DB: query '%s' da IN%d trovata con valore %d\n", mess_in.word, mess_in.id, db[i].value);
	
				if (msgsnd(msgq_des[1], &mess_out, MSG2_DIM, 0) == -1) {
					perror("msgsnd DB->OUT");
					exit(1);
				}

				break;
			}
		}

		if (!found) {
			printf("DB: query '%s' da IN%d non trovata\n", mess_in.word, mess_in.id);
		}
	}

	mess_out.done = 1;

	if (msgsnd(msgq_des[1], &mess_out, MSG2_DIM, 0) == -1) {
		perror("msgsnd DB->OUT");
		exit(1);
	}

	free(db);

	exit(0);
}

void out_child(int msgq_des) {
	msg2 mess_db;
	global_stats arr[2];

	for (int i = 0; i < 2; i++) {
		arr[i].queries = 0;
		arr[i].total = 0;
	}

	while (1) {
		if (msgrcv(msgq_des, &mess_db, MSG2_DIM, 0, 0) == -1) {
			perror("msgrcv OUT<-DB");
			exit(1);
		}

		if (mess_db.done) {
			break;
		}

		arr[mess_db.id - 1].queries++;
		arr[mess_db.id - 1].total += mess_db.value;
	}

	for (int i = 0; i < 2; i++) {
		printf("OUT: ricevuti n.%d valori validi per IN%d con totale %d\n", arr[i].queries, i + 1, arr[i].total);
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc != 4) {
		fprintf(stderr, "Usage: %s <db-file> <query-file-1> <query-file-2>\n", argv[0]);
		exit(1);
	}

	int msgq_des[2];

	for (int i = 0; i < 2; i++) {
		msgq_des[i] = init_msgq();
	}
	// IN
	for (int i = 0; i < 2; i++) {
		if (!fork()) {
			in_child(msgq_des[0], i + 1, argv[i + 2]);
		}
	}
	// DB
	if (!fork()) {
		db_child(msgq_des, argv[1]);
	}
	// OUT
	if (!fork()) {
		out_child(msgq_des[1]);
	}

	for (int i = 0; i < 4; i++) {
		wait(NULL);
	}

	for (int i = 0; i < 2; i++) {
		msgctl(msgq_des[i], IPC_RMID, NULL);
	}

	exit(0);
}
