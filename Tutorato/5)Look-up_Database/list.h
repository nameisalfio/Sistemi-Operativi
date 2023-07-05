#ifndef LIST_H
#define LIST_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_SIZE 4096

typedef struct __node
{
    char key[KEY_SIZE];
    int value;
    struct __node *next;
} node;

typedef struct 
{
    node *head;
} list;

void init_list(list *l) { l->head = NULL; }

void insert_head(list *l, const char *key, const int value) 
{
    node *n = (node*) malloc(sizeof(node));
    strncpy(n->key, key, KEY_SIZE);
    n->value = value;

    n->next = l->head;
    l->head = n;
}

void insert_tail(list *l, const char *key, const int value) 
{
    node *n = (node*) malloc(sizeof(node));
    strncpy(n->key, key, KEY_SIZE);
    n->value = value;
    n->next = NULL;

    if(!l->head)
    {
        insert_head(l, key, value);
        return;
    }

    node *ptr = l->head;
    while (ptr->next) 
        ptr = ptr->next;
    ptr->next = n;
}

void list_print(const list *l) 
{
    node *ptr = l->head;
    while (ptr) 
    {
        printf("(%s,%d) ", ptr->key, ptr->value);
        ptr = ptr->next;
    }
    printf("\n");
}

bool list_search(const list *l, const char *key, int *result)
{
    node *ptr = l->head;

    while (ptr && (strcmp(ptr->key, key) != 0))
        ptr = ptr->next;

    if (!ptr) return false;

    *result = ptr->value;
    return true;
}

int list_count(const list *l) 
{
    node *ptr = l->head;
    int counter = 0;

    while (ptr) 
    {
        counter++;
        ptr = ptr->next;
    }
    return counter;
}

void destroy_list(list *l) 
{
    node *ptr = l->head;
    node *tmp;
    while (ptr) 
    {
        tmp = ptr;
        ptr = ptr->next;
        free(tmp);
    }
    free(l);
}

#endif 
