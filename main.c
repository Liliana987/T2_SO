#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LADO 5
#define N_GRID (LADO * LADO)        
#define NODO_TOP N_GRID              
#define NODO_LEFT (N_GRID + 1)
#define NODO_RIGHT (N_GRID + 2)
#define N_NODOS (N_GRID + 3)
#define NODO(x, y) ((y) * LADO + (x)) 
#define N_PERROS 12
#define N_PIEZAS (N_PERROS + 1)         
#define CAPTURAS_PARA_GANAR 6
#define MAX_VECINOS 8
#define MAX_MOVIMIENTOS 2000         
#define CELDA_LIBRE (-1)
#define CELDA_PUMA (-2)               

enum { GANA_NADIE, GANA_PUMA, GANA_PERROS };
enum { ROL_NINGUNO, ROL_PUMA, ROL_PERROS };
enum { RES_NINGUNO, RES_GANO, RES_PERDIO, RES_SIN_GANADOR, RES_ERROR };


int main(void) {
    printf("Torneo de Komikan Concurrente\n");
    return 0;
}