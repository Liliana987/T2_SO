#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
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
    int id;
    pthread_mutex_t mtx;
    pthread_cond_t cv;
    int tab[NN];
    int pos[NP], viva[NP];
    atomic_int vista_puma;
    int capturas, movs, fin, ganador;
    char motivo[96];
    Pieza pz[NP];
    pthread_t th[NP];
};

static Config cfg = {42, 50, 300, 0, N(2, 2), {0}};
static int vec[NN][MAXV], nvec[NN];
static int sal[NN][MAXV][2], nsal[NN];
static int dist[NN][NN];

static int g_N, g_M;
static pthread_mutex_t g_mtx_log = PTHREAD_MUTEX_INITIALIZER;

static void hilo(pthread_t *t, void *(*f)(void *), void *a) {
    if (pthread_create(t, NULL, f, a)) { fputs("error al crear un hilo\n", stderr); exit(1); }
}

static void registrar(const char *fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    pthread_mutex_lock(&g_mtx_log);
    fputs(b, stdout);
    fflush(stdout);
    pthread_mutex_unlock(&g_mtx_log);
}

static int retardo(unsigned *s) { return cfg.dmin + (int)(rand_r(s) % (unsigned)(cfg.dmax - cfg.dmin + 1)); }

static void dormir_ms(int ms) {
    struct timespec t = {ms / 1000, (long)(ms % 1000) * 1000000L};
    while (nanosleep(&t, &t) == -1 && errno == EINTR) {}
}

static const char *nombre(int n, char *buf) {
    if (n == TOP) return "TRONO_TOP";
    if (n == TIZQ) return "TRONO_LEFT";
    if (n == TDER) return "TRONO_RIGHT";
    snprintf(buf, 16, "(%d,%d)", n % LADO, n / LADO);
    return buf;
}

static int leer(FILE *f, int n, int v[]) {
    for (int i = 0; i < n; i++)
        if (fscanf(f, "%d", &v[i]) != 1) return 0;
    return 1;
}

static int en_rango(int x, int y) { return x >= 0 && x < LADO && y >= 0 && y < LADO; }

static int cargar_config(const char *ruta) {
    FILE *f = fopen(ruta, "r");
    if (!f) { perror(ruta); return -1; }
    char k[64];
    int v[3] = {0}, ok = 1;
    while (ok && fscanf(f, "%63s", k) == 1 && strcmp(k, "EVENT")) {
        if (!strcmp(k, "SEED")) { ok = leer(f, 1, v); cfg.seed = (unsigned)v[0]; }
        else if (!strcmp(k, "DELAY_MIN")) { ok = leer(f, 1, v); cfg.dmin = v[0]; }
        else if (!strcmp(k, "DELAY_MAX")) { ok = leer(f, 1, v); cfg.dmax = v[0]; }
        else if (!strcmp(k, "PROB_FALLO")) { ok = leer(f, 1, v); cfg.prob_fallo = v[0]; }
        else if (!strcmp(k, "PUMA_INIT")) {
            ok = leer(f, 2, v) && en_rango(v[0], v[1]);
            if (ok) cfg.puma = N(v[0], v[1]);
        } else if (!strcmp(k, "DOG")) {
            ok = leer(f, 3, v) && v[0] >= 0 && v[0] < NP - 1 && en_rango(v[1], v[2]);
            if (ok) cfg.perro[v[0] + 1] = N(v[1], v[2]);
        }
    }
    fclose(f);
    if (!ok || cfg.dmin < 0 || cfg.dmin > cfg.dmax || cfg.prob_fallo < 0 || cfg.prob_fallo > 100) {
        fprintf(stderr, "%s: configuración inválida\n", ruta);
        return -1;
    }
    return 0;
}

static void arista(int a, int b) {
    vec[a][nvec[a]++] = b;
    vec[b][nvec[b]++] = a;
    dist[a][b] = dist[b][a] = 1;
}

static void salto(int a, int mid, int b) {
    sal[a][nsal[a]][0] = mid; sal[a][nsal[a]++][1] = b;
    sal[b][nsal[b]][0] = mid; sal[b][nsal[b]++][1] = a;
}

static void construir_grafo(void) {
    for (int i = 0; i < NN; i++)
        for (int j = 0; j < NN; j++) dist[i][j] = i == j ? 0 : 99;
    for (int y = 0; y < LADO; y++)
        for (int x = 0; x < LADO; x++) {
            if (x < LADO - 1) arista(N(x, y), N(x + 1, y));
            if (y < LADO - 1) arista(N(x, y), N(x, y + 1));
            if (x < LADO - 2) salto(N(x, y), N(x + 1, y), N(x + 2, y));
            if (y < LADO - 2) salto(N(x, y), N(x, y + 1), N(x, y + 2));
        }
    arista(TOP, TIZQ); arista(TOP, TDER); arista(TIZQ, TDER); arista(TOP, N(2, 4));
    arista(TIZQ, N(0, 4)); arista(TIZQ, N(1, 4)); arista(TDER, N(3, 4)); arista(TDER, N(4, 4));
    salto(N(2, 3), N(2, 4), TOP); salto(N(1, 3), N(1, 4), TIZQ); salto(N(3, 3), N(3, 4), TDER);
    salto(N(0, 4), TIZQ, TOP); salto(N(4, 4), TDER, TOP);
    for (int k = 0; k < NN; k++)
        for (int i = 0; i < NN; i++)
            for (int j = 0; j < NN; j++)
                if (dist[i][k] + dist[k][j] < dist[i][j]) dist[i][j] = dist[i][k] + dist[k][j];
}

static int es_ady(int a, int b) {
    for (int i = 0; i < nvec[a]; i++)
        if (vec[a][i] == b) return 1;
    return 0;
}

static void colocar(Mesa *m, int id, int n) { m->tab[n] = id; m->pos[id] = n; m->viva[id] = 1; }

static void inicializar(Mesa *m) {
    for (int i = 0; i < NN; i++) m->tab[i] = -1;
    colocar(m, 0, cfg.puma);
    atomic_store(&m->vista_puma, cfg.puma);
    for (int id = 1; id < NP; id++)
        if (cfg.perro[id] >= 0 && m->tab[cfg.perro[id]] < 0) colocar(m, id, cfg.perro[id]);
    for (int id = 1, n = 0; id < NP; id++) {
        if (m->viva[id]) continue;
        while (n < NG && m->tab[n] >= 0) n++;
        if (n < NG) colocar(m, id, n);
    }
}

static char sim(int v) { return v < 0 ? '.' : v ? 'D' : 'P'; }

static void imprimir_tablero(const Mesa *m, const char *txt) {
    char b[768];
    size_t n = (size_t)snprintf(b, sizeof b, "[Partida %d] %s\n    trono: T=%c izq=%c der=%c\n", m->id, txt,
                                sim(m->tab[TOP]), sim(m->tab[TIZQ]), sim(m->tab[TDER]));
    for (int y = LADO - 1; y >= 0; y--) {
        n += (size_t)snprintf(b + n, sizeof b - n, "    y=%d ", y);
        for (int x = 0; x < LADO; x++) n += (size_t)snprintf(b + n, sizeof b - n, " %c", sim(m->tab[N(x, y)]));
        n += (size_t)snprintf(b + n, sizeof b - n, "\n");
    }
    registrar("%s         0 1 2 3 4  (x)\n", b);
}

static void terminar(Mesa *m, int ganador, const char *motivo) {
    if (m->fin) return;
    m->fin = 1;
    m->ganador = ganador;
    snprintf(m->motivo, sizeof m->motivo, "%.90s", motivo);
    pthread_cond_broadcast(&m->cv);
}

static int movs_puma(const Mesa *m, int dest[], int cap[]) {
    int n = 0, p = m->pos[0];
    for (int i = 0; i < nvec[p]; i++)
        if (m->tab[vec[p][i]] < 0) { dest[n] = vec[p][i]; cap[n++] = -1; }
    for (int i = 0; i < nsal[p]; i++)
        if (m->tab[sal[p][i][0]] > 0 && m->tab[sal[p][i][1]] < 0) { dest[n] = sal[p][i][1]; cap[n++] = sal[p][i][0]; }
    return n;
}

static void revisar(Mesa *m) {
    int d[16], c[16];
    if (m->capturas >= 6) terminar(m, PUMA, "el puma capturó 6 perros");
    else if (!movs_puma(m, d, c)) terminar(m, PERROS, "puma acorralado, sin movimientos");
    else if (m->movs >= MAX_MOVS) terminar(m, NADIE, "límite de movimientos (empate)");
}

static void aplicar(Mesa *m, int id, int hasta, int cap) {
    char a[16], b[16], c[16], quien[16], extra[64] = "", ev[160];
    int desde = m->pos[id];
    if (id) snprintf(quien, sizeof quien, "perro %d", id); else strcpy(quien, "PUMA");
    if (cap >= 0) {
        snprintf(extra, sizeof extra, " captura al perro %d en %s (capturas=%d)", m->tab[cap], nombre(cap, c), m->capturas + 1);
        m->viva[m->tab[cap]] = 0;
        m->tab[cap] = -1;
        m->capturas++;
    }
    m->tab[desde] = -1;
    m->tab[hasta] = id;
    m->pos[id] = hasta;
    if (!id) atomic_store(&m->vista_puma, hasta);
    snprintf(ev, sizeof ev, "%s %s -> %s%s", quien, nombre(desde, a), nombre(hasta, b), extra);
    m->movs++;
    imprimir_tablero(m, ev);
}

static void mover_puma(Mesa *m, unsigned *s) {
    int dest[16], cap[16], idx[16], n = movs_puma(m, dest, cap), hay_cap = 0, c = 0;
    for (int i = 0; i < n; i++) hay_cap |= cap[i] >= 0;
    for (int i = 0; i < n; i++)
        if ((cap[i] >= 0) == hay_cap) idx[c++] = i;
    if (c) { int k = idx[rand_r(s) % (unsigned)c]; aplicar(m, 0, dest[k], cap[k]); }
}

static void mover_perro(Mesa *m, int id, int destino) {
    char a[16], b[16];
    if (m->tab[destino] < 0 && es_ady(m->pos[id], destino)) aplicar(m, id, destino, -1);
    else registrar("[Partida %d] RECHAZADO perro %d: %s -> %s (casilla ocupada)\n", m->id, id, nombre(m->pos[id], a), nombre(destino, b));
}

static int elegir_destino(const Mesa *m, int id, unsigned *s) {
    int pos = m->pos[id], n = nvec[pos], mejor = vec[pos][rand_r(s) % (unsigned)n];
    int pu = atomic_load(&m->vista_puma);
    for (int i = 0; i < n; i++)
        if (dist[vec[pos][i]][pu] < dist[mejor][pu]) mejor = vec[pos][i];
    return mejor;
}

static void *hilo_ficha(void *arg) {
    Pieza *p = arg;
    Mesa *m = p->m;
    int id = p->id;
    for (;;) {
        dormir_ms(retardo(&p->seed));
        int destino = id ? elegir_destino(m, id, &p->seed) : -1;
        pthread_mutex_lock(&m->mtx);
        if (m->fin || !m->viva[id]) {
            pthread_mutex_unlock(&m->mtx);
            return NULL;
        }
        if (id) mover_perro(m, id, destino); else mover_puma(m, &p->seed);
        revisar(m);
        pthread_mutex_unlock(&m->mtx);
    }
}

int main(int argc, char **argv) {
    if (argc < 3 || argc > 4 || (g_N = atoi(argv[1])) < 1 || g_N > 100000 || (g_M = atoi(argv[2])) < 1 || g_M > 100000) {
        fprintf(stderr, "Uso: %s N M [config.txt]\n", argv[0]);
        return 1;
    }
    for (int i = 0; i < NP; i++) cfg.perro[i] = -1;
    if (argc == 4 && cargar_config(argv[3])) return 1;
    construir_grafo();
    registrar("[Servidor] %d jugadores, máximo %d mesas (seed=%u, retardo %d-%d ms)\n", g_N, g_M, cfg.seed, cfg.dmin, cfg.dmax);

    Mesa m = {0};
    char txt[160];
    m.id = 1;
    pthread_mutex_init(&m.mtx, NULL);
    pthread_cond_init(&m.cv, NULL);
    inicializar(&m);
    imprimir_tablero(&m, "INICIO");
    for (int i = 0; i < NP; i++) {
        m.pz[i] = (Pieza){&m, i, cfg.seed + (unsigned)i};
        hilo(&m.th[i], hilo_ficha, &m.pz[i]);
    }
    pthread_mutex_lock(&m.mtx);
    while (!m.fin) pthread_cond_wait(&m.cv, &m.mtx);
    pthread_mutex_unlock(&m.mtx);
    for (int i = 0; i < NP; i++) pthread_join(m.th[i], NULL);

    snprintf(txt, sizeof txt, "FIN ganador=%s | %s | capturas=%d, movimientos=%d",
             m.ganador == PUMA ? "PUMA" : m.ganador == PERROS ? "PERROS" : "nadie", m.motivo, m.capturas, m.movs);
    imprimir_tablero(&m, txt);
    pthread_cond_destroy(&m.cv);
    pthread_mutex_destroy(&m.mtx);
    return 0;
}
