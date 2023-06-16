#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define NUM_THREAD 2
#define NAMES_THREAD { "Thread_1", "Thread_2", "Thread_3", "Thread_4", "Thread_5", "Thread_6" }
#define INCREMENTS 100

void handle_error(char* msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

typedef struct 
{
    unsigned long total;
    pthread_mutex_t lock;   //meccanismi di mutua esclusione tra i dati condivisi
} shared_data;

typedef struct 
{
    pthread_t tid;
    char* name;
    unsigned long increments;

    shared_data* shared;  
} thread_data;

void* thread_function(void* arg)
{
    thread_data* data_ptr = (thread_data*) arg;
    printf("[%s] Applicazione progressiva dei %lu incrementi...\n", data_ptr->name, data_ptr->increments);
    while (data_ptr->increments > 0)
    {
        if (pthread_mutex_lock(&data_ptr->shared->lock)) handle_error("Error lock\n");  //wait

        (data_ptr->shared->total)++;    //zona critica

        if (pthread_mutex_unlock(&data_ptr->shared->lock)) handle_error("Error unlock\n");  //signal
        
        data_ptr->increments--;
    }
    printf("[%s] incrementi applicati!\n", data_ptr->name);
    pthread_exit(NULL);
}

int main(int argc, char** argv)
{
    thread_data datas[NUM_THREAD];
    shared_data shared = {0, PTHREAD_MUTEX_INITIALIZER};    //dati condivisi (variabile, mutex)
    char* thread_names[] = NAMES_THREAD;
    unsigned long expected_total = NUM_THREAD * INCREMENTS;

    for (int i=0; i<NUM_THREAD; i++)
    {
        datas[i].name = thread_names[i];
        datas[i].increments = INCREMENTS;
        datas[i].shared = &shared; // Dati condivisi sempre per indirizzo
    }


    for (int i=0; i<NUM_THREAD; i++)
    {
        printf("[main] Creazione del thread '%s'...\n", datas[i].name);
        if (pthread_create(&datas[i].tid, NULL, thread_function, (void*)(&datas[i]))) handle_error("Error pthread_create\n"); // &datas[i] -> arg
    }

    for (int i=0; i<NUM_THREAD; i++)
    {
        printf("[main] attesa terminazione del thread '%s' ...\n", datas[i].name);

        if (pthread_join(datas[i].tid, NULL)) handle_error("Error pthread join\n");

        printf("[main] thread '%s' terminato!\n", datas[i].name);
    }
    printf("\n[main] valore reale = %lu, valore atteso = %lu\n", shared.total, expected_total);

    exit(EXIT_SUCCESS);
}
