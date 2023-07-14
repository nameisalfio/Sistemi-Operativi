#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#define handle(msg)                              \
do{                                             \
    fprintf(stderr, "[%d] error ", __LINE__);   \
    perror(msg);                                \
    exit(EXIT_FAILURE);                         \
}while(false);                                  

#define MAX_LEN 1024

typedef struct 
{
    char line[MAX_LEN];
    unsigned n_filter;
    bool finish;
    sem_t* sem;
}Shared;

typedef struct 
{
    pthread_t tid;
    unsigned ID;
    char* filter; // ^word || _word || %word_1,word_2
    Shared* sh;
}Thread;

void shared_init(Shared *sh, unsigned n_filter)
{
    sh->finish = false;
    sh->n_filter = n_filter;
    sh->sem = malloc(sizeof(sem_t) * (n_filter+1));
    if(sem_init(&sh->sem[0], 0, 1)) handle("sem init");
    for(int i=1; i<=n_filter; i++)
        if(sem_init(&sh->sem[i], 0, 0)) handle("sem init");
}

void shared_destroy(Shared *sh, unsigned n_filter)
{
    if(sem_destroy(&sh->sem[0])) handle("sem destroy");
    for(int i=1; i<=n_filter; i++)
        if(sem_destroy(&sh->sem[i])) handle("sem destroy");
    free(sh);
}

void upper_filter(char* line, char* word)
{
    char* ptr;
    while(ptr = strstr(line, word))
    {
        for(int i=0; i<strlen(word); i++)
            ptr[i]  = toupper(ptr[i]);
    }
}

void lower_filter(char* line, char* word)
{
    char* ptr;
    while(ptr = strstr(line, word))
    {
        for(int i=0; i<strlen(word); i++)
            ptr[i]  = tolower(ptr[i]);
    }
}

void replace_filter(char* line, char* word_1, char* word_2)
{
    int len_1 = strlen(word_1), len_2 = strlen(word_2);
    char* ptr;

    if(!strcmp(word_1, word_2)) return;

    while(ptr = strstr(line, word_1))
    {
        size_t n = strlen(ptr + len_1) + 1;
        memmove(ptr + len_2, ptr + len_1, n);
        memcpy(ptr, word_2, len_2);
    }
}

void apply_filter(char* line, char* filter)
{
    // ^word || _word || %word_1,word_2
    char word[MAX_LEN];
    for (int i = 0; i < strlen(filter); i++) 
            word[i] = filter[i+1];
    switch (filter[0])
    {
        case '^': 
            upper_filter(line, word);
            break;

        case '_': 
            lower_filter(line, word);
            break;

        case '%': 
            char *word_1 = strtok(word, ",");
            char *word_2 = strtok(NULL, "\n");
            replace_filter(line, word_1, word_2);
            break;
        
        default:
            fprintf(stderr, "Invalid filter\n");
            exit(EXIT_FAILURE);
            break;
    }
}

void Filter(void* arg)
{
    Thread* th = (Thread*)arg;
    unsigned next;
    while(!th->sh->finish)
    {
        if(sem_wait(&th->sh->sem[th->ID])) handle("sem wait");
        apply_filter(th->sh->line, th->filter);
        next = (th->ID + 1) % (th->sh->n_filter + 1);
        if(sem_post(&th->sh->sem[next])) handle("sem post");
    }
    pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
    if(argc < 2) handle("argc");

    const char* filename = argv[1];
    unsigned n_filter = argc-2;

    Thread th[n_filter];
    Shared* sh = malloc(sizeof(Shared));
    shared_init(sh, n_filter);
    
    //Create filter
    for(int i=0; i<n_filter; i++)
    {
        th[i].ID = i+1;
        th[i].filter = argv[i+2];
        th[i].sh = sh;
        if(pthread_create(&th[i].tid, NULL, (void*)Filter, (void*)&th[i])) handle("create");
    }

    FILE* fp;
    if(!(fp = fopen(filename, "r"))) handle("fopen");

    char line[MAX_LEN];
    while(fgets(line, MAX_LEN, fp))
    {        
        if(sem_wait(&sh->sem[0])) handle("sem wait");
        printf("%s", sh->line);
        strcpy(sh->line, line);
        if(sem_post(&sh->sem[1])) handle("sem post");
    }
    printf("%s", sh->line);
    sh->finish = true;

    for(int i=0; i<n_filter; i++)
        if(pthread_join(th[i].tid, NULL)) handle("join");

    shared_destroy(sh, n_filter);
    fclose(fp);

    exit(EXIT_SUCCESS);
}