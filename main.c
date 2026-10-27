#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LADO 5
#define N(x, y) ((y) * LADO + (x))
#define NP 13

typedef struct {
    unsigned seed;
    int dmin, dmax, prob_fallo, puma, perro[NP];
} Config;

static Config cfg = {42, 50, 300, 0, N(2, 2), {0}};
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

int main(int argc, char **argv) {
    if (argc < 3 || argc > 4 || (g_N = atoi(argv[1])) < 1 || g_N > 100000 || (g_M = atoi(argv[2])) < 1 || g_M > 100000) {
        fprintf(stderr, "Uso: %s N M [config.txt]\n", argv[0]);
        return 1;
    }
    for (int i = 0; i < NP; i++) cfg.perro[i] = -1;
    if (argc == 4 && cargar_config(argv[3])) return 1;
    registrar("[Servidor] %d jugadores, máximo %d mesas (seed=%u, retardo %d-%d ms)\n", g_N, g_M, cfg.seed, cfg.dmin, cfg.dmax);
    return 0;
}