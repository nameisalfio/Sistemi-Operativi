#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h> 

#define MODE 0660 // --> 0 110 110 000 

void handle_error(const char* msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char**argv)
{
    umask(0);
    int fd;
    const char w_buffer[BUFSIZ] = "Hello World";
    char r_buffer[BUFSIZ];

    if((fd = open("File.txt", O_CREAT | O_RDWR | O_TRUNC, MODE)) < 0) handle_error("Error open\n");

    if((write(fd, w_buffer, strlen(w_buffer))) < 0) handle_error("Error write\n");
    lseek(fd, 0, SEEK_SET);
    if((read(fd, r_buffer, BUFSIZ)) < 0) handle_error("Error read\n");

    printf("%s\n", r_buffer);

    close(fd);

    exit(EXIT_SUCCESS);
}
