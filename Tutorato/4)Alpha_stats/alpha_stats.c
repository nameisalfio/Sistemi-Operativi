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

#define handle(msg)                              \
do{                                              \
    fprintf(stderr, "[%d] error ", __LINE__);    \
    perror(msg);                                 \
    exit(EXIT_FAILURE);                          \
}while(false);      

typedef struct 
{
    char c;
    bool finish;
    unsigned long stats[26];
    sem_t P, AL, MZ;
} Shared;

typedef struct 
{
    pthread_t tid;
    char* file;
    Shared* sh;
} Thread;

void shared_init(Shared* sh)
{
    sh->finish = false;
    memset(&sh->stats, 0, sizeof(sh->stats));
    if(sem_init(&sh->P, 0, 1)) handle("sem init");
    if(sem_init(&sh->AL, 0, 0)) handle("sem init");
    if(sem_init(&sh->MZ, 0, 0)) handle("sem init");
}

void shared_destroy(Shared* sh)
{
    if(sem_destroy(&sh->P)) handle("sem destroy");
    if(sem_destroy(&sh->AL)) handle("sem destroy");
    if(sem_destroy(&sh->MZ)) handle("sem destroy");
    free(sh);
}

void print_stats(Thread* th)
{
    printf("Stats\n");
    for(int i=0, j=0; i<26; i++)
    {
        printf("%c: %lu ", 'a'+i, th->sh->stats[i]);
        if(++j == 4) 
        {
            printf("\n");
            j=0;
        }
    }
    printf("\n");
}

void P(void* arg)
{
    Thread* th = (Thread*) arg;
    int fd, i;
    struct stat info;
    char* map, char_read;

    if((fd = open(th->file, O_RDONLY)) < 0) handle("open");
    if((fstat(fd, &info)) < 0) handle("fstat");
    if((map = mmap(NULL, info.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) handle("mmap");
    close(fd);

    i = 0;
    while(i < info.st_size)
    {
        char_read = tolower(map[i]);
        if(char_read >= 'a' &&  char_read <= 'l')
        {
            if(sem_wait(&th->sh->P)) handle("wait");
            th->sh->c = char_read;
            if(sem_post(&th->sh->AL)) handle("post");
        }
        else if (char_read >= 'm' &&  char_read <= 'z')
        {
            if(sem_wait(&th->sh->P)) handle("wait");
            th->sh->c = char_read;
            if(sem_post(&th->sh->MZ)) handle("post");
        }
        i++;
    }
    if (munmap(map, info.st_size) < 0) handle("munmap");

    if(sem_wait(&th->sh->P)) handle("wait");
    th->sh->finish = true;
    if(sem_post(&th->sh->AL)) handle("post");
    if(sem_post(&th->sh->MZ)) handle("post");

    print_stats(th);
    pthread_exit(NULL);
}

void AL(void* arg)
{
    Thread* th = (Thread*) arg;
    while(true)
    {
        if(sem_wait(&th->sh->AL)) handle("wait");
        th->sh->stats[th->sh->c-'a']++;     // Normalizzazione
        if(th->sh->finish) break;
        if(sem_post(&th->sh->P)) handle("post");
    }
    pthread_exit(NULL);
}

void MZ(void* arg)
{
    Thread* th = (Thread*) arg;
    while(true)
    {
        if(sem_wait(&th->sh->MZ)) handle("wait");
        th->sh->stats[th->sh->c-'a']++;     // Normalizzazione
        if(th->sh->finish) break;
        if(sem_post(&th->sh->P)) handle("post");
    }
    pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
    if(argc < 2) handle("argc");

    Thread th[3];
    Shared* sh = malloc(sizeof(Shared));
    shared_init(sh);

    for(int i=0; i<3; i++)
        th[i].sh = sh;

    th[0].file = argv[1];

    if(pthread_create(&th[0].tid, NULL, (void*)P, (void*)&th[0])) handle("create");
    if(pthread_create(&th[1].tid, NULL, (void*)AL, (void*)&th[1])) handle("create");
    if(pthread_create(&th[2].tid, NULL, (void*)MZ, (void*)&th[2])) handle("create");

    for(int i=0; i<3; i++)
        if(pthread_join(th[i].tid, NULL)) handle("join");

    shared_destroy(sh);
    exit(EXIT_SUCCESS);
}