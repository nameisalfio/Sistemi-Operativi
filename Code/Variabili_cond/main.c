#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

/*
    Il programma gestisce una serie di partite tra due giocatori virtuali (thread) P1 e P2 che giocano alla Morra Cinese. 
    Il programma creerà: due thread P1 e P2 che rappresenteranno i giocatori, un thread giudice G e un thread tabellone T. 
    I thread condivideranno una struttura dati e useranno un certo numero di semafori (minimo a scelta dello studente) da 
    usare opportunamente.

    La struttura dati dovrà contenere i dati relativi ad una singola partita con i seguenti specifici campi:

    mossa_p1: C(arta)/F(orbici)/S(asso)
    mossa_p2: C/arta/F(orbici)/S(asso)
    vincitore: 1 (giocatore 1)/2 giocatore(2)
    (eventuali dati ausiliari)

    Iniziata una partita, P1 e P2 popolano i relativi campi con una propria mossa casuale; ognuno, fatta la mossa, deve 
    segnalare la cosa al processo G che valuterà chi ha vinto, se c'è un vincitore allora G popolerà il campo apposito e 
    segnalerà la disponibilità di un nuovo esito al processo T; se invece la partita è patta (stessa mossa), allora lancerà 
    direttamente una nuova partita. Il processo T, in caso di vittoria, aggiornerà la propria classifica interna e avvierà, 
    se necessario, una nuova partita. Sempre T, alla fine della serie di partite, decreterà l'eventuale vincitore.

    I thread dovranno tutti terminare spontaneamente alla fine del torneo.
*/

#define TO_MOVE 10

typedef enum {PLAYER1, PLAYER2, JUDGE, SCOREBOARD} thread;
typedef enum {CARTA, FORBICE, SASSO} move;
char* nome_mossa[3] = {"carta", "forbice", "sasso"};

#define handle(msg) \
do { \
    fprintf(stderr, "[%d] ", __LINE__); \
    perror(msg); \
    exit(EXIT_FAILURE); \
    } while(false)

typedef struct 
{   
    move moves[2]; 
    thread winner;
    bool finish; 
    unsigned n_match;
    pthread_mutex_t lock;
    pthread_cond_t cond[4];
}Shared;

typedef struct 
{
    pthread_t tid;
    unsigned idx;
    Shared* shared;
}thread_data;

void Shared_init(Shared* shared)
{
    shared->winner = -1;
    shared->finish = false;
    shared->moves[PLAYER1] = shared->moves[PLAYER2] = TO_MOVE;
    if(pthread_mutex_init(&shared->lock, NULL)) handle("mutex init");
    for(int i=0; i<4; i++)
        if(pthread_cond_init(&shared->cond[i], NULL)) handle("cond init");
}

void Shared_destroy(Shared* shared)
{
    if(pthread_mutex_destroy(&shared->lock)) handle("mutex destroy");
    for(int i=0; i<4; i++)
        if(pthread_cond_destroy(&shared->cond[i])) handle("cond destroy");
    free(shared);
}

void player(void* arg)
{
    thread_data* td = (thread_data*)arg;
    while(true)
    {
        // ottengo il lock
        if(pthread_mutex_lock(&td->shared->lock) != 0 ) handle("mutex lock");

        // wait sulla variabile di condizione
        while(td->shared->moves[td->idx] != TO_MOVE && !td->shared->finish)
            if(pthread_cond_wait(&td->shared->cond[td->idx], &td->shared->lock)) handle("cond wait");

        // zona critica
        if(td->shared->finish)
        {
            if(pthread_mutex_unlock(&td->shared->lock)) handle("mutex unlock");
            break;
        }
        move m = rand()%3;
        td->shared->moves[td->idx] = m;
        printf("[Player%d] %s\n", td->idx+1, nome_mossa[m]);

        // signal sulla variabile di condizione
        if(pthread_cond_signal(&td->shared->cond[JUDGE])) handle("cond signal");

        //rilascio il lock
        if(pthread_mutex_unlock(&td->shared->lock)) handle("mutex unlock");
    }
    pthread_exit(NULL);
}

thread whowin(move* moves)
{
    if (moves[PLAYER1] == moves[PLAYER2]) return -1;

    if ((moves[PLAYER1] == CARTA && moves[PLAYER2] == SASSO) ||
        (moves[PLAYER1] == FORBICE && moves[PLAYER2] == CARTA) ||
        (moves[PLAYER1] == SASSO && moves[PLAYER2] == FORBICE))
        return PLAYER1;

    return PLAYER2;
}

void judge(void* arg)
{
    thread_data* td = (thread_data*)arg;
    thread winner;
    unsigned completed_match = 0;
    while(true)
    {
        // acquisisco il lock
        if(pthread_mutex_lock(&td->shared->lock)) handle("mutex lock");

        // wait sulla variabile di condizione
        while((td->shared->moves[PLAYER1] == TO_MOVE ||
               td->shared->moves[PLAYER2] == TO_MOVE) && !td->shared->finish)
            if(pthread_cond_wait(&td->shared->cond[JUDGE], &td->shared->lock)) handle("cond wait");

        // sezione critica
        if(td->shared->finish)
        {
            if(pthread_mutex_unlock(&td->shared->lock)) handle("mutex unlock");
            break;
        }
        printf("[Judge] P1:%s | P2:%s\n", nome_mossa[td->shared->moves[PLAYER1]], nome_mossa[td->shared->moves[PLAYER2]]);
        winner = whowin(td->shared->moves);

        td->shared->moves[PLAYER1] = td->shared->moves[PLAYER2] = TO_MOVE;

        // signal sulla variabile di condizione 
        if(winner == -1)
        {
            if(pthread_cond_signal(&td->shared->cond[PLAYER1])) handle("cond signal");
            if(pthread_cond_signal(&td->shared->cond[PLAYER2])) handle("cond signal");
            printf("[Judge] -- Match drawed --\n");
        }
        else
        {
            completed_match ++;
            td->shared->winner = winner;
            printf("[Judge] Match n.%d winner : P%d\n", completed_match, winner+1);
            if(pthread_cond_signal(&td->shared->cond[SCOREBOARD])) handle("cond signal");
        }
        
        // rilascio il lock
        if(pthread_mutex_unlock(&td->shared->lock)) handle("mutex unlock");
    }
    pthread_exit(NULL);
}

void scoreboard(void* arg)
{
    thread_data* td = (thread_data*)arg;
    unsigned score[2] = {0,0};
    for(int i=0; i< td->shared->n_match; i++)
    {
        if(pthread_mutex_lock(&td->shared->lock)) handle("mutex lock");
        
        while(td->shared->winner == -1)
            if(pthread_cond_wait(&td->shared->cond[SCOREBOARD], &td->shared->lock)) handle("cond wait");

        score[td->shared->winner] ++;   
        if(i < td->shared->n_match)
            printf("[Scoreboard] P1:%d P2:%d\n\n", score[PLAYER1], score[PLAYER2]);
        td->shared->winner = -1;

        if(pthread_cond_signal(&td->shared->cond[PLAYER1])) handle("cond signal");
        if(pthread_cond_signal(&td->shared->cond[PLAYER2])) handle("cond signal");
        if(pthread_mutex_unlock(&td->shared->lock)) handle("mutex unlock");
    }
    if(pthread_mutex_lock(&td->shared->lock)) handle("mutex lock");
    
    if(score[PLAYER1] == score[PLAYER2])
        printf("[Scoreboard] Pareggio\n");
    else
        printf("[Scoreboard] Vincitore del torneo: P%d\n", score[PLAYER1] > score[PLAYER2] ? 1 : 2);
    
    td->shared->finish = true;

    if(pthread_cond_signal(&td->shared->cond[PLAYER1])) handle("cond signal");
    if(pthread_cond_signal(&td->shared->cond[PLAYER2])) handle("cond signal");
    if(pthread_cond_signal(&td->shared->cond[JUDGE])) handle("cond signal");
    if(pthread_mutex_unlock(&td->shared->lock)) handle("mutex unlock");
    
    pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
    if(argc != 2 || atoi(argv[1]) < 0) handle("argc");
    unsigned n = atoi(argv[1]);

    srand(time(NULL));
    thread_data td[4];
    Shared* shared = malloc(sizeof(Shared));
    shared->n_match = n;
    Shared_init(shared);    //inizializza la struttura dati condivisa

    for(int i=0; i<4; i++)
    {
        td[i].idx = i;
        td[i].shared = shared;
    }

    if(pthread_create(&td[PLAYER1].tid, NULL, (void*)player, (void*)&td[PLAYER1])) handle("create player1");
    if(pthread_create(&td[PLAYER2].tid, NULL, (void*)player, (void*)&td[PLAYER2])) handle("create player2");
    if(pthread_create(&td[JUDGE].tid, NULL, (void*)judge, (void*)&td[JUDGE])) handle("create judge");
    if(pthread_create(&td[SCOREBOARD].tid, NULL, (void*)scoreboard, (void*)&td[SCOREBOARD])) handle("create scoreboard");

    for(int i=0; i<4; i++)
        if(pthread_join(td[i].tid, NULL)) handle("join");

    Shared_destroy(shared);
    exit(EXIT_SUCCESS);
}