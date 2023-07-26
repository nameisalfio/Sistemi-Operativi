#ifndef LIST_H
#define LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define KEY_SIZE 4096

typedef struct _node
{
    char key[KEY_SIZE];
    int value;
    struct _node *next;
} node;

typedef struct 
{
    node *head;
} list;

void list_init(list *l) { l->head = NULL; }

void list_insert_head(list *l, char *key, int value) 
{
    node *n = malloc(sizeof(node));
    strncpy(n->key, key, KEY_SIZE);
    n->value = value;

    n->next = l->head;
    l->head = n;
}

void list_insert_tail(list *l, char *key, int value) 
{
    node *n = malloc(sizeof(node));
    strncpy(n->key, key, KEY_SIZE);
    n->value = value;
    n->next = NULL;

    if(!l->head)
    {
        list_insert_head(l, key, value);
        return;
    }

    node *ptr = l->head;
    while (ptr->next) 
        ptr = ptr->next;
    ptr->next = n;
}

void list_print (list *l) 
{
    node *ptr = l->head;
    while (ptr) 
    {
        printf("(%s,%d) ", ptr->key, ptr->value);
        ptr = ptr->next;
    }
    printf("\n");
}

bool list_search (list *l, char *key, int *result)
{
    node *ptr = l->head;
    while (ptr && (strcmp(ptr->key, key) != 0))
        ptr = ptr->next;

    if (!ptr) return false;

    *result = ptr->value;
    return true;
}

int list_count (list *l) 
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

void list_destroy(list *l) 
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
