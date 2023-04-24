#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

/*
SYSTEM CALL FORK
    La system call fork() è una chiamata di sistema che crea un nuovo processo identico al processo chiamante, chiamato processo figlio. 
    La creazione del processo figlio viene effettuata mediante la duplicazione dello spazio di indirizzamento del processo chiamante, 
    compresi i dati, il codice e lo stato del processo.
    Dopo la creazione del processo figlio, entrambi i processi continuano ad eseguire il proprio codice in modo indipendente. 
    Il processo figlio riceve un valore di ritorno 0 dalla chiamata alla fork(), mentre il processo padre riceve il PID (Process IDentifier) del
    processo figlio. Questo PID può essere utilizzato dal processo padre per interagire con il processo figlio, ad esempio per inviare segnali o 
    per attendere che il processo figlio termini l'esecuzione.

PROCESSO ORFANO
    Un processo orfano è un processo figlio il cui padre termina la propria esecuzione ma senza aspettare che termini l'esecuzione del figlio.
    In questo caso il processo orfano viene "adottato" dal peocesso INIT che diventa il suo nuovo processo padre. 

    NB il processo INIT ha pid = 1 ed è l'unico processo senza padre

    Ottengo il processo orfano eseguendo e visualizzando i processi con " -o pid,ppid,command":

    ./main
    ps -o pid,ppid,command
    

PROCESSO ZOMBIE
    Un processo zombie è un processo genitore che ha un esecuzione molto lunga, a differenza del figlio che ha un esecuzione molto breve e rimane
    un processo terminato che rimane ad aspettare che il padre lo rilevi.

    Ottengo il processo zombie eseguendo in background e visualizzando i processi :

    ./main &
    ps
*/

int main()
{
    pid_t pid;  //process id

    printf("Prima della fork:\tpid = %d\tpid del genitore = %d\n", getpid(), getppid());
    
    if((pid = fork()) < 0)
        printf("Error during fork()");

    else if(pid == 0)   //Figlio
    {
        //sleep(5); //Orfano
        printf("\n[Figlio]\nValore della fork = %d\n", pid);
        printf("pid = %d\tpid del genitore = %d\n", getpid(), getppid());
    }

    else //Genitore
    {
        //sleep(5); //Zombie
        printf("\n[Genitore]\nValore della fork = %d\n", pid);
        printf("pid = %d\tpid del genitore = %d\tpid del figlio = %d\n", getpid(), getppid(), pid);
    }

    return 0;
}