#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

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

int main(int argc, char **argv) {
    if (argc < 3 || argc > 4 || (g_N = atoi(argv[1])) < 1 || g_N > 100000 || (g_M = atoi(argv[2])) < 1 || g_M > 100000) {
        fprintf(stderr, "Uso: %s N M [config.txt]\n", argv[0]);
        return 1;
    }
    registrar("[Servidor] %d jugadores, máximo %d mesas\n", g_N, g_M);
    return 0;
}