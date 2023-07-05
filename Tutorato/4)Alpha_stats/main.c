#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>
#include <ctype.h>

/*
    Creare un programma alpha-stats.c in linguaggio C che accetti invocazioni sulla riga di comando del tipo

    alpha-stats <file.txt>
    Il programma sostanzialmente leggerà il file di testo indicato e riporterà il numero di occorrenze riscontrate per tutte le lettere dell'alfabeto inglese.

    Il thread padre P, creerà due thread AL e MZ. Tutti i thread condivideranno una struttura dati condivisa contenente

    c: un char/byte;
    stats: un array di 26 unsigned long inizialmente posti a zero;
    altri elementi ritenuti utili/necessari.

    Per coordinarsi, i thread dovranno usare un certo numero di semafori (minimo a scelta dello studente).

    Il thread P, dopo le operazioni preliminari sopra indicate, leggerà il contenuto del file usando la mappatura dei file in memoria (ogni altro 
    metodo non sarà considerato corretto) e per ogni carattere alfabetico (a-z o A-Z) riscontrato provvederà a depositare in c la sua versione minuscola 
    (a-z) e ad attivare, rispettivamente, il thread AL o MZ a secondo dell'intervallo di competenza. Il thread AL (o MZ) una volta attivato provvederà ad 
    aggiornare il contatore corrispondente alla lettera ricevuta presente nell'array stats.

    Dopo che tutte le lettere del file saranno state "analizzate" e le statistiche aggiornate, allora il thread P provvederà a stampare a video le statistiche 
    presenti all'interno di stats e tutti i thread termineranno spontaneamente.

    Tutte le strutture dati utilizzate dovranno essere ripulite correttamente all'uscita.
*/

typedef enum {AL, MZ, P} Thread;

void handle(const char* msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

typedef struct 
{
    char c;
    unsigned long stats[26];
    bool finish;
    sem_t sem[3];
}Shared;   

typedef struct 
{
    pthread_t tid;
    Thread thread_n;
    Shared* shared;
}Thread_data;

void init_shared(Shared* sh)
{
    memset(sh->stats, 0, sizeof(sh->stats));
    sh->finish = false;
    if(sem_init(&sh->sem[P], 0, 1)) handle("Error sem_init\n");
    if(sem_init(&sh->sem[AL], 0, 0)) handle("Error sem_init\n");
    if(sem_init(&sh->sem[MZ], 0, 0)) handle("Error sem_init\n");
}

void destroy_shared(Shared* sh)
{
    if(sem_destroy(&sh->sem[P])) handle("Error sem_destroy\n");
    if(sem_destroy(&sh->sem[AL])) handle("Error sem_destroy\n");
    if(sem_destroy(&sh->sem[MZ])) handle("Error sem_destroy\n");
    free(sh);
}

void stats(void* arg)
{
    Thread_data* td = (Thread_data*)arg;
    while(true)
    {
        if(sem_wait(&td->shared->sem[td->thread_n])) handle("Error sem_wait\n");
        
        if(td->shared->finish) break;

        td->shared->stats[td->shared->c - 'a']++; 

        if(sem_post(&td->shared->sem[P])) handle("Error sem_post\n");
    }
    pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
    if(argc != 2) handle("Error argc\n");
    int fd;
    char* map;
    struct stat info;
    const char* filename = argv[1];

    Thread_data td[2];
    Shared* sh = malloc(sizeof(Shared));
    init_shared(sh);
    
    for(int i=0; i<2; i++)
    {
        td[i].thread_n = i;
        td[i].shared = sh;
        if(pthread_create(&td[i].tid, 0, (void*)stats, &td[i])) handle("Error pthread_create\n");
    }

    if((fd = open(filename, O_RDONLY)) < 0) handle("Error fopen\n");
    if((fstat(fd, &info)) < 0) handle("Error fstat\n");
    if((map = mmap(NULL, info.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) handle("Error mmap\n");
    if((close(fd)) < 0) handle("Error close\n");

    int i=0;
    while(i < info.st_size)
    {
        if(((map[i] >= 'a' && map[i] <= 'z') || (map[i] >= 'A' && map[i] <= 'Z')))
        {
            if(sem_wait(&sh->sem[P])) handle("Error sem_wait\n");

            sh->c = tolower(map[i]);
            if(sh->c <= 'l')
            {
                if(sem_post(&sh->sem[AL])) handle("Error sem_post\n");
            }   
            else
                if(sem_post(&sh->sem[MZ])) handle("Error sem_post\n");

            i++;
        }
        else
            i++;
    }

    if(sem_wait(&sh->sem[P])) handle("Error sem_wait\n");
    sh->finish = true;
    if(sem_post(&sh->sem[AL])) handle("Error sem_post\n");
    if(sem_post(&sh->sem[MZ])) handle("Error sem_post\n");

    for(int i=0; i<2; i++)
        if(pthread_join(td[i].tid, NULL)) handle("Error pthread_create\n");

    printf("Stats\n");
    for(int i=0; i<26; i++)
        printf("%c: %lu ", 'a'+i, sh->stats[i]);
    printf("\n");

    munmap(map, info.st_size);
    exit(EXIT_SUCCESS);
}