#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define NUM_THREADS 5

void handle_error(char* msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

void thread_info(char* name)
{
    pid_t pid = getpid();
    pthread_t tid = pthread_self();
    printf("[%s] PID: %d, (%lu)\n", name, pid, (long unsigned)tid);
}

void* thread_function(void* arg) 
{
    int thread_id = *((int*)arg);
    thread_info((char *)arg);
    pthread_exit(NULL);
}

int main() 
{
    pthread_t threads[NUM_THREADS];
    unsigned long int i;

    thread_info("main");
    for (i = 0; i < NUM_THREADS; i++) 
        if(pthread_create(&threads[i], NULL, thread_function, (void*)i)) handle_error("Error pthread_create\n");

    for (i=0; i<NUM_THREADS; i++) 
        pthread_join(threads[i], NULL);
    
    printf("\nAll thread are finished\n");

    exit(EXIT_SUCCESS);
}
