#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define WORD_DIM 1024

void r_child(int pipefd, char *argv) {
	FILE *fd;

	if ((fd = fopen(argv, "r")) == NULL) {
		perror("fopen Reader");
		exit(1);
	}

	char word[WORD_DIM];

	while (fgets(word, WORD_DIM, fd)) {
		if (word[strlen(word) - 1] == '\n') {
			word[strlen(word) - 1] = '\0';
		}

		dprintf(pipefd, "%s\n", word);
	}

	dprintf(pipefd, "-1\n");
	fclose(fd);
	close(pipefd);

	exit(0);
}

void f_child(int pipefd, const char *pathname, char *filter, char i, char v) {
	FILE *pfd;

	if ((pfd = fdopen(pipefd, "r")) == NULL) {
		perror("fdopen");
		exit(1);
	}

	int fifofd;

	if ((fifofd = open(pathname, O_WRONLY)) == -1) {
		perror("open");
		unlink(pathname);
		exit(1);
	}

	char word[WORD_DIM];

	while (1) {
		fgets(word, WORD_DIM, pfd);

		if (!strcmp(word, "-1\n")) {
			break;
		}

		if (!i && !v) {
			if (strstr(word, filter)) {
				dprintf(fifofd, "%s", word);
			}
		}
		else if (!i && v) {
			if (!strstr(word, filter)) {
				dprintf(fifofd, "%s", word);
			}
		}
		else if (i && !v) {
			if (strcasestr(word, filter)) {
				dprintf(fifofd, "%s", word);
			}
		}
		else if (i && v) {
			if (!strcasestr(word, filter)) {
				dprintf(fifofd, "%s", word);
			}
		}
	}

	dprintf(fifofd, "-1\n");
	fclose(pfd);
	close(pipefd);
	close(fifofd);

	exit(0);
}
void w_child(const char *pathname) {
	FILE *fifofd;

	if ((fifofd = fopen(pathname, "r")) == NULL) {
		perror("fopen Father");
		unlink(pathname);
		exit(1);
	}

	char word[WORD_DIM];

	while (1) {
		fgets(word, WORD_DIM, fifofd);

		if (!strcmp(word, "-1\n")) {
			break;
		}

		printf("%s", word);
	}

	fclose(fifofd);
	unlink(pathname);
	exit(0);
}

int main(int argc, char **argv) {
	if (argc < 2 || argc > 5) {
		fprintf(stderr, "Usage: %s [-i] [-v] <word> [file]\n", argv[0]);
		exit(1);
	}

	int word_p = 1, file_p = 2;
	char i = 0, v = 0;

	if (!strcmp(argv[1], "-i") || !strcmp(argv[1], "-v")) {
		word_p++;
		file_p++;

		if (!strcmp(argv[1], "-i")) {
			i = 1;
		}
		else {
			v = 1;
		}
	}

	if (!strcmp(argv[2], "-i") || !strcmp(argv[2], "-v")) {
		word_p++;
		file_p++;

		if (!strcmp(argv[2], "-i")) {
			i = 1;
		}
		else {
			v = 1;
		}
	}

	int pipefd[2];

	if (pipe(pipefd) == -1) {
		perror("pipe");
		exit(1);
	}
	// Reader
	if (!fork()) {
		close(pipefd[0]);
		r_child(pipefd[1], argv[file_p]);
	}

	const char *pathname = "/tmp/fifo";

	if (mkfifo(pathname, 0600) == -1) {
		fprintf(stderr, "mkfifo %s\n", pathname);
		exit(1);
	}
	// Filterer
	if (!fork()) {
		close(pipefd[1]);
		f_child(pipefd[0], pathname, argv[word_p], i, v);
	}
	// Writer
	if (!fork()) {
		close(pipefd[0]);
		close(pipefd[1]);
		w_child(pathname);
	}

	for (int i = 0; i < 3; i++) {
		wait(NULL);
	}

	for (int i = 0; i < 2; i++) {
		close(pipefd[i]);
	}

	exit(0);
}
