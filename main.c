#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h> 
#include <pthread.h>
#include <string.h>

#define ROJO     "\033[31m"
#define VERDE   "\033[32m"
#define RESET   "\033[0m"

struct memoria {
    float tFisica_MB;
    float tLogica_MB;
    float tPagina_KB;
    int numMarcosFisicos;
    int numPaginasLogicas;
    int numMarcosSwap;
};

struct Pagina {            
    int presente;        
    int marcoIndice;      
    int swapIndice;       
};

struct proceso {
    int idProceso;
    float tamaño_bytes;
    int demandaTotalPaginas;
    struct Pagina *paginas;  
    int paginasAsignadas;   
};

struct Marco {           
    int ocupado;         
    int processId;       
    int pagIndice;       
};

struct Swap {        
    int ocupado;         
    int processId;
    int pagIndice;
};

struct memoria MEM;
struct proceso *procesos = NULL;  
int cantProcesos = 0;
struct Marco *RAM = NULL;          
struct Swap *SWAP = NULL;
int *fifo = NULL;     
int fifo_head = 0, fifo_cola = 0, fifo_contador = 0; 
pthread_mutex_t mutex_mem = PTHREAD_MUTEX_INITIALIZER;
int maxMarcos = 0;
int simulacion_terminada = 0;
long minProcesoBytes = 1 * 1024;    
long maxProcesoBytes = 100 * 1024;  

long bytes_por_pagina() {
    return (long)(MEM.tPagina_KB * 1024.0f);
}

void fifo_push(int marcoId) {
    if (fifo_contador == MEM.numMarcosFisicos) {
        return;
    }
    fifo[fifo_cola] = marcoId;
    fifo_cola = (fifo_cola + 1) % MEM.numMarcosFisicos;
    fifo_contador++;
}

int fifo_pop() {
    if (fifo_contador == 0) return -1;
    int val = fifo[fifo_head];
    fifo_head = (fifo_head + 1) % MEM.numMarcosFisicos;
    fifo_contador--;
    return val;
}

int eliminar_marco_fifo(int marcoId) {
     if (fifo_contador == 0) return 0;
    int n = fifo_contador;
    int nueva_cabeza = fifo_head;
    int nueva_cola = fifo_head;
    int mantener = 0;
    for (int i = 0; i < n; i++) {
        int id = fifo[(fifo_head + i) % MEM.numMarcosFisicos];
        if (id != marcoId) {
            fifo[nueva_cola] = id;
            nueva_cola = (nueva_cola + 1) % MEM.numMarcosFisicos;
            mantener++;
        }
    }
    fifo_head = nueva_cabeza;
    fifo_cola = nueva_cola;
    fifo_contador = mantener;
    return 1;
}

int encontrar_marco_libre() {
    for (int i = 0; i < MEM.numMarcosFisicos; i++) {
        if (!RAM[i].ocupado) return i;
    }
    return -1;
}

int encontrar_swap_libre() {
    for (int i = 0; i < MEM.numMarcosSwap; i++) {
        if (!SWAP[i].ocupado) return i;
    }
    return -1;
}

void imprimir_mapa_memoria() {
    printf(ROJO "\n--- MAPA MEMORIA (RAM) ---\n" RESET);
    for (int i = 0; i < MEM.numMarcosFisicos; i++) {
        if (RAM[i].ocupado) {
            printf(ROJO "Marco %3d: P%d:p%d\n" RESET, i, RAM[i].processId, RAM[i].pagIndice);
        } else {
            printf(ROJO "Marco %3d: LIBRE\n" RESET, i);
        }
    }
    printf(VERDE "--- SWAP ---\n" RESET);
    for (int i = 0; i < MEM.numMarcosSwap; i++) {
        if (SWAP[i].ocupado) {
            printf(VERDE "Swap %3d: P%d:p%d\n" RESET, i, SWAP[i].processId, SWAP[i].pagIndice);
        } else {
            printf( VERDE "Swap %3d: LIBRE\n" RESET, i);
        }
    }
}

int swap_out_frame(int marcoId) {
    int pid = RAM[marcoId].processId;
    int pagId = RAM[marcoId].pagIndice;

    int swap = encontrar_swap_libre();
    if (swap == -1) {
        return 1; 
    }

    SWAP[swap].ocupado = 1;
    SWAP[swap].processId = pid;
    SWAP[swap].pagIndice= pagId;

    for (int i = 0; i < cantProcesos; i++) {
        if (procesos[i].idProceso == pid) {
            procesos[i].paginas[pagId].presente = 0;
            procesos[i].paginas[pagId].marcoIndice = -1;
            procesos[i].paginas[pagId].swapIndice = swap;
            break;
        }
    }

    RAM[marcoId].ocupado = 0;
    RAM[marcoId].processId = -1;
    RAM[marcoId].pagIndice = -1;

    eliminar_marco_fifo(marcoId);

    return 0;
}

int swap_in_page(int procId, int pagId) {

    struct Pagina *p = &procesos[procId].paginas[pagId];

    if (p->presente == 1) return 0;

    int marco = encontrar_marco_libre();

    if (marco == -1) {
        int cambio = fifo_pop();
        if (cambio == -1) {
            return 1;
        }
        int res = swap_out_frame(cambio);
        if (res == 1) return 1; 
        marco = cambio; 
    }

    int swapId = p->swapIndice;
    if (swapId >= 0) {
        SWAP[swapId].ocupado = 0;
        SWAP[swapId].processId = -1;
        SWAP[swapId].pagIndice = -1;
        p->swapIndice = -1;
    }

    RAM[marco].ocupado = 1;
    RAM[marco].processId = procesos[procId].idProceso;
    RAM[marco].pagIndice = pagId;

    p->presente = 1;
    p->marcoIndice = marco;

    fifo_push(marco);

    return 0;
}

void crear_proceso(int id, long tamaño_bytes) {
    pthread_mutex_lock(&mutex_mem);
    struct proceso *tmp = realloc(procesos, sizeof(struct proceso) * (cantProcesos + 1));
    procesos = tmp;

    struct proceso *proc = &procesos[cantProcesos];
    proc->idProceso = id;
    proc->tamaño_bytes = (float)tamaño_bytes;
    float pagina_b = (float)bytes_por_pagina();
    float division = proc->tamaño_bytes / pagina_b;
    proc->demandaTotalPaginas = (int)ceil(division);
    proc->paginasAsignadas = 0;

    proc->paginas = calloc(proc->demandaTotalPaginas, sizeof(struct Pagina));
    for (int i = 0; i < proc->demandaTotalPaginas; i++) {
        proc->paginas[i].presente = 0;
        proc->paginas[i].marcoIndice = -1;
        proc->paginas[i].swapIndice = -1;
    }

    printf("\nProceso id %d creado. Tamaño:%.2f KB, páginas:%d\n", proc->idProceso, proc->tamaño_bytes / 1024.0f, proc->demandaTotalPaginas);

    for (int i = 0; i < proc->demandaTotalPaginas; i++) {
        if (simulacion_terminada) break;

        int marco = encontrar_marco_libre();
        if (marco != -1) {
            RAM[marco].ocupado = 1;
            RAM[marco].processId = proc->idProceso;
            RAM[marco].pagIndice = i;

            proc->paginas[i].presente = 1;
            proc->paginas[i].marcoIndice = marco;
            proc->paginas[i].swapIndice = -1;

            fifo_push(marco);
            proc->paginasAsignadas++;
            printf("Página %d del P%d -> RAM marco %d\n", i, proc->idProceso, marco);
            continue;
        }

        int swapId = encontrar_swap_libre();
        if (swapId != -1) {
            SWAP[swapId].ocupado = 1;
            SWAP[swapId].processId = proc->idProceso;
            SWAP[swapId].pagIndice = i;

            proc->paginas[i].presente = 0;
            proc->paginas[i].marcoIndice = -1;
            proc->paginas[i].swapIndice = swapId;

            proc->paginasAsignadas++;
            printf("Página %d del P%d -> SWAP slot %d\n", i, proc->idProceso, swapId);
            continue;
        }

        printf("\nNo hay espacio en SWAP ni RAM para asignar la página %d del proceso %d. Terminando simulación.\n", i, proc->idProceso);
        simulacion_terminada = 1;
        break;
    }

    cantProcesos++;
    pthread_mutex_unlock(&mutex_mem);
}

void eliminar_proceso_aleatorio() {
    pthread_mutex_lock(&mutex_mem);
    if (cantProcesos == 0) {
        pthread_mutex_unlock(&mutex_mem);
        return;
    }
    int id = rand() % cantProcesos;
    struct proceso proc = procesos[id];

    printf("\nEliminando proceso id:%d\n", proc.idProceso);

    for (int p = 0; p < proc.demandaTotalPaginas; p++) {
        if (proc.paginas[p].presente == 1) {
            int m = proc.paginas[p].marcoIndice;
            if (m >= 0 && m < MEM.numMarcosFisicos) {
                RAM[m].ocupado = 0;
                RAM[m].processId = -1;
                RAM[m].pagIndice = -1;
                eliminar_marco_fifo(m);
            }
        } else {
            int s = proc.paginas[p].swapIndice;
            if (s >= 0 && s < MEM.numMarcosSwap) {
                SWAP[s].ocupado = 0;
                SWAP[s].processId = -1;
                SWAP[s].pagIndice = -1;
            }
        }
    }

    free(proc.paginas);

    for (int i = id; i < cantProcesos - 1; i++) {
        procesos[i] = procesos[i + 1];
    }
    cantProcesos--;

    pthread_mutex_unlock(&mutex_mem);
}

void acceder_direccion_virtual() {
    pthread_mutex_lock(&mutex_mem);
    if (cantProcesos == 0) {
        printf("\nNo hay procesos para acceder.\n");
        pthread_mutex_unlock(&mutex_mem);
        return;
    }
    int pid= rand() % cantProcesos;
    struct proceso *proc = &procesos[pid];

    int pagId = rand() % proc->demandaTotalPaginas;
    long offset = rand() % bytes_por_pagina();
    printf("\nAccediendo a P%d:p%d offset %ld (virtual)\n", proc->idProceso, pagId, offset);

    if (proc->paginas[pagId].presente == 1) {
        printf("Página en RAM (marco %d). No hay page fault.\n", proc->paginas[pagId].marcoIndice);
        pthread_mutex_unlock(&mutex_mem);
        return;
    }

    printf("PAGE FAULT: Página P%d:p%d no está en RAM.\n", proc->idProceso, pagId);

    int res = swap_in_page(pid, pagId);
    if (res == 1) {
        printf("\nNo hay espacio en SWAP para realizar swap. Terminando simulación.\n");
        simulacion_terminada = 1;
    } else {
        printf("P%d:p%d cargada en marco %d\n", proc->idProceso, pagId, proc->paginas[pagId].marcoIndice);
    }

    pthread_mutex_unlock(&mutex_mem);
}

void *hilo_creador(void *arg) {
    int proxId = 1;
    while (!simulacion_terminada) {
        long tam = (rand() % (maxProcesoBytes - minProcesoBytes + 1)) + minProcesoBytes;
        crear_proceso(proxId, tam);
        proxId++;

        if (simulacion_terminada) break;

        pthread_mutex_lock(&mutex_mem);
        imprimir_mapa_memoria();
        pthread_mutex_unlock(&mutex_mem);

        sleep(2);
    }
    return NULL;
}

void *hilo_eliminador(void *arg) {
    sleep(30);
    while (!simulacion_terminada) {      
        eliminar_proceso_aleatorio();
        pthread_mutex_lock(&mutex_mem);
        imprimir_mapa_memoria();
        pthread_mutex_unlock(&mutex_mem);
        sleep(5);
    }
    return NULL;
}

void *hilo_accesos(void *arg) {
    sleep(30);
    while (!simulacion_terminada) {
        ;
        acceder_direccion_virtual();
        pthread_mutex_lock(&mutex_mem);
        imprimir_mapa_memoria();
        pthread_mutex_unlock(&mutex_mem);
        sleep(5);
    }
    return NULL;
}

int main() {
    srand(time(NULL));

    printf("Ingrese tamaño de memoria fisica (MB): ");
    if (scanf("%f", &MEM.tFisica_MB) != 1) return 1;

    float r = ((float)rand() / RAND_MAX) * 3.0f + 1.5f;
    MEM.tLogica_MB = MEM.tFisica_MB * r;

    printf("Memoria virtual: %.2f MB\n", MEM.tLogica_MB);

    printf("Ingrese tamaño de pagina (KB): ");
    if (scanf("%f", &MEM.tPagina_KB) != 1) return 1;

    long tFisBytes = (long)(MEM.tFisica_MB * 1024.0f * 1024.0f);
    long tLogBytes = (long)(MEM.tLogica_MB * 1024.0f * 1024.0f);
    long tPagBytes = bytes_por_pagina();

    MEM.numMarcosFisicos = (int)(tFisBytes / tPagBytes);
    MEM.numPaginasLogicas = (int)ceil((double)tLogBytes / (double)tPagBytes);
    MEM.numMarcosSwap = (int)(MEM.numMarcosFisicos * 1.5f);

    maxMarcos = MEM.numMarcosFisicos + MEM.numMarcosSwap;

    printf("\nDatos de memoria\n");
    printf("RAM: %.2f MB -> %d marcos\n", MEM.tFisica_MB, MEM.numMarcosFisicos);
    printf("Virtual: %.2f MB -> %d paginas\n", MEM.tLogica_MB, MEM.numPaginasLogicas);
    printf("SWAP: %d marcos\n", MEM.numMarcosSwap);


    printf("\nIngrese tamaño mínimo proceso (KB): ");
    long minKB, maxKB;
    if (scanf("%ld", &minKB) != 1) return 1;
    printf("Ingrese tamaño máximo proceso (KB): ");
    if (scanf("%ld", &maxKB) != 1) return 1;
    minProcesoBytes = minKB * 1024;
    maxProcesoBytes = maxKB * 1024;
    if (minProcesoBytes < 1) minProcesoBytes = 1;
    if (maxProcesoBytes < minProcesoBytes) maxProcesoBytes = minProcesoBytes * 2;

    RAM = calloc(MEM.numMarcosFisicos, sizeof(struct Marco));
    SWAP = calloc(MEM.numMarcosSwap, sizeof(struct Swap));
    fifo= calloc(MEM.numMarcosFisicos, sizeof(int));

    for (int i = 0; i < MEM.numMarcosFisicos; i++) {
        RAM[i].ocupado = 0;
        RAM[i].processId = -1;
        RAM[i].pagIndice = -1;
    }
    for (int i = 0; i < MEM.numMarcosSwap; i++) {
        SWAP[i].ocupado = 0;
        SWAP[i].processId = -1;
        SWAP[i].pagIndice = -1;
    }

    pthread_t t_creador, t_elim, t_acc;
    pthread_create(&t_creador, NULL, hilo_creador, NULL);
    pthread_create(&t_elim, NULL, hilo_eliminador, NULL);
    pthread_create(&t_acc, NULL, hilo_accesos, NULL);

    while (!simulacion_terminada) {
        sleep(1);
    }

    pthread_cancel(t_creador);
    pthread_cancel(t_elim);
    pthread_cancel(t_acc);

    pthread_mutex_lock(&mutex_mem);
    for (int i = 0; i < cantProcesos; i++) {
        free(procesos[i].paginas);
    }
    free(procesos);
    free(RAM);
    free(SWAP);
    free(fifo);
    pthread_mutex_unlock(&mutex_mem);

    return 0;
}
