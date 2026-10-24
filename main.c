#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LADO 5
#define NG (LADO * LADO)              
#define TOP NG                       
#define TIZQ (NG + 1)
#define TDER (NG + 2)
#define NN (NG + 3)
#define N(x, y) ((y) * LADO + (x))    
#define NP 13                        
#define MAXV 8
#define MAX_MOVS 2000               
enum { NADIE, PUMA, PERROS };

typedef struct {
    unsigned seed;
    int dmin, dmax, prob_fallo, puma, perro[NP];
} Config;

typedef struct Mesa Mesa;
typedef struct { Mesa *m; int id; unsigned seed; } Pieza;

struct Mesa {
    int id, jp, jd;                   
    pthread_mutex_t mtx;           
    pthread_cond_t cv;
    int tab[NN];                     
    int pos[NP], viva[NP], latido[NP];
    atomic_int vista_puma;            
    int capturas, movs, fin, ganador, caido;
    char motivo[96];
    Pieza pz[NP];
    pthread_t th[NP];
};

typedef struct { int id; const char *rol, *res; } Jugador;

static Config cfg = {42, 50, 300, 0, N(2, 2), {0}};
static int vec[NN][MAXV], nvec[NN];        
static int sal[NN][MAXV][2], nsal[NN];   
static int dist[NN][NN];                  

static int g_N, g_M, g_ini, g_fin, g_mm_fin;
static Jugador *g_jug;
static int *g_cola;
static pthread_mutex_t g_mtx_cola = PTHREAD_MUTEX_INITIALIZER, g_mtx_log = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cv_cola = PTHREAD_COND_INITIALIZER;
static sem_t g_sem;
static sigset_t g_sig;
static atomic_int g_stop, g_fin_normal, g_activas, g_max, g_partidas;

static void *xcalloc(size_t n, size_t s) {
    void *p = calloc(n, s);
    if (!p) { fputs("sin memoria\n", stderr); exit(1); }
    return p;
}

static void hilo(pthread_t *t, void *(*f)(void *), void *a) {
    if (pthread_create(t, NULL, f, a)) { fputs("error al crear un hilo\n", stderr); exit(1); }
}

int main(void) {
    printf("Torneo de Komikan Concurrente\n");
    return 0;
}