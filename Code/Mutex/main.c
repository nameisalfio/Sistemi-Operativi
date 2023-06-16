#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#define NUM_THREADS 10
#define NAMES_THREAD {"Thread_1", "Thread_2", "Thread_3", "Thread_4", "Thread_5", \
                      "Thread_6", "Thread_7", "Thread_8", "Thread_9", "Thread_10"}
#define VET_SIZE 5

void handle_error(const char* msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

void printVet(int vet[], int n)
{
    printf("[");
    for(int i=0; i<n; i++)
        printf(" %d", vet[i]);
    printf(" ]\n");
}

typedef struct
{
    int vet[VET_SIZE];
    pthread_mutex_t mutex;
}shared_data;

typedef struct 
{
    pthread_t tid;
    char* name;

    shared_data* shared;
}thread_data;

void* thread_function(void* arg)
{
    thread_data* data = (thread_data*)arg;
    if (pthread_mutex_lock(&data->shared->mutex)) handle_error("Error lock\n");

    //Sezione critica
    for(int i=0; i<VET_SIZE; i++)  
        data->shared->vet[i] ++;

    if (pthread_mutex_unlock(&data->shared->mutex)) handle_error("Error unlock\n");
    pthread_exit(NULL);
}

int main(int argc, char**argv)
{
    thread_data threads[NUM_THREADS];
    char* names[] = NAMES_THREAD;
    shared_data shared;
    for(int i=0; i<VET_SIZE; i++)   
        shared.vet[i] = i;
    pthread_mutex_init(&shared.mutex, NULL);

    for(int i=0; i<NUM_THREADS; i++)
    {
        threads[i].name = names[i];
        threads[i].shared = &shared;
    }

    for(int i=0; i<NUM_THREADS; i++)
    {
        printf("[main] Creazione del thread '%s' in corso...\n", threads[i].name);
        if(pthread_create(&threads[i].tid, NULL, thread_function, (void*)&threads[i])) handle_error("Error create\n");
    }

    for (int i=0; i<NUM_THREADS; i++)
    {
        printf("[main] attesa terminazione del thread '%s'\n", threads[i].name);
        if (pthread_join(threads[i].tid, NULL)) handle_error("Error pthread join\n");
        printf("[main] thread '%s' terminato!\n", threads[i].name);
    }

    pthread_mutex_destroy(&shared.mutex);

    printf("[main] vettore condiviso --> ");
    printVet(shared.vet, VET_SIZE);

    exit(EXIT_SUCCESS);
}