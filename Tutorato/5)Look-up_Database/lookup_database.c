#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include "list.h"

#define TIME 8
#define handle(msg)                             \
do{                                             \
    fprintf(stderr, "[%d] error ", __LINE__);   \
    perror(msg);                                \
    exit(EXIT_FAILURE);                         \
}while(false);   

typedef struct 
{
    bool finish;
    list* l;
    pthread_rwlock_t lock;
}Shared;

typedef struct 
{
    pthread_t tid;
    unsigned ID;
    char* filename;
    Shared* sh;
}Thread;

void shared_init(Shared* sh)
{
    sh->finish = false;
    if(pthread_rwlock_init(&sh->lock, NULL)) handle("rw_lock init");
    sh->l = malloc(sizeof(list));
    list_init(sh->l);
}

void shared_destroy(Shared* sh)
{
    if(pthread_rwlock_destroy(&sh->lock)) handle("rw_lock destroy");
    list_destroy(sh->l);
    free(sh);
}

void Reader(void* arg)
{
    Thread* th = (Thread*)arg;
    FILE* fp;
    char line[BUFSIZ], *key, *value_str;
    int value;

    if(!(fp = fopen(th->filename, "r"))) handle("fopen");
    while(fgets(line, BUFSIZ, fp))
    {
        key = strtok(line, ":");
        value_str = strtok(NULL, "\n");
        value = atoi(value_str);

        if(pthread_rwlock_wrlock(&th->sh->lock)) handle("wrlock");
        if(th->sh->finish)
        {
            if(pthread_rwlock_unlock(&th->sh->lock)) handle("unlock");
            break;
        }

        list_insert_head(th->sh->l, key, value);
        if(pthread_rwlock_unlock(&th->sh->lock)) handle("unlock");
        printf("[READER-%d] Insert %s:%d from %s\n",th->ID, key, value, th->filename);
        sleep(TIME);
    }
    pthread_exit(NULL);
}

void Query(void* arg)
{
    Thread* th = (Thread*)arg;
    char line[BUFSIZ];
    int value;
    while (true)
    {   
        if(fgets(line, BUFSIZ, stdin))
        {
            line[strcspn(line, "\n")] = '\0';
            if(!strcasecmp(line, "quit"))
            {
                if(pthread_rwlock_wrlock(&th->sh->lock)) handle("wrlock");
                th->sh->finish = true;
                if(pthread_rwlock_unlock(&th->sh->lock)) handle("unlock");
                printf("[QUERY] chiusura dei thread...\n");
                break;
            }
            if(pthread_rwlock_rdlock(&th->sh->lock)) handle("rdlock");

            if(list_search(th->sh->l, line, &value))
                printf("[QUERY] key %s : %d\n", line, value);
            else
                printf("[QUERY] key %s not found\n", line);

            if(pthread_rwlock_unlock(&th->sh->lock)) handle("unlock");
        }
    }
    pthread_exit(NULL);
}

void Counter(void* arg)
{
    Thread* th = (Thread*)arg;
    int count=0;
    while(true)
    {
        if(pthread_rwlock_rdlock(&th->sh->lock)) handle("rdlock");
        if(th->sh->finish)
        {
            if(pthread_rwlock_unlock(&th->sh->lock)) handle("unlock");
            break;
        }
        count = list_count(th->sh->l);
        if(pthread_rwlock_unlock(&th->sh->lock)) handle("unlock");
        printf("[COUNTER] there are %d elements\n\n", count);
        sleep(TIME);
    }
    pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
    if(argc < 2) handle("argc");

    int n = argc - 1;

    Thread th[n+2];
    Shared* sh = malloc(sizeof(Shared));
    shared_init(sh);

    // Reader
    for(int i=0; i<n; i++)
    {
        th[i].ID = i+1;
        th[i].filename = argv[i+1];
        th[i].sh = sh;
        if(pthread_create(&th[i].tid, NULL, (void*)Reader, (void*)&th[i])) handle("create");
    }

    // Query
    th[n].sh = sh;
    if(pthread_create(&th[n].tid, NULL, (void*)Query, (void*)&th[n])) handle("create");

    // Counter
    th[n+1].sh = sh;
    if(pthread_create(&th[n+1].tid, NULL, (void*)Counter, (void*)&th[n+1])) handle("create");

    for(int i=0; i<n+2; i++)
        if(pthread_join(th[i].tid, NULL)) handle("join");

    shared_destroy(sh);
    exit(EXIT_SUCCESS);
}