#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>

/*
Creare un programma palindrome-filter.c in linguaggio C che accetti invocazioni sulla riga di comando del tipo:

palindrome-filter <input-file>
Il programma dovrà fungere da filtro per selezionare, tra le parole in input, quelle che rappresentano una parola palindroma. L'input atteso è una lista 
di parole (una per riga) dal file specificato sulla riga di comando. L'output risultato della selezione verrà riversato sullo standard output.

Il programma, al suo avvio, creerà 3 thread R, P e W che avranno accesso ad una struttura dati condivisa a cui accederanno con mutua esclusione utilizzando 
un certo numero di semafori (minimo a scelta dello studente).

I ruoli dei 3 thread saranno i seguenti:

il thread R leggerà il file riga per riga e inserirà, ad ogni iterazione, la riga letta (parola) all'interno della struttura dati condivisa;
il thread P analizzerà, ad ogni iterazione, la parola inserita da R nella struttura dati; se la parola è palindroma, P dovrà passarla al thread W "svegliandolo";
il thread W, ad ogni "segnalazione" di P, scriverà sullo standard output la parola palindroma.
*/

#define handle(msg)\
do{\
    fprintf(stderr, "[%d] ", __LINE__);\
    perror(msg);\
    exit(EXIT_FAILURE);\
}while(false);

typedef enum {R, P, W} Thread_t;

typedef struct 
{
    char buffer[BUFSIZ];
    bool finish;
    sem_t sem[3];
}Shared;

typedef struct 
{
    pthread_t tid;
    char filename[FILENAME_MAX];
    Shared* shared;
}Thread_data;

bool is_palindroma(const char* string)
{
    int n = strlen(string);
    for(int i=0; i<n/2; i++)
        if(string[i] != string[n-i-1]) return false;
    return true;
}

void function_R(void* arg)
{
    Thread_data* td = (Thread_data*)arg;
    FILE* fp; 
    char buffer[BUFSIZ];

    if(!(fp = fopen(td->filename, "r"))) handle("fopen\n");

    while(fgets(buffer, BUFSIZ, fp))
    {
        buffer[strcspn(buffer, "\n")] = 0;
        if(sem_wait(&td->shared->sem[R])) handle("sem_wait\n");
        
        strcpy(td->shared->buffer, buffer);

        if(sem_post(&td->shared->sem[P])) handle("sem_post\n");
    }

    if(sem_wait(&td->shared->sem[R])) handle("sem_wait\n");

    td->shared->finish = true;
        
    if(sem_post(&td->shared->sem[P])) handle("sem_post\n");
    if(sem_post(&td->shared->sem[W])) handle("sem_post\n");

    if((fclose(fp))) handle("fclose\n");

    pthread_exit(NULL);
}

void function_P(void* arg)
{
    Thread_data* td = (Thread_data*)arg;
    while(true)
    {
        if(sem_wait(&td->shared->sem[P])) handle("sem_wait\n");

        if(td->shared->finish) break;

        if(is_palindroma(td->shared->buffer))
        {
            if(sem_post(&td->shared->sem[W])) handle("sem_post\n");
        }
        else
        {
            if(sem_post(&td->shared->sem[R])) handle("sem_post\n");
        }
    }
    pthread_exit(NULL);
}

void function_W(void* arg)
{
    Thread_data* td = (Thread_data*)arg;
    int idx = 0;
    while(true)
    {
        if(sem_wait(&td->shared->sem[W])) handle("sem_wait\n");

        if(td->shared->finish) break;

        if(sem_post(&td->shared->sem[R])) handle("sem_post\n");
        printf("%d)Word) %s\n", ++idx, td->shared->buffer);
    }
    pthread_exit(NULL);
}

void init(Shared* s)
{
    memset(&s->buffer, 0, BUFSIZ);
    s->finish = false;
    if(sem_init(&s->sem[R], 0, 1)) handle("sem_init\n");
    if(sem_init(&s->sem[P], 0, 0)) handle("sem_init\n");
    if(sem_init(&s->sem[W], 0, 0)) handle("sem_init\n");
}

void destroy(Shared* s)
{
    for(int i=0; i<3; i++)
        sem_destroy(&s->sem[i]);
    free(s);
}

int main(int argc, char*argv[])
{
    if(argc != 2) handle("argc\n");
    const char* filename = argv[1];

    Thread_data td[3];
    Shared* shared = malloc(sizeof(Shared));
    init(shared);
    
    for(int i=0; i<3; i++)
        td[i].shared = shared;
    
    strcpy(td[R].filename, filename);

    if(pthread_create(&td[R].tid, NULL, (void*)function_R, (void*)&td[R])) handle("pthread_create\n");
    if(pthread_create(&td[P].tid, NULL, (void*)function_P, (void*)&td[P])) handle("pthread_create\n");
    if(pthread_create(&td[W].tid, NULL, (void*)function_W, (void*)&td[W])) handle("pthread_create\n");

    for(int i=0; i<3; i++)
        pthread_join(td[i].tid, NULL);

    destroy(shared);

    exit(EXIT_SUCCESS);
}