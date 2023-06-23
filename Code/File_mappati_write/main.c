#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

void handle_error(const char* msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int main()
{
    int fd;
    char* map;
    const char* str = "Hello World";
    off_t size = strlen(str);

    umask(0);
    if ((fd = open("File.txt", O_RDWR | O_CREAT | O_TRUNC, 0660)) < 0) handle_error("Error open\n");

    if (ftruncate(fd, size) < 0) handle_error("Error ftruncate\n");

    if ((map = mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) handle_error("Error mmap\n");
    if (close(fd) < 0) handle_error("Error close\n");

    memcpy(map, str, strlen(str));
    if (msync(map, size, MS_ASYNC) < 0) handle_error("Error msync\n");

    if (munmap(map, size) < 0) handle_error("Error munmap\n");

    exit(EXIT_SUCCESS);
}