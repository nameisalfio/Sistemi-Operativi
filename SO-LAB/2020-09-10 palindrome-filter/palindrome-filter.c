#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <string.h>

#define WORD_DIM 2048

void r_child(int pipefd, char *argv) {
	int fd;

	if ((fd = open(argv, O_RDONLY)) == -1) {
		perror("fopen");
		exit(1);
	}

	struct stat sb;

	if (fstat(fd, &sb) == -1) {
		perror("fstat");
		exit(1);
	}

	if (!S_ISREG(sb.st_mode)) {
		fprintf(stderr, "File %s isn't regular!", argv);
		exit(1);
	}

	char *data;

	if ((data = (char *)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == (char *)MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	close(fd);
	char word[WORD_DIM];
	int j = 0;

	for (int i = 0; i < sb.st_size; i++) {
		word[j++] = data[i];

		if (data[i] == '\n') {
			word[j - 1] = '\0';
			j = 0;
			dprintf(pipefd, "%s\n", word);
		}
	}

	dprintf(pipefd, "-1\n");

	exit(0);
}

void w_child(int pipefd) {
	FILE *pfd;

	if ((pfd = fdopen(pipefd, "r")) == NULL) {
		perror("fdopen");
		exit(1);
	}

	char word[WORD_DIM];

	while (1) {
		fgets(word, WORD_DIM, pfd);

		if (!strcmp(word, "-1\n")) {
			break;
		}

		if (word[strlen(word) - 1] == '\n') {
			word[strlen(word) - 1] = '\0';
		}

		printf("%s\n", word);
	}

	exit(0);
}

void p_father(int pipefd1, int pipefd2) {
	FILE *pfd;

	if ((pfd = fdopen(pipefd1, "r")) == NULL) {
		perror("fdopen");
		exit(1);
	}

	char word[WORD_DIM];

	while (1) {
		fgets(word, WORD_DIM, pfd);

		if (!strcmp(word, "-1\n")) {
			break;
		}

		if (word[strlen(word) - 1] == '\n') {
			word[strlen(word) - 1] = '\0';
		}

		char pal = 1;

		for (int i = 0, j = strlen(word) - 1; i < strlen(word) / 2; i++, j--) {
			if (word[i] != word[j]) {
				pal = 0;
			}
		}

		if (pal) {
			dprintf(pipefd2, "%s\n", word);
		}
	}

	dprintf(pipefd2, "-1\n");
	fclose(pfd);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
	}
	int pipefd1[2];
	int pipefd2[2];

	if (pipe(pipefd1) == -1) {
		perror("pipe");
		exit(1);
	}

	if (pipe(pipefd2) == -1) {
		perror("pipe");
		exit(1);
	}
	// R
	if (!fork()) {
		close(pipefd1[0]);
		close(pipefd2[0]);
		close(pipefd2[1]);
		r_child(pipefd1[1], argv[1]);
	}
	// W
	if (!fork()) {
		close(pipefd1[0]);
		close(pipefd1[1]);
		close(pipefd2[1]);
		w_child(pipefd2[0]);
	}

	close(pipefd1[1]);
	close(pipefd2[0]);
	p_father(pipefd1[0], pipefd2[1]);

	for (int i = 0; i < 3; i++) {
		wait(NULL);
	}

	close(pipefd1[0]);
	close(pipefd2[1]);

	exit(0);
}
