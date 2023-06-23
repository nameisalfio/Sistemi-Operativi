#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void handle_error(const char* msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

void fixed_sequence2()
{
    execlp("sh", "sh", "-c", "whoami && cal 6 2023 && not_exist", NULL);
}

void fixed_sequence()
{
    if(!fork())
        execlp("whoami", "whoami", NULL);
    else
        wait(NULL);
    
    if(!fork())
        execlp("cal", "cal", "6", "2023", NULL);
    else
        wait(NULL);

    if(!fork())
        execlp("not_exist", "not_exist", NULL);
    else
        wait(NULL);

}

void passed_sequence(int argc, char**argv)     
{
    char* command_args[argc];
    char* pathname = strdup(argv[1]);
        
    for (int i = 0; i < argc - 1; i++) 
        command_args[i] = strdup(argv[i + 1]);
    command_args[argc - 1] = NULL; 
    execvp(pathname, command_args);
}

int main(int argc, char**argv)
{
    if(argc == 1) 
        fixed_sequence2();
    else
        passed_sequence(argc, argv);

    exit(EXIT_SUCCESS);
}