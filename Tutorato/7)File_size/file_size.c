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
    Il programma dovrà determinare la dimensione totale in byte dei file regolari direttamente contenuti all'interno delle 
    cartelle indicate (senza ricorsione).

    Al suo avvio il programma creerà n+1 thread:

    un thread DIR-i che si occuperanno di scansionare la cartella assegnata alla ricerca di file regolari direttamente contenuti 
    in essa (no ricorsione);
    un thread STAT che si occuperà di determinare la dimensione di ogni file regolare individuato.

    Gli n thread DIR-i agiranno in parallelo e inseriranno, per ogni file regolare incontrato, il pathname dello stesso all'interno 
    di un buffer condiviso di capienza prefissata (10 pathname). Il thread STAT estrarrà, uno alla volta, i pathname da tale buffer 
    e determinerà la dimensione in byte del file associato. 

    La coppia di informazioni (pathname, dimensione) sarà passata, attraverso un'altra struttura dati, al thread principale MAIN 
    che si occuperà di mantenere un totale globale.

    I thread si dovranno coordinare opportunamente tramite mutex e semafori numerici POSIX: il numero (minimo) e le modalità di 
    impiego sono da determinare da parte dello studente. Si dovrà inoltre rispettare la struttura dell'output riportato 
    nell'esempio a seguire.

    I thread dovranno terminare spontaneamente al termine dei lavori.
*/

#define CAPACITY 10
#define handle(msg)                             \
do{                                             \
    fprintf(stderr, "[%d] error ", __LINE__);   \
    perror(msg);                                \
    exit(EXIT_FAILURE);                         \
} while (false);

typedef struct 
{
    char* buffer[CAPACITY];
    unsigned in, out, size, finish, n_dir; 
    sem_t empty, full;
    pthread_rwlock_t lock;
} Shared_dir;

typedef struct 
{
    char* file_name;
    int file_size;
    bool finish;
    sem_t read, write;
} Shared_main;

typedef struct 
{
    pthread_t tid;
    unsigned ID;
    char* path;
    Shared_dir* sh_dir;
    Shared_main* sh_main;
}Thread;

void shared_dir_init(Shared_dir* sh, int n_dir)
{   
    sh->n_dir = n_dir;
    sh->in = sh->out = sh->finish = sh->size = 0;
    for(int i=0; i<CAPACITY; i++)
        sh->buffer[i] = malloc(PATH_MAX);
    if(pthread_rwlock_init(&sh->lock, NULL)) handle("rwlock init");
    if(sem_init(&sh->empty, 0, CAPACITY)) handle("sem init");
    if(sem_init(&sh->full, 0, 0)) handle("sem init");
}

void shared_main_init(Shared_main* sh)
{
    sh->finish = false;
    sh->file_name = malloc(PATH_MAX);
    if(sem_init(&sh->write, 0, 1)) handle("sem init");
    if(sem_init(&sh->read, 0, 0)) handle("sem init");
}

void shared_dir_destroy(Shared_dir* sh)
{
    for(int i=0; i<CAPACITY; i++)
        free(sh->buffer[i]);
    if(pthread_rwlock_destroy(&sh->lock)) handle("rwlock destroy");
    if(sem_destroy(&sh->empty)) handle("sem destroy");
    if(sem_destroy(&sh->full)) handle("sem destroy");
    free(sh);
}

void shared_main_destroy(Shared_main* sh)
{
    free(sh->file_name);
    if(sem_destroy(&sh->write)) handle("sem destroy");
    if(sem_destroy(&sh->read)) handle("sem destroy");
    free(sh);
}

void Dir(void* arg)
{
    Thread* th = (Thread*) arg;
    DIR* dp;
    struct dirent* file;
    struct stat info;

    if(!(dp = opendir(th->path))) handle("opendir");
    printf("[D-%d] scansione della cartella '%s'\n", th->ID, th->path);

    char path[PATH_MAX];
    while(file = readdir(dp))
    {
        sprintf(path, "%s/%s", th->path, file->d_name); // costruisco il percorso
        if(lstat(path, &info) < 0) handle("lstat");
        if(S_ISREG(info.st_mode))
        {
            printf("[D-%d] trovato file '%s' in '%s'\n", th->ID, file->d_name, th->path);
            if(sem_wait(&th->sh_dir->empty)) handle("wait");
            if(pthread_rwlock_wrlock(&th->sh_dir->lock)) handle("wrlock");

            // Zona critica
            th->sh_dir->in = (th->sh_dir->in + 1)%CAPACITY;
            strcpy(th->sh_dir->buffer[th->sh_dir->in], path);
            th->sh_dir->size ++;

            if(pthread_rwlock_unlock(&th->sh_dir->lock)) handle("unlock");
            if(sem_post(&th->sh_dir->full)) handle("post");
        }
    }
    closedir(dp);
            
    if(pthread_rwlock_wrlock(&th->sh_dir->lock)) handle("lock");
    th->sh_dir->finish ++;
    if(pthread_rwlock_unlock(&th->sh_dir->lock)) handle("unlock");

    pthread_exit(NULL);
}

void Stat(void* arg)
{
    Thread* th = (Thread*) arg;
    char* path;
    struct stat info;

    bool done = false;
    while(!done)
    {
        if(sem_wait(&th->sh_dir->full)) handle("wait");
        if(pthread_rwlock_rdlock(&th->sh_dir->lock)) handle("rdlock");

        // Zona critica (DIR)
        th->sh_dir->out = (th->sh_dir->out + 1)%CAPACITY;
        th->sh_dir->size --; 

        // n DIR hanno finito && coda vuota
        if(th->sh_dir->finish == th->sh_dir->n_dir && th->sh_dir->size == 0)
        {
            th->sh_main->finish = true;
            done = true;
        }
        path = th->sh_dir->buffer[th->sh_dir->out];

        if((lstat(path, &info)) < 0) handle("lstat");
        printf("[STAT] il file '%s' ha dimensione %ld byte.\n", path, info.st_size);

        if(done) break;

        if(pthread_rwlock_unlock(&th->sh_dir->lock)) handle("unlock");
        if(sem_post(&th->sh_dir->empty)) handle("post");

        if(sem_wait(&th->sh_main->write)) handle("wait");
        // Zona critica (MAIN)
        strcpy(th->sh_main->file_name, path);
        th->sh_main->file_size = info.st_size;

        if(sem_post(&th->sh_main->read)) handle("post");
    }
    pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
    if(argc < 2) handle("argc");
    int n = argc-1;
    long unsigned global_tot = 0;

    Thread th[n+1];
    Shared_dir* sh_dir = malloc(sizeof(Shared_dir));
    Shared_main* sh_main = malloc(sizeof(Shared_main));
    shared_dir_init(sh_dir, n);
    shared_main_init(sh_main);

    // Dir
    for(int i=0; i<n; i++)
    {
        th[i].ID = i+1;
        th[i].path = argv[i+1];
        th[i].sh_dir = sh_dir;
        if(pthread_create(&th[i].tid, NULL, (void*)Dir, (void*)&th[i])) handle("create");
    }

    // Stat
    th[n].sh_dir = sh_dir;
    th[n].sh_main = sh_main;
    if(pthread_create(&th[n].tid, NULL, (void*)Stat, (void*)&th[n])) handle("create");

    // Main
    while(true)
    {
        if(sem_wait(&sh_main->read)) handle("wait");
        
        // Zona critica 
        global_tot += sh_main->file_size;
        if(sh_main->finish) break;
        printf("[MAIN] con il file '%s' il totale parziale è di %lu byte.\n", sh_main->file_name, global_tot);

        if(sem_post(&sh_main->write)) handle("post");
    }
    printf("[MAIN] il totale finale è di %lu byte.\n", global_tot);

    for(int i=0; i<n+1; i++)
        if(pthread_join(th[i].tid, NULL)) handle("join");

    shared_dir_destroy(sh_dir);
    shared_main_destroy(sh_main);

    exit(EXIT_SUCCESS);
}