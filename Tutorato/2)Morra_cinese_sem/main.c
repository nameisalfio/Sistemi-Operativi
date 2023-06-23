#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>  

typedef enum { PLAYER1, PLAYER2, JUDGE, SCOREBOARD } thread;
typedef enum { CARTA, FORBICE, SASSO } move;
char* move_name[] = { "carta", "forbice", "sasso"};

void handle(const char* msg, int line)
{
    fprintf(stderr, "error at line %d : ", line);
    perror(msg);
    exit(EXIT_FAILURE);
}

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

typedef struct
{
    move moves[2]; //MOSSA1 | MOSSA2
    thread winner; 
    bool finish;
    sem_t sem[4]; 
}shared;

typedef struct 
{
    pthread_t tid;
    unsigned n_thread; //PLAYER1 PLAYER2 JUDGE SCOREBOARD
    unsigned n_match;
    shared* share;
}thread_data;

void init_sem(sem_t* sem)
{
    if(sem_init(&sem[PLAYER1], 0, 1)) handle("sem_init\n", __LINE__);
    if(sem_init(&sem[PLAYER2], 0, 1)) handle("sem_init\n", __LINE__);
    if(sem_init(&sem[JUDGE], 0, 0)) handle("sem_init\n", __LINE__);
    if(sem_init(&sem[SCOREBOARD], 0, 0)) handle("sem_init\n", __LINE__);
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

void player(void* arg)
{
    thread_data* td = (thread_data*)arg;
    move move;
    while(true)
    {
        //wait
        if(sem_wait(&td->share->sem[td->n_thread])) handle("sem_wait\n", __LINE__);

        //sezione critica
        if(td->share->finish) break;

        move = rand()%3;
        td->share->moves[td->n_thread] = move;
        printf("[Player%d] %s\n", td->n_thread+1, move_name[move]);

        //signal
        if(sem_post(&td->share->sem[JUDGE])) handle("sem_wait\n", __LINE__);
    }
    pthread_exit(NULL);
}

void judge(void* arg)
{
    thread_data* td = (thread_data*)arg;
    int completed_match;
    int winner;
    while(true)
    {
        //wait
        if(sem_wait(&td->share->sem[JUDGE])) handle("sem_wait\n", __LINE__);
        if(sem_wait(&td->share->sem[JUDGE])) handle("sem_wait\n", __LINE__);

        //sezione critica
        if(td->share->finish) break;

        printf("[Judge] P1:%s | P2:%s\n", move_name[td->share->moves[PLAYER1]], move_name[td->share->moves[PLAYER2]]);
        winner = whowin(td->share->moves);
        if(winner < 0)  //pareggio
        {
            //signal
            if(sem_post(&td->share->sem[PLAYER1])) handle("sem_post\n", __LINE__);
            if(sem_post(&td->share->sem[PLAYER2])) handle("sem_post\n", __LINE__);
            printf("[Judge] -- Match not winned --\n");
        }
        else
        {
            completed_match ++;
            td->share->winner = winner;
            printf("[Judge] Match n.%d winner : P%d\n", completed_match, winner+1);

            //signal
            if(sem_post(&td->share->sem[SCOREBOARD])) handle("sem_post\n", __LINE__);
        }
    }
    pthread_exit(NULL);
}

void scoreboard(void* arg)
{
    thread_data* td = (thread_data*)arg;
    thread score[] = {0, 0};
    for(int i=0; i<td->n_match; i++)
    {
        //wait
        if(sem_wait(&td->share->sem[SCOREBOARD])) handle("sem_wait\n", __LINE__);

        //sezione critica
        score[td->share->winner]++;    //aggiorno il tabellone
        if (i < td->n_match) //se non è l'ulitma partita(all'ultima dovrò uscire)
        {
            //signal
            printf("[Scoreboard] P1:%d P2:%d\n\n", score[PLAYER1], score[PLAYER2]);
            if(sem_post(&td->share->sem[PLAYER1])) handle("sem_post\n", __LINE__);
            if(sem_post(&td->share->sem[PLAYER2])) handle("sem_post\n", __LINE__);
        }
    }
    td->share->finish = true;
    
    if(score[PLAYER1] == score[PLAYER2])
        printf("[Scoreboard] Pareggio\n");
    else
        printf("[Scoreboard] Vincitore del torneo: P%d\n", score[PLAYER1] > score[PLAYER2] ? 1 : 2);

    //signal
    if(sem_post(&td->share->sem[JUDGE])) handle("sem_post\n", __LINE__);
    if(sem_post(&td->share->sem[JUDGE])) handle("sem_post\n", __LINE__);

    pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
    int n;

    if(argc != 2 || atoi(argv[1]) == 0) handle("argc\n", __LINE__);
    n = atoi(argv[1]);

    thread_data threads[4];
    shared* share = malloc(sizeof(shared));

    //inizializzo i dati condivisi
    share->finish = false;
    init_sem(share->sem);

    //inizializzo i dati privati
    for(int i=0; i<4; i++)
    {
        threads[i].n_match = n;
        threads[i].n_thread = i;
        threads[i].share = share;
    }

    srand(time(NULL));

    // creazione dei thread    
    if (pthread_create(&threads[PLAYER1].tid, NULL, (void*)player, (void*)&threads[PLAYER1])) handle("pthread_create\n", __LINE__);
    if (pthread_create(&threads[PLAYER2].tid, NULL, (void*)player, (void*)&threads[PLAYER2])) handle("pthread_create\n", __LINE__);
    if (pthread_create(&threads[JUDGE].tid, NULL, (void*)judge, (void*)&threads[JUDGE])) handle("pthread_create\n", __LINE__);
    if (pthread_create(&threads[SCOREBOARD].tid, NULL, (void*)scoreboard, (void*)&threads[SCOREBOARD])) handle("pthread_create\n", __LINE__);

    // attendo la fine dell'esecuzione dei thread
    for(int i=0; i<4; i++)
        if(pthread_join(threads[i].tid, NULL)) handle("pthread_join\n", __LINE__);
    
    for (int i = 0; i < 4; i++)
        if(sem_destroy(&share->sem[i])) handle("sem_destroy\n", __LINE__);

    free(share);
    exit(EXIT_SUCCESS);
}