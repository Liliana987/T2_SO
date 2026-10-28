#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LADO 5
#define NG (LADO * LADO)
#define TOP NG
#define TIZQ (NG + 1)
#define TDER (NG + 2)
#define NN (NG + 3)
#define N(x, y) ((y) * LADO + (x))
#define NP 13
#define MAXV 8

typedef struct {
    unsigned seed;
    int dmin, dmax, prob_fallo, puma, perro[NP];
} Config;

typedef struct {
    int id;
    int tab[NN];
    int pos[NP], viva[NP];
    atomic_int vista_puma;
} Mesa;

static Config cfg = {42, 50, 300, 0, N(2, 2), {0}};
static int vec[NN][MAXV], nvec[NN];
static int sal[NN][MAXV][2], nsal[NN];
static int dist[NN][NN];
static int g_N, g_M;
static pthread_mutex_t g_mtx_log = PTHREAD_MUTEX_INITIALIZER;

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
    m.id = 1;
    inicializar(&m);
    imprimir_tablero(&m, "tablero inicial");
    return 0;
}