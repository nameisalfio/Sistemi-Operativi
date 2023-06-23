#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

void handle_error(const char* msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int main()
{
    int fd;
    char* map;
    struct stat info;
    off_t size;

    umask(0);
    if((fd = open("File.txt", O_RDONLY, 0660)) < 0) handle_error("Error open\n");

    if((fstat(fd, &info)) < 0) handle_error("Error \n");
    size = info.st_size;

    if((map = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) handle_error("Error mmap\n");

    if((close(fd)) < 0) handle_error("Error close\n");

    puts("File mappato : ");
    printf("%.*s", (int)size, map);

    if((munmap(map, size)) < 0) handle_error("Error munmap\n");

    exit(EXIT_SUCCESS);
}
