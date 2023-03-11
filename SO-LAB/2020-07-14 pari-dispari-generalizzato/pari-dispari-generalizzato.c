#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include <sys/wait.h>
#include <string.h>

#define MSG_DIM sizeof(msg) - sizeof(long)

typedef struct {
	long type;
	char id;
	char move;
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
	msg mess_c;
	srand(time(NULL) + id);

	while (1) {
		if (msgrcv(msgq_des, &mess_c, MSG_DIM, id + 1, 0) == -1) {
			fprintf(stderr, "msgrcv P%d\n", id);
			exit(1);
		}

		if (mess_c.done) {
			break;
		}

		mess_c.type = id + 8;
		mess_c.id = id;
		mess_c.move = rand() % 10;

		if (msgsnd(msgq_des, &mess_c, MSG_DIM, 0) == -1) {
			perror("msgsnd J");
			exit(1);
		}		
	}

	exit(0);
}

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <n=numero-giocatori> <m=numero-partite>\n", argv[0]);
		exit(1);
	}

	int players = atoi(argv[1]);
	int matches = atoi(argv[2]);
	

	if (players < 2 || players > 6 || matches < 1) {
		fprintf(stderr, "Usage: %s 2 <= <n=numero-giocatori> <= 6 <m=numero-partite> >= 1\n", argv[0]);
		exit(1);
	}

	int msgq_des = init_msgq();

	for (int i = 0; i < players; i++) {
		if (!fork()) {
			p_child(msgq_des, i);
		}
	}

	char moves[players];
	char winners[players];

	for (int i = 0; i < players; i++) {
		winners[i] = 0;
	}

	int won_matches = 0;
	char winner;
	int match_n = 0;
	msg mess_j;
	mess_j.done = 0;

	while (won_matches < matches) {
		printf("J:  inizio partita n.%d\n", ++match_n);
		winner = 0;

		for (int i = 0; i < players; i++) {
			mess_j.type = i + 1;

			if (msgsnd(msgq_des, &mess_j, MSG_DIM, 0) == -1) {
				perror("msgsnd J");
				exit(1);
			}

			if (msgrcv(msgq_des, &mess_j, MSG_DIM, i + 8, 0) == -1) {
				perror("msgsnd J");
				exit(1);
			}

			printf("P%d: mossa %d\n", mess_j.id, mess_j.move);
			moves[mess_j.id] = mess_j.move;
		}

		char equal = 0;

		for (int i = 0; i < players - 1 && !equal; i++) {
			for (int j = i + 1; j < players && !equal; j++) {
				if (moves[i] == moves[j]) {
					equal = 1;
					printf("J: partita n.%d patta e quindi da ripetere\n", match_n--);
				}
			}
		}

		if (equal) {
			continue;
		}

		for (int i = 0; i < players; i++) {
			winner += moves[i];
		}

		winner %= players;
		winners[winner]++;

		printf("J: partita n.%d vinta da P%d\n", match_n, winner);

		won_matches++;
	}

	mess_j.done = 1;

	for (int i = 0; i < players; i++) {
		mess_j.type = i + 1;

		if (msgsnd(msgq_des, &mess_j, MSG_DIM, 0) == -1) {
			perror("msgsnd J");
			exit(1);
		}

		wait(NULL);
	}

	msgctl(msgq_des, IPC_RMID, NULL);

	char buff[50] = "";
	int max = 0;

	for (int i = 0; i < players; i++) {
		char s[10];
		sprintf(s, " P%d=%d", i, winners[i]);
		strcat(buff, s);

		if (winners[i] > winners[max]) {
			max = i;
		}
	}

	printf("J: classifica finale:%s\n", buff);

	int wins = winners[max];
	char equal = 0;

	for (int i = 0; i < players && equal < 2; i++) {
		if (winners[i] == wins) {
			equal++;
		}
	}

	if (equal < 2) {
		printf("J: vincitore del torneo: P%d\n", max);
	}

	exit(0);
}
