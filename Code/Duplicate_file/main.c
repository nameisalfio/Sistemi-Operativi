// copia un file sorgente su uno destinazione usando gli stream

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

void handle_error(const char* msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char*argv[])
{
    FILE *src, *dst;
    char line[BUFSIZ];

    if(argc != 3)   handle_error("Error argc\n");

    if((src = fopen(argv[1], "r")) < 0) handle_error("Error fopen src\n");
    if((dst = fopen(argv[2], "w")) < 0) handle_error("Error fopen dst\n");

    while(fgets(line, BUFSIZ, src))
        fprintf(dst, "%s", line);

    fclose(src);
    fclose(dst);

    exit(EXIT_SUCCESS);
}