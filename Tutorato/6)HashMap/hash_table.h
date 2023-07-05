#ifndef HASH_TABLE
#define HASH_TABLE

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define KEY_SIZE 4096
#define HASH_FUNCTION_MULTIPLIER 97

void handle(const char* msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

typedef struct __item 
{
    char key[KEY_SIZE];
    int value;
    struct __item *next;
} item;

typedef struct 
{
    unsigned long size; // numero slot
    unsigned long n; // numero elementi nella tabella hash
    item **table;
    pthread_rwlock_t lock;
} hash_table;

hash_table *new_hash_table(unsigned long size)
{
    hash_table *h = malloc(sizeof(hash_table));

    h->size = size;
    h->n = 0;
    h->table = calloc(size, sizeof(item *));

    if ((pthread_rwlock_init(&h->lock, NULL))) handle("pthread_rwlock_init");
    return h;
}

unsigned long hash_function(const char *key) 
{
    unsigned const char *us;
    unsigned long h = 0;

    for(int i=0; i<strlen(key); i++)
        h = h * HASH_FUNCTION_MULTIPLIER + key[i];

    return h;
}

void hash_table_insert(hash_table *h, const char *key, const int value) 
{
    item *i = malloc(sizeof(item));
    strncpy(i->key, key, KEY_SIZE);
    i->value = value;
    unsigned long hindex = hash_function(key);

    if ((pthread_rwlock_wrlock(&h->lock))) handle("pthread_rwlock_wrlock");

    hindex = hindex % h->size;
    i->next = h->table[hindex];
    h->table[hindex] = i;
    h->n++;

    if ((pthread_rwlock_unlock(&h->lock))) handle("pthread_rwlock_unlock");
}

bool hash_table_search(hash_table *h, const char *key, int *value) 
{
    bool ret_value = false;
    item *ptr;
    unsigned long hindex = hash_function(key);

    if ((pthread_rwlock_rdlock(&h->lock))) handle("pthread_rwlock_wrlock");

    hindex = hindex % h->size;
    ptr = h->table[hindex];

    while (ptr && strcmp(key, ptr->key))
        ptr = ptr->next;

    if (ptr) 
    {
        ret_value = true;
        *value = ptr->value;
    }

    if ((pthread_rwlock_unlock(&h->lock))) handle("pthread_rwlock_unlock");

    return ret_value;
}

unsigned long hash_table_get_n(hash_table *h) 
{
    unsigned long n;
    if ((pthread_rwlock_rdlock(&h->lock))) handle("pthread_rwlock_rdlock");

    n = h->n;

    if ((pthread_rwlock_unlock(&h->lock))) handle("pthread-rwlock_unlock");

    return n;
}

void list_destroy(item *l) 
{
    item *ptr = l;
    item *tmp;

    while (ptr) 
    {
        tmp = ptr;
        ptr = ptr->next;
        free(tmp);
    }
}

void hash_table_destroy(hash_table *h) 
{

    if ((pthread_rwlock_wrlock(&h->lock))) handle("pthread_rwlock_wrlock");

    for (unsigned long i = 0; i < h->size; i++)
        list_destroy(h->table[i]);

    free(h->table);
    pthread_rwlock_destroy(&h->lock);
    free(h);
}

#endif