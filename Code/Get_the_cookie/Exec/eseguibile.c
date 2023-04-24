#include <stdio.h>
#include <unistd.h>

//Compilo questo codice e uso il suo eseguibile per la exec() del file main.c

int main()
{
    printf("I'm extern process with pid : %d\n", getpid());
    return 0;
}
