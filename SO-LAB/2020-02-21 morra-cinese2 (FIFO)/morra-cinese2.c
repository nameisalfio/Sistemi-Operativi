#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MSG_DIM sizeof(msg) - sizeof(long)

typedef enum {S, C, F} M_TYPE;

typedef struct {
	long type;
	char move;
	char id;
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

void p_child(int msgq_des, int id) {
	srand(time(NULL) + id);
	msg mess_p;
	char *move[3] = {"sasso", "carta", "forbice"};

	while (1) {
		if (msgrcv(msgq_des,  &mess_p, MSG_DIM, id, 0) == -1) {
			fprintf(stderr, "msgrcv P%d\n", id);
			exit(1);
		}

		if (mess_p.done) {
			break;
		}

		mess_p.type = id + 2;
		mess_p.move = rand() % 3;
		mess_p.id = id;
		printf("P%d: mossa '%s'\n", id, move[mess_p.move]);

		if (msgsnd(msgq_des, &mess_p, MSG_DIM, 0) == -1) {
			fprintf(stderr, "msgsnd P%d\n", id);
			exit(1);
		}
	}

	exit(0);
}

void j_child(int msgq_des, const char *pathname, int matches) {
	msg mess_j;
	mess_j.done = 0;
	int match = 1;
	int moves[2];
	int fifofd;

	if ((fifofd = open(pathname, O_WRONLY)) == -1) {
		fprintf(stderr, "open %s\n", pathname);
		unlink(pathname);
		exit(1);
	}

	while (match <= matches) {
		printf("G:  inizio partita n.%d\n", match);

		for (int i = 0; i < 2; i++) {
			mess_j.type = i + 1;

			if (msgsnd(msgq_des, &mess_j, MSG_DIM, 0) == -1) {
				perror("msgsnd J");
				exit(1);
			}

			if (msgrcv(msgq_des,  &mess_j, MSG_DIM, i + 3, 0) == -1) {
				perror("msgrcv J");
				exit(1);
			}

			moves[i] = mess_j.move;
		}

		if (moves[0] == moves[1]) {
			printf("G:  partita n.%d patta e quindi da ripetere\n", match);
			continue;
		}
		else if ((moves[0] == C && moves[1] == S) || (moves[0] == S && moves[1] == F) || (moves[0] == F && moves[1] == C)) {
			printf("G:  partita n.%d vinta da P1\n", match);
			dprintf(fifofd, "1\n");
		}
		else if ((moves[0] == S && moves[1] == C) || (moves[0] == F && moves[1] == S) || (moves[0] == C && moves[1] == F)) {
			printf("G:  partita n.%d vinta da P2\n", match);
			dprintf(fifofd, "2\n");
		}

		match++;
		sleep(1);
	}

	mess_j.type = 1;
	mess_j.done = 1;

	if (msgsnd(msgq_des, &mess_j, MSG_DIM, 0) == -1) {
		perror("msgsnd J");
		exit(1);
	}

	mess_j.type = 2;

	if (msgsnd(msgq_des, &mess_j, MSG_DIM, 0) == -1) {
		perror("msgsnd J");
		exit(1);
	}

	dprintf(fifofd, "-1\n");

	exit(0);
}

void t_child(const char *pathname, int matches) {
	FILE *fifofd;

	if ((fifofd = fopen(pathname, "r")) == NULL) {
		perror("fopen");
		exit(1);
	}

	int match = 0;
	char winner[5];
	int wins[2] = {0, 0};

	while (fgets(winner, 5, fifofd)) {
		winner[strlen(winner) - 1] = '\0';

		if (!strcmp(winner, "-1")) {
			break;
		}

		if (!strcmp(winner, "1")) {
			wins[0]++;
		}
		else {
			wins[1]++;
		}

		if (++match == matches) {
			printf("T:  classifica finale: P1=%d P2=%d\n", wins[0], wins[1]);
		}
		else {
			printf("T:  classifica temporanea: P1=%d P2=%d\n", wins[0], wins[1]);
		}
	}

	if (wins[0] > wins[1]) {
		printf("T:  vincitore del torneo: P1\n");
	}
	else if (wins[1] > wins[0]) {
		printf("T:  vincitore del torneo: P2\n");
	}
	else {
		printf("T:  nessun vincitore del torneo\n");
	}

	fclose(fifofd);
	unlink(pathname);

	exit(0);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <numero-partite>\n", argv[0]);
		exit(1);
	}

	int matches = atoi(argv[1]);
	int msgq_des = init_msgq();
	const char *pathname = "/tmp/fifo";

	if (mkfifo(pathname, 0600) == -1) {
		fprintf(stderr, "mkfifo %s\n", pathname);
		exit(1);
	}

	// P
	for (int i = 0; i < 2; i++) {
		if (!fork()) {
			p_child(msgq_des, i + 1);
		}
	}
	// J
	if (!fork()) {
		j_child(msgq_des, pathname, matches);
	}
	// T
	if (!fork()) {
		t_child(pathname, matches);
	}

	for (int i = 0; i < 4; i++) {
		wait(NULL);
	}

	msgctl(msgq_des, IPC_RMID, NULL);

	exit(0);
}
