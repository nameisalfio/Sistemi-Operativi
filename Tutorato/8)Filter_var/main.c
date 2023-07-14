#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>

#define handle(msg)                             \
do{                                             \
    fprintf(stderr, "[%d] error ", __LINE__);   \
    perror(msg);                                \
    exit(EXIT_FAILURE);                         \
}while(false);                                  

#define MAX_LEN 1024

typedef struct 
{
    char line[MAX_LEN];
    unsigned turn;
    unsigned n_filter;
    bool finish;    
    pthread_mutex_t lock;
    pthread_cond_t *cond;
}Shared;

typedef struct 
{
    pthread_t tid;
    unsigned ID;
    char* filter; // ^word || _word || %word_1,word_2
    Shared* sh;
}Thread;

void init_shared(Shared* sh, int n_filter)
{
    sh->turn = sh->finish = 0;
    sh->n_filter = n_filter;
    if(pthread_mutex_init(&sh->lock, NULL)) handle("mutex init");
    sh->cond = malloc(sizeof(pthread_cond_t) * (n_filter+1));
    for(int i=0; i<= n_filter; i++)
        if(pthread_cond_init(&sh->cond[i], NULL)) handle("cond init");
}

void destroy_shared(Shared* sh, int n_filter)
{
    if(pthread_mutex_destroy(&sh->lock)) handle("mutex destroy");
    for(int i=0; i<= n_filter; i++)
        if(pthread_cond_destroy(&sh->cond[i])) handle("cond_destroy");
    free(sh->cond);
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
    Thread *th = (Thread*)arg;
    while(true)
    {
        if(pthread_mutex_lock(&th->sh->lock)) handle("lock");
        while(!(th->sh->turn == th->ID))
            if(pthread_cond_wait(&th->sh->cond[th->ID], &th->sh->lock)) handle("cond wait");

        apply_filter(th->sh->line, th->filter);
        th->sh->turn = (th->sh->turn + 1) % (th->sh->n_filter + 1);

        if(pthread_cond_signal(&th->sh->cond[(th->ID+1)%(th->sh->n_filter+1)])) handle("cond signal");
        if(pthread_mutex_unlock(&th->sh->lock)) handle("unlock");
        if(th->sh->finish) break;
    }
    pthread_exit(NULL);
}

int main(int argc, char* argv[])    // ./main filename.txt [filter-1] ... [filter-n]
{
    if(argc < 2) handle("argc");

    const char* filename = argv[1];
    unsigned n_filter = argc - 2;
    Shared *sh = malloc(sizeof(Shared));
    Thread th[n_filter+1];

    init_shared(sh, n_filter);

    // Main create
    th[0].ID = 0;
    th[0].sh = sh;

    // Filter-i create
    for(int i=1; i<= n_filter; i++)
    {
        th[i].ID = i;
        th[i].filter = argv[i+1];
        th[i].sh = sh;
        if(pthread_create(&th[i].tid, NULL, (void*)Filter, &th[i])) handle("create");
    }

    FILE *fp;
    if(!(fp = fopen(filename, "r"))) handle("fopen");

    char line[MAX_LEN];
    while(fgets(line, MAX_LEN, fp))
    {
        if(pthread_mutex_lock(&sh->lock)) handle("lock");
        while(!(sh->turn == 0))
            if(pthread_cond_wait(&sh->cond[0], &sh->lock)) handle("cond wait");
        printf("%s", sh->line);

        strcpy(sh->line, line);
        sh->turn = (sh->turn + 1) % n_filter;

        if(pthread_cond_signal(&sh->cond[1])) handle("cond signal");
        if(pthread_mutex_unlock(&sh->lock)) handle("unlock");
    }
    printf("%s", sh->line);
    sh->finish = true;

    for(int i=1; i<= n_filter; i++)
        if(pthread_join(th[i].tid, NULL)) handle("join");

    fclose(fp);
    destroy_shared(sh, n_filter);
    exit(EXIT_SUCCESS);
} 