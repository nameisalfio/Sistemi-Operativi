#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <linux/limits.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#define handle(msg)                              \
do{                                              \
    fprintf(stderr, "[%d] error ", __LINE__);    \
    perror(msg);                                 \
    exit(EXIT_FAILURE);                          \
}while(false);                                   

typedef struct
{
    int n_reader;
    int turn;
    int next;
    bool finish;
    char line[BUFSIZ];    
    char file[PATH_MAX];
    pthread_mutex_t lock;
    pthread_cond_t *cond;
} Shared_r;

typedef struct
{
    bool turn;
    bool finish;
    char line[BUFSIZ];
    char file[PATH_MAX];
    pthread_mutex_t lock;
    pthread_cond_t F;
    pthread_cond_t M;
    pthread_barrier_t barrier;
} Shared_m;

typedef struct
{
    pthread_t tid;
    unsigned ID;
    char file[PATH_MAX];
    char word[BUFSIZ];
    bool opt_i;
    bool opt_v;
    Shared_r *sr;
    Shared_m *sm;
} Thread;

void Shared_r_init(Shared_r* sr, int n_reader)
{
    sr->finish = false;
    sr->turn = sr->next = 1;
    sr->n_reader = n_reader;
    if(pthread_mutex_init(&sr->lock, NULL)) handle("mutex init");
    sr->cond = malloc(sizeof(pthread_cond_t) * (n_reader + 1));
    for(int i=0; i<=n_reader; i++)
        if(pthread_cond_init(&sr->cond[i], NULL)) handle("cond init");
}

void Shared_r_destroy(Shared_r* sr, int n_reader)
{
    if(pthread_mutex_destroy(&sr->lock)) handle("mutex destroy");
    for(int i=0; i<=n_reader; i++)
        if(pthread_cond_destroy(&sr->cond[i])) handle("cond destroy");
    free(sr->cond);
    free(sr);
}

void Shared_m_init(Shared_m* sm)
{
    sm->finish = false;
    sm->turn = 0;
    if(pthread_mutex_init(&sm->lock, NULL)) handle("mutex init");
    if(pthread_cond_init(&sm->M, NULL)) handle("cond init");
    if(pthread_cond_init(&sm->F, NULL)) handle("cond init");
    if(pthread_barrier_init(&sm->barrier, NULL, 3)) handle("barrier init");
}

void Shared_m_destroy(Shared_m* sm)
{
    if(pthread_mutex_destroy(&sm->lock)) handle("mutex destroy");
    if(pthread_cond_destroy(&sm->M)) handle("cond destroy");
    if(pthread_cond_destroy(&sm->F)) handle("cond destroy");
    if(pthread_barrier_destroy(&sm->barrier)) handle("barrier destroy");
    free(sm);
}

void READER(void *arg)
{
    Thread *th = (Thread *)arg;
    char* map;
    int fd;
    struct stat info;
    
    if((fd = open(th->file, O_RDONLY)) < 0) handle("open");
    if((fstat(fd, &info)) < 0) handle("fstat");
    if((map = mmap(NULL, info.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0)) == MAP_FAILED) handle("mmap");
    close(fd);

    if (pthread_mutex_lock(&th->sr->lock)) handle("pthread_mutex_lock");
    while (!(th->sr->turn == th->ID))
        if (pthread_cond_wait(&th->sr->cond[th->ID], &th->sr->lock)) handle("pthread_cond_wait");
        
    char* line = strtok(map, "\n");
    if(!line) pthread_exit(NULL);

    if(pthread_cond_signal(&th->sr->cond[0])) handle("signal");
    if(pthread_mutex_unlock(&th->sr->lock)) handle("unlock");

    while (line)
    {
        if (pthread_mutex_lock(&th->sr->lock)) handle("pthread_mutex_lock");
        while (!(th->sr->turn == th->ID))
            if (pthread_cond_wait(&th->sr->cond[th->ID], &th->sr->lock)) handle("pthread_cond_wait");
            
        strcpy(th->sr->line, line);
        strcpy(th->sr->file, th->file);
        th->sr->turn = 0;
        line = strtok(NULL, "\n");

        if(pthread_cond_signal(&th->sr->cond[0])) handle("signal");
        if(pthread_mutex_unlock(&th->sr->lock)) handle("unlock");
    }
    munmap(map, info.st_size);

    if(pthread_mutex_lock(&th->sr->lock)) handle("lock");
    while(!(th->sr->turn == th->ID))
        if(pthread_cond_wait(&th->sr->cond[th->ID], &th->sr->lock)) handle("wait");
        
    th->sr->finish = true;
    th->sr->turn = 0;   

    if (th->sr->next > th->sr->n_reader) pthread_exit(NULL);
    th->sr->next++;

    if(pthread_cond_signal(&th->sr->cond[0])) handle("signal");
    if(pthread_mutex_unlock(&th->sr->lock)) handle("unlock");

    pthread_exit(NULL);
}

bool apply_filter(Thread* th)
{
    bool outcome = false;

    if(!th->opt_v && !th->opt_i)  // no parameter
        if((strstr(th->sr->line, th->word))) outcome = true;

    if(!th->opt_v && th->opt_i)   // -i (case insensitive)
        if((strcasestr(th->sr->line, th->word)))  outcome = true;

    if(th->opt_v && !th->opt_i)   // -v (inverted)
        if(!(strstr(th->sr->line, th->word))) outcome = true;

    if(th->opt_v && th->opt_i)    // -v -i
        if(!(strcasestr(th->sr->line, th->word))) outcome = true;
  
    return outcome;
}

void FILTERER(void *arg)
{
    int err;
    Thread *th = (Thread *)arg;

    while (1)
    {
        if(pthread_mutex_lock(&th->sr->lock)) handle("lock");
        while(!(th->sr->turn == 0))
            if(pthread_cond_wait(&th->sr->cond[th->ID], &th->sr->lock)) handle("wait");

        th->sr->turn = th->sr->next;
        if (th->sr->finish)
            th->sr->finish = false;

        if (th->sr->next > th->sr->n_reader)
        {
            th->sm->finish = true;
            th->sm->turn = 1;

            if(pthread_cond_signal(&th->sm->M)) handle("signal");
            if(pthread_mutex_unlock(&th->sr->lock)) handle("unlock");
            break;
        }

        if (apply_filter(th))
        {
            if (pthread_mutex_lock(&th->sm->lock)) handle("pthread_mutex_lock");

            while(!(th->sm->turn == 0))
                if(pthread_cond_wait(&th->sm->F, &th->sm->lock)) handle("wait");
            
            strcpy(th->sm->line, th->sr->line);
            strcpy(th->sm->file, th->sr->file);
            th->sm->turn = 1;

            if (pthread_cond_signal(&th->sm->M)) handle("pthread_cond_signal");
            if (pthread_mutex_unlock(&th->sm->lock)) handle("pthread_mutex_unlock");
        }

        if(pthread_cond_signal(&th->sr->cond[th->sr->turn])) handle("signal");
        if(pthread_mutex_unlock(&th->sr->lock)) handle("unlock");
    }
    if(pthread_barrier_wait(&th->sm->barrier) > 0) handle("wait");
    pthread_exit(NULL);
}

void MAIN(void *arg)
{
    int err;
    Thread *th = (Thread *)arg;

    while (1)
    {

        if (pthread_mutex_lock(&th->sm->lock)) handle("pthread_mutex_lock");

        while (th->sm->turn == 0)
            if (pthread_cond_wait(&th->sm->M, &th->sm->lock)) handle("pthread_cond_wait");

        if (th->sm->finish)
        {
            if (pthread_mutex_unlock(&th->sm->lock)) handle("pthread_mutex_unlock");
            break;
        }

        printf("[MAIN] %s : %s\n", th->sm->file, th->sm->line);
        th->sm->turn = 0;

        if (pthread_cond_signal(&th->sm->F)) handle("pthread_cond_signal");
        if (pthread_mutex_unlock(&th->sm->lock)) handle("pthread_mutex_unlock");
    }
    if(pthread_barrier_wait(&th->sm->barrier) > 0) handle("wait");
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    if(argc < 3) handle("argc");

    bool opt_i, opt_v;
    opt_i = opt_v = false;
    
    int pos = 1;
    if(!strcmp(argv[1], "-v")) { opt_v = true; pos++; }
    if(!strcmp(argv[1], "-i")) { opt_i = true; pos++; }
    if(!strcmp(argv[2], "-v")) { opt_v = true; pos++; }
    if(!strcmp(argv[2], "-i")) { opt_i = true; pos++; }

    char* word = argv[pos];
    int n_reader = argc-(pos+1);

    Thread th[n_reader+2];
    Shared_r* sr = malloc(sizeof(Shared_r));
    Shared_m* sm = malloc(sizeof(Shared_m));

    Shared_r_init(sr, n_reader);
    Shared_m_init(sm);

    // Filter
    th[0].ID = 0;
    th[0].opt_i = opt_i;
    th[0].opt_v = opt_v;
    strcpy(th[0].word, word);
    th[0].sm = sm;
    th[0].sr = sr;
    if(pthread_create(&th[0].tid, NULL, (void*)FILTERER, (void*)&th[0])) handle("create");

    // Reader
    for(int i=1; i<=n_reader; i++)
    {
        th[i].ID = i;
        strcpy(th[i].file, argv[pos+i]);
        th[i].sr = sr;
        if(pthread_create(&th[i].tid, NULL, (void*)READER, (void*)&th[i])) handle("create");
    }

    // MAIN
    th[n_reader+1].ID = n_reader+1;
    th[n_reader+1].sm = sm;
    if(pthread_create(&th[n_reader+1].tid, NULL, (void*)MAIN, (void*)&th[n_reader+1])) handle("create");

    if(pthread_barrier_wait(&th->sm->barrier) > 0) handle("wait");

    Shared_r_destroy(sr, n_reader);
    Shared_m_destroy(sm);

    exit(EXIT_SUCCESS);
}