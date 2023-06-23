#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/mman.h>

/*
    Scrivere un programma in linguaggio C che permetta di copiare un numero arbitrario di file regolari su una directory di destinazione preesistente.

    Il programma dovra' accettare una sintassi del tipo:

    $ homework-1 file1.txt path/file2.txt "nome con spazi.pdf" directory-destinazione
*/

void handle(const char* msg, int line) 
{
    fprintf(stderr, "Error at line %d : ", line);
    perror(msg);
    exit(EXIT_FAILURE);
}

char* create_path_dest(char* source, char* dest)
{
    char* path = malloc(PATH_MAX * sizeof(char));
    sprintf(path, "%s/%s", dest, basename(source));
    return path;
}

void copy(char* source, char* dest)
{
    int fs, fd;
    char *map_s, *map_d, *path;
    struct stat info_s, info_d;

    if((fs = open(source, O_RDONLY)) < 0) handle("open\n", __LINE__);
    if((fstat(fs, &info_s)) < 0) handle("sstat\n", __LINE__);
    if((map_s = mmap(NULL, info_s.st_size, PROT_READ, MAP_SHARED, fs, 0)) < 0) handle("mmap\n", __LINE__);
    if(close(fs) < 0) handle("close", __LINE__);

    path = create_path_dest(source, dest);  // costruisco il path di destinazione

    if((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, info_s.st_mode)) < 0) handle("open\n", __LINE__);
    if((map_d = mmap(NULL, strlen(map_s), PROT_WRITE, MAP_SHARED, fd, 0)) < 0) handle("mmap\n", __LINE__);
    if(close(fd) < 0) handle("close", __LINE__);
    
    memcpy(map_d, map_s, strlen(map_s));

    if (munmap(map_s, info_s.st_size) < 0) handle("munmap\n", __LINE__);
    if (munmap(map_d, info_s.st_size) < 0) handle("munmap\n", __LINE__);
    free(path);
}

int main(int argc, char* argv[])
{
    if(argc < 3) handle("argc\n", __LINE__);

    int n = argc-1;
    for(int i=1; i<n-1; i++)
        copy(argv[i], argv[n]);

    exit(EXIT_SUCCESS);
}