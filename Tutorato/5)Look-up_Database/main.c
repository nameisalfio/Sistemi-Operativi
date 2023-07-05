#include "list.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
    Creare un programma lookup-database.c in linguaggio C che accetti invocazioni sulla riga di comando del tipo:

    lookup-database <db-file1> <db-file2> <...> <db-filen>
    Il programma dovrà fondamentalmente leggere n file database con coppie del tipo (nome, valore), permettendo all'utente di effettuare ricerche 
    interattive sugli stessi attraverso una chiave. Ciascun file database è un file testuale in cui ogni riga ha la struttura "nome:valore" dove nome 
    può contenere degli spazi e/o altri simboli e valore è un numero intero. Per una maggiore comprensione, vedere i file d'esempio allegati.

    Il programma, una volta avviato creerà n+2 thread che condivideranno una struttura dati a cui accederanno utilizzando lo strumento rwlock (quantità 
    e modalità di impiego da determinare da parte dello studente).

    La struttura dati condivisa conterrà:

    una lista dinamica singolarmente linkata (implementata dallo studente) in cui verranno caricati i dati secondo le regole definite di seguito;
    altri elementi ritenuti utili/necessari.
    I ruoli dei thread saranno i seguenti:

    ciascun thread 
    leggerà l'i-esimo file passato da riga di comando e ne manterrà il contenuto in memoria sulla lista dinamica presente nella struttura dati condivisa; 
    il file potrebbe avere un numero arbitrario di righe non noto a priori. Inoltre, ciascun thread, dopo aver inserito un nuovo elemento all'interno della 
    lista, dovrà bloccarsi per 8 secondi (utilizzando la funzione sleep).

    un thread Q si occuperà di prendere una chiave dallo standard input ed effettuerà una ricerca in tempo reale sulla lista singolamente linkata, stampando 
    in output il valore associato (se la chiave esiste)

    un thread C si occuperà, ogni 8 secondi, di conteggiare il numero di entry (nodi) presenti all'interno della lista singolarmente linkata e stamperà tale 
    statistica a video

    Al termine degli inserimenti da parte dei primi n thread, il thread Q dovrà continuare a leggere eventuali query dallo standard input e restituire in 
    output il risultato.

    Il programma deve terminare se l'utente invia al thread Q, la keyword quit.
*/

#define SLEEP_S 8

void handle(const char* msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

typedef struct 
{
    pthread_rwlock_t lock; // lock in lettura/scrittura
    bool finish;
    list *l; // puntatore alla lista condivisa
} shared;

typedef struct 
{
    // dati privati
    pthread_t tid;
    unsigned thread_n;
    char *filename;

    // dati condivisi
    shared *shared;
} thread_data;

void init_shared(shared *sh) 
{
    if (pthread_rwlock_init(&sh->lock, NULL)) handle("Error pthread_rwlock_init\n");

    sh->l = malloc(sizeof(list));
    sh->finish = 0;
    init_list(sh->l);
}

void destroy_shared(shared *sh) 
{
    pthread_rwlock_destroy(&sh->lock);
    destroy_list(sh->l);
    free(sh);
}

// Lettori (nota che modificano la struttura dati quindi vi fanno accesso in scrittura)
void reader(void *arg) 
{
    thread_data *td = (thread_data *)arg;
    FILE *f;
    char buffer[BUFSIZ];
    char *key, *s_value;
    int value;

    // apro il file in sola lettura
    if (!(f = fopen(td->filename, "r"))) handle("fopen");

    // leggo il file riga per riga utilizzando un buffer
    while ((fgets(buffer, BUFSIZ, f))) 
    {
        if ((key = strtok(buffer, ":")) && (s_value = strtok(NULL, ":")))  
        {
            value = atoi(s_value);

            // provo ad ottenere il lock in scrittura
            if (pthread_rwlock_wrlock(&td->shared->lock)) handle("pthread_rwlock_wrlock\n");

            if (td->shared->finish) 
            {
                printf("R%d: esco.\n", td->thread_n);

                // rilascio il lock sulla struttura dati condivisa ed esco
                if (pthread_rwlock_unlock(&td->shared->lock)) handle("pthread_rwlock_unlock\n");
                break;
            }

            // inserisco l'elemento all'interno della lista
            insert_head(td->shared->l, key, value);

            // rilascio il lock sulla struttura dati condivisa
            if (pthread_rwlock_unlock(&td->shared->lock))handle("pthread_rwlock_unlock\n");

            printf("R%d: inserito l'elemento (%s,%d)\n", td->thread_n, key, value);
            sleep(SLEEP_S); // rimango in attesa 
        }
    }
    fclose(f);
    pthread_exit(NULL);
}

//fa sia rd_lock, sia wr_lock
void query(void *arg) 
{
    thread_data *td = (thread_data *)arg;
    char query[BUFSIZ];
    int result;
    bool ret_value;

    while (true) 
    {
        // leggo la query dallo standard input
        if (fgets(query, BUFSIZ, stdin)) 
        {
            query[strcspn(query, "\n")] = 0;

            // verifico se l'utente ha chiesto di terminare
            if (!strcasecmp(query, "quit")) 
            {
                if (pthread_rwlock_wrlock(&td->shared->lock)) handle("pthread_rwlock_wrlock\n");

                td->shared->finish = true;

                if (pthread_rwlock_unlock(&td->shared->lock)) handle("pthread_rwlock_unlock\n");
                
                printf("Q: chiusura dei thread...\n");
                break;
            } 
            else 
            {
                // provo a ottenere il lock lettura
                if (pthread_rwlock_rdlock(&td->shared->lock)) handle("pthread_rwlock_rdlock\n");

                // effettuo una ricerca all'interno della lista
                ret_value = list_search(td->shared->l, query, &result);

                // rilascio il lock sulla struttura dati condivisa
                if (pthread_rwlock_unlock(&td->shared->lock)) handle("pthread_rwlock_unlock\n");

                if (ret_value)
                    printf("Q: occorrenza trovata (%s,%d)\n", query, result);
                else
                    printf("Q: non è stata trovata alcuna occorrenza con chiave %s\n", query);
            }
        }
    }
    pthread_exit(NULL);
}

void counter(void *arg) 
{
    thread_data *td = (thread_data *)arg;
    int err, counter;

    while (true) 
    {
        if (pthread_rwlock_rdlock(&td->shared->lock)) handle("pthread_rwlock_rdlock\n");

        if (td->shared->finish) 
        {
            printf("C: esco.\n");

            // rilascio il lock sulla struttura dati condivisa ed esco
            if (pthread_rwlock_unlock(&td->shared->lock)) handle("pthread_rwlock_unlock\n");
            break;
        }

        counter = list_count(td->shared->l);

        if (pthread_rwlock_unlock(&td->shared->lock)) handle("pthread_rwlock_unlock\n");

        printf("C: sono presenti %d elementi all'interno della lista\n\n", counter);
        sleep(SLEEP_S);
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) 
{
    if (argc < 2) handle("Usage: <db-file1> <db-file2> <...> <db-filen>\n");

    // creo un array di thread_data, uno per ciascun thread (n+2)
    thread_data td[argc + 1];
    shared *sh = malloc(sizeof(shared));
    init_shared(sh);

    // creo i thread Reader
    for (int i = 0; i < argc - 1; i++) 
    {
        td[i].shared = sh;
        td[i].filename = argv[i + 1];
        td[i].thread_n = i + 1;

        if (pthread_create(&td[i].tid, NULL, (void *)reader, &td[i])) handle("pthread_create\n");
    }

    // // creo il thread Query
    td[argc - 1].shared = sh;
    if (pthread_create(&td[argc - 1].tid, NULL, (void *)query, &td[argc - 1])) handle("pthread_create\n");

    // creo il thread C
    td[argc].shared = sh;
    if (pthread_create(&td[argc].tid, NULL, (void *)counter, &td[argc])) handle("pthread_create\n");

    for (int i = 0; i < argc + 1; i++)
        if (pthread_join(td[i].tid, NULL)) handle("pthread_join\n");

    destroy_shared(sh);
}