#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

void handle_error(const char* msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int main()
{
    int fd;
    const char* msg = "Hello World!";
    int len = strlen(msg);
    char row[len+1];

    fd = creat("file.txt", S_IRUSR | S_IWUSR);
    if(fd < 0)
        handle_error("Error create file\n");

    fd = open("file.txt", O_RDWR);
    if(fd < 0)
        handle_error("Error opening file\n");

    size_t wf = write(fd, msg, len);
    if(wf < 0)
        handle_error("Error writing file\n");

    // Riposiziona il puntatore del file all'inizio del file
    if(lseek(fd, 0, SEEK_SET) < 0)
        handle_error("Error lseek\n");

    size_t rf = read(fd, row, len);
    if(rf < 0)
        handle_error("Error reading file\n");

    row[rf] = 0;

    printf("Message : %s\n", row);

    close(fd);

    return 0;
}