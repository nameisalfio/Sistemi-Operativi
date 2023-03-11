#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <ctype.h>

#define WORD_DIM 1024

void r_child(const char *pathname1, const char *pathname2, char input, char *argv) {
	int fd;

	if (input) {
		if ((fd = open(argv, O_RDONLY)) == -1) {
			perror("open");
			exit(1);
		}
	}

	struct stat sb;
	char *data;

	if (input) {
		if (fstat(fd, &sb) == -1) {
			perror("fstat");
			exit(1);
		}

		if (S_ISREG(sb.st_mode) == -1) {
			fprintf(stderr, "%s non è un file regolare\n", argv);
			unlink(pathname1);
			unlink(pathname2);
			exit(1);
		}

		if ((data = (char *)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
			perror("mmap");
			unlink(pathname1);
			unlink(pathname2);
			exit(1);
		}
	}

	int pfd;

	if ((pfd = open(pathname1, O_WRONLY)) == -1) {
		perror("open FIFO R");
		unlink(pathname1);
		unlink(pathname2);
		exit(1);
	}

	char word[WORD_DIM];
	int j = 0;

	if (input) {
		for (int i = 0; i < sb.st_size; i++) {
			word[j++] = data[i];

			if (data[i] == '\n') {
				word[j - 1] = '\0';
				dprintf(pfd, "%s\n", word);
				strcpy(word, "");
				j = 0;
			}
		}
	}
	else {
		while (fgets(word, WORD_DIM, stdin)) {
			if (word[strlen(word) - 1] == '\n') {
				word[strlen(word) - 1] = '\0';
			}

			if (!strcmp(word, "-1")) {
				break;
			}

			dprintf(pfd, "%s\n", word);
		}
	}

	dprintf(pfd, "-1\n");

	if (input) {
		munmap(data, sb.st_size);
		close(fd);
	}

	close(pfd);

	exit(0);
}

void w_child(const char *pathname) {
	FILE *fd;

	if ((fd = fopen(pathname, "r")) == NULL) {
		perror("fopen FIFO W");
		unlink(pathname);
		exit(1);
	}

	char word[WORD_DIM];

	while (1) {
		fgets(word, WORD_DIM, fd);

		if (!strcmp(word, "-1\n")) {
			break;
		}

		if (word[strlen(word) - 1] == '\n') {
			word[strlen(word) - 1] = '\0';
		}

		printf("%s\n", word);
	}

	fclose(fd);

	exit(0);
}

int main(int argc, char **argv) {
	char input = 1;

	if (argc > 2) {
		fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
		exit(1);
	}
	else if (argc < 2) {
		argc = 2;
		argv[1] = "";
		input = 0;
	}

	const char *pathname1 = "/tmp/fifo1";
	const char *pathname2 = "/tmp/fifo2";

	if (mkfifo(pathname1, 0600) == -1) {
		fprintf(stderr, "mkfifo %s\n", pathname1);
		exit(1);
	}

	if (mkfifo(pathname2, 0600) == -1) {
		fprintf(stderr, "mkfifo %s\n", pathname2);
		exit(1);
	}
	// R
	if (!fork()) {
		r_child(pathname1, pathname2, input, argv[1]);
	}
	// W
	if (!fork()) {
		w_child(pathname2);
	}

	FILE *pfd;

	if ((pfd = fopen(pathname1, "r")) == NULL) {
		perror("fopen FIFO1 P");
		unlink(pathname1);
		unlink(pathname2);
		exit(1);
	}

	int pfd2;

	if ((pfd2 = open(pathname2, O_WRONLY)) == -1) {
		perror("open FIFO2 P");
		unlink(pathname1);
		unlink(pathname2);
		exit(1);
	}

	char word[WORD_DIM];

	while (1) {
		fgets(word, WORD_DIM, pfd);

		if (word[strlen(word) - 1] == '\n') {
			word[strlen(word) - 1] = '\0';
		}

		if (!strcmp(word, "-1")) {
			break;
		}

		char palindrome = 1;

		for (int i = 0, j = strlen(word) - 1; i < strlen(word) / 2; i++, j--) {
			if (tolower(word[i]) != tolower(word[j])) {
				palindrome = 0;
			}
		}

		if (palindrome) {
			dprintf(pfd2, "%s\n", word);
		}
	}

	dprintf(pfd2, "-1\n");

	for (int i = 0; i < 2; i++) {
		wait(NULL);
	}

	close(pfd2);
	fclose(pfd);
	unlink(pathname1);
	unlink(pathname2);

	exit(0);
}
