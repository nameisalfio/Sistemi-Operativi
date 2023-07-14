#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <libgen.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <dirent.h>

/*
    Creare un programma file-size.c in linguaggio C che accetti invocazioni sulla riga di comando del tipo:

    file-size <dir-1> <dir-2> ... <dir-*n*>
    Il programma dovrà determinare la dimensione totale in byte dei file regolari direttamente contenuti all'interno delle cartelle indicate (senza ricorsione).

    Al suo avvio il programma creerà n+1 thread:

    un thread DIR-i che si occuperanno di scansionare la cartella assegnata alla ricerca di file regolari direttamente contenuti in essa (no ricorsione);
    un thread STAT che si occuperà di determinare la dimensione di ogni file regolare individuato.

    - Gli n thread DIR-i agiranno in parallelo e inseriranno, per ogni file regolare incontrato, il pathname dello stesso all'interno di un buffer condiviso di 
    capienza prefissata (10 pathname). Il thread STAT estrarrà, uno alla volta, i pathname da tale buffer e determinerà la dimensione in byte del file associato. 
    La coppia di informazioni (pathname, dimensione) sarà passata, attraverso un'altra struttura dati, al thread principale MAIN che si occuperà di mantenere 
    un totale globale.

    I thread si dovranno coordinare opportunamente tramite mutex e semafori numerici POSIX: il numero (minimo) e le modalità di impiego sono da determinare da 
    parte dello studente. Si dovrà inoltre rispettare la struttura dell'output riportato nell'esempio a seguire.

    I thread dovranno terminare spontaneamente al termine dei lavori.
*/

#define handle(msg)                             \
do{                                             \
    fprintf(stderr, "[%d] error ", __LINE__);   \
    perror(msg);                                \
    exit(EXIT_FAILURE);                         \
} while (false);

#define CAPACITY 10

typedef struct 
{
    char pathname[CAPACITY][PATH_MAX]; // buffer di 10 pathname
    int in, out; 
    unsigned size; // dimensione del buffer
    unsigned end_work; // incremento quando un DIR ha finito
    pthread_mutex_t lock;
    sem_t full, empty;
}Shared;

typedef struct 
{
    char pathfile[PATH_MAX];
    unsigned file_size; 
    bool finish;
    sem_t write, read;
}Pair;

typedef struct 
{
    pthread_t tid;
    unsigned idx;
    char* directory; // directory assegnata (solo DIR)
    Shared* sh;
    Pair* p;
}Thread;

void shared_init(Shared* s)
{
    s->in = s->out = s->end_work = s->size = 0;
    if(sem_init(&s->empty, 0, 10)) handle("sem init");
    if(sem_init(&s->full, 0, 0)) handle("sem init");
    if(pthread_mutex_init(&s->lock, NULL)) handle("mutex init");
}

void shared_destroy(Shared* s)
{
    if(sem_destroy(&s->empty)) handle("sem destroy");
    if(sem_destroy(&s->full)) handle("sem destroy");
    if(pthread_mutex_destroy(&s->lock)) handle("mutex destroy");
    free(s);
}

void pair_init(Pair* p)
{
    p->finish = false;
    if(sem_init(&p->write, 0, 1)) handle("sem init");
    if(sem_init(&p->read, 0, 0)) handle("sem init");
}

void pair_destroy(Pair* p)
{
    if(sem_destroy(&p->write)) handle("sem destroy");
    if(sem_destroy(&p->read)) handle("sem destroy");
    free(p);
}

void THREAD_DIR(void* arg)
{
    Thread* th = (Thread*)arg;
    char pathfile[PATH_MAX];    // costruisco il path del file regolare trovato
    struct stat info;
    struct dirent *entry;

    DIR* dp;
    if(!(dp = opendir(th->directory))) handle("opendir");
    printf("[D-%d] scansione della cartella '%s'\n", th->idx, th->directory);

    while(entry = readdir(dp))
    {
        sprintf(pathfile, "%s/%s", th->directory, entry->d_name);
        if((lstat(pathfile, &info)) < 0) handle("lstat");
        if(S_ISREG(info.st_mode))   // per ogni file regolare
        {
            printf("[D-%d] trovato file '%s' in '%s'\n", th->idx, entry->d_name, th->directory);
            if(sem_wait(&th->sh->empty)) handle("wait");
            if(pthread_mutex_lock(&th->sh->lock)) handle("mutex lock");

            // Inserisco il file nella coda condivisa (Producer)
            th->sh->in = (th->sh->in + 1) % 10; 
            strncpy(th->sh->pathname[th->sh->in], pathfile, PATH_MAX);
            th->sh->size ++;

            if(pthread_mutex_unlock(&th->sh->lock)) handle("mutex unlock");
            if(sem_post(&th->sh->full)) handle("post");
        }
    }

    // Un DIR ha finito di depositare i file della propria directory
    if(pthread_mutex_lock(&th->sh->lock)) handle("mutex lock");
    th->sh->end_work ++;
    if(pthread_mutex_unlock(&th->sh->lock)) handle("mutex unlock");

    closedir(dp);
    pthread_exit(NULL);
}

void THREAD_STAT(void* arg)
{
    Thread* th = (Thread*)arg;
    char* pathfile;
    struct stat info;
    bool done = false;

    while(!done)
    {
        if(sem_wait(&th->sh->full)) handle("sem wait");
        if(sem_wait(&th->p->write)) handle("sem wait");
        if(pthread_mutex_lock(&th->sh->lock)) handle("mutex lock");

        // Estraggo un elemento dalla coda (Consumer)
        th->sh->out = (th->sh->out + 1) % 10; 
        th->sh->size --;
        
        // Se n DIR hanno terminato e la coda è vuota
        if(th->sh->end_work == (th->idx - 1) && th->sh->size == 0)
        {
            th->p->finish = true;   // segnalo al main che tutti i DIR hanno finito
            done = true;
        }

        pathfile = th->sh->pathname[th->sh->out];

        if((lstat(pathfile, &info)) < 0) handle("lstat");
        printf("[STAT] il file '%s' ha dimensione %ld byte.\n", pathfile, info.st_size);

        // Scrivo i dati al Main (Writer)
        strncpy(th->p->pathfile, pathfile, PATH_MAX);
        th->p->file_size = info.st_size;

        if(pthread_mutex_unlock(&th->sh->lock)) handle("mutex unlock");
        if(sem_post(&th->p->read)) handle("sem post");
        if(sem_post(&th->sh->empty)) handle("sem post");
    }
    pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
    if(argc < 2) handle("argc");
    int n = argc-1;

    Thread th[n+1]; // n DIR + 1 STAT
    Shared* sh = malloc(sizeof(Shared));
    Pair* p = malloc(sizeof(Pair));
    long global_tot = 0;

    shared_init(sh);
    pair_init(p);

    // Create DIR
    for(int i=0; i<n; i++)
    {
        th[i].idx = i+1;
        th[i].directory = argv[i+1];
        th[i].sh = sh;
        if(pthread_create(&th[i].tid, NULL, (void*)THREAD_DIR, (void*)&th[i])) handle("create");
    }
    
    // Create STAT
    th[n].idx = n + 1;
    th[n].sh = sh;
    th[n].p = p;
    if(pthread_create(&th[n].tid, NULL, (void*)THREAD_STAT, (void*)&th[n])) handle("create");

    while(true)
    {
        if(sem_wait(&p->read)) handle("sem wait");
        // Leggo la dimesione depositata da STAT (Reader)
        global_tot += p->file_size;
        if(p->finish) break;
        printf("[MAIN] con il file '%s' il totale parziale è di %lu byte.\n", p->pathfile, global_tot);
        if(sem_post(&p->write)) handle("sem post");
    }
    printf("[MAIN] il totale finale è di %lu byte.\n", global_tot);

    for(int i=0; i<n+1; i++)
        if(pthread_join(th[i].tid, NULL)) handle("join");
    
    shared_destroy(sh);
    pair_destroy(p);

    exit(EXIT_SUCCESS);
}
