#include <stdio.h>
#include <unistd.h>

/*
EXEC
	La system call exec() è una chiamata di sistema utilizzata per eseguire un nuovo programma in un processo esistente. Quando un processo chiama 
    la exec(), viene sostituito il codice del processo corrente con il codice del programma specificato nella chiamata alla exec(). 
    In questo modo, il programma sostituisce l'esecuzione del processo corrente, utilizzando lo stesso spazio di indirizzamento.

    Ci sono diverse varianti della exec(), tra cui:

    execl(): questa chiamata di sistema accetta un elenco di argomenti, dove l'ultimo argomento deve essere NULL. Il nuovo programma viene eseguito 
             con gli argomenti specificati.

    execv(): questa chiamata di sistema accetta un array di puntatori a stringhe che rappresentano gli argomenti del programma. L'ultimo elemento 
             dell'array deve essere NULL.

    execle(): simile alla execl(), ma permette di specificare anche l'ambiente di esecuzione del nuovo programma.

    execvp(): simile alla execv(), ma cerca il programma specificato nell'elenco di directory presenti nella variabile di ambiente PATH.

    NB Anche dopo la sostituzione il pid rimane invariato
*/
/*
int main()
{
    printf("I'm process with pid : %d\n", getpid());
    fflush(stdout);
    puts("\t---I'm trying to execute some external code---");

    execl("./eseguibile", "./eseguibile", NULL);
    
    puts("Code substitution failed!");  //Se la exec() va a buon termine non viene eseguita

    return 0;
}
*/

int main()
{
    char command[] = {"sudo apt-get update && sudo apt-get upgrade"};

    printf("I'm process with pid : %d\n", getpid());
    printf("\n---I'm trying to execute the command '%s'---\n\n", command);

    execl("/bin/bash", "bash", "-c", command, NULL);
    
    puts("Code substitution failed!");  //Se la exec() va a buon termine non viene eseguita

    return 0;
}