#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <libgen.h>

/*
    Scrivere un programma in linguaggio C che permetta di copiare un numero arbitrario di file regolari su una directory di destinazione preesistente.

    Il programma dovra' accettare una sintassi del tipo:

    $ homework-1 file1.txt path/file2.txt "nome con spazi.pdf" directory-destinazione
*/

void handle_error(char* msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

char* create_dest_path(char* source, char* dest)
{
    char* path = malloc(PATH_MAX * sizeof(char));
    sprintf(path, "%s/%s", dest, basename(source));
    return path;
}

void copy(char* source, char* dest) 
{
    FILE* fp_source;
    FILE* fp_dest;
    int size;
    char buffer[BUFSIZ];

    if (!(fp_source = fopen(source, "r"))) handle_error(source);

    char* path = create_dest_path(source, dest);    //crea il path di destinazione

    if (!(fp_dest = fopen(path, "w"))) handle_error("Error opening destination file");

    while((size = fread(buffer, 1, BUFSIZ, fp_source))) 
        if (fwrite(buffer, 1, size, fp_dest) != size) handle_error("Error write");
        
    fclose(fp_source);
    fclose(fp_dest);
    free(path);
}

int main(int argc, char* argv[])
{
    if(argc < 3) handle_error("Error argc\n");

    int n = argc - 1;
    for(int i=1; i<n; i++)
        copy(argv[i], argv[n]);

    exit(EXIT_SUCCESS);
}