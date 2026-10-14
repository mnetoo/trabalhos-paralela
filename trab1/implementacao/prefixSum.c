#define _GNU_SOURCE

#include "prefixSum.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <unistd.h>

/* Vetor de mapeamento de núcleos físicos para priorizar núcleos físicos distintos antes de hyperthreads */
static const int physical_core_map[MAX_THREADS] = {
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55,
    56, 57, 58, 59, 60, 61, 62, 63
};

/* Estruturas do Pool de Threads */
static pthread_t threads[MAX_THREADS];
static pthread_attr_t attr[MAX_THREADS];
static int thread_ids[MAX_THREADS];

static int pool_initialized = 0;
static int pool_nThreads = 0;
static volatile int pool_shutdown = 0;

/* Estruturas de Sincronização: 2 Barreiras + Contador Atômico */
static pthread_barrier_t pool_barrier; /* BARREIRA 1: Disparo do bloco de iteração */
static pthread_barrier_t algo_barrier; /* BARREIRA 2: Somas parciais prontas */
static atomic_int threads_completed;   /* Contagem de término da Fase 2 das workers */

/* Parâmetros Globais do Kernel */
static volatile TYPE *g_Vec = NULL;
static long g_nTotalElmts = 0;
static long g_chunkSize = 0;
static int g_nThreads = 0;

/* Vetor Auxiliar Global de Somas Parciais (1 elemento por thread) */
static TYPE PartialSum[MAX_THREADS];

/**
 * @brief Executa a Fase 1 da Soma de Prefixos para a thread myIndex:
 *        Lê a sua fração do vetor Vec e armazena a soma em PartialSum[myIndex].
 */
static void computePhase1(int myIndex, volatile TYPE *Vec, long nTotalElmts, long chunkSize, int nThreads) {
    long nElements = (nTotalElmts + (long)nThreads - 1) / (long)nThreads;
    long first = (long)myIndex * nElements;
    long last = (long)(myIndex + 1) * nElements;
    if (last > nTotalElmts) {
        last = nTotalElmts;
    }
    last--; /* Limite inclusivo: [first .. last] */

    TYPE myPartialSum = 0;
    if (first <= last) {
        long effChunk = (chunkSize > 0) ? chunkSize : (last - first + 1);
        for (long c = first; c <= last; c += effChunk) {
            long cEnd = c + effChunk - 1;
            if (cEnd > last) {
                cEnd = last;
            }
            for (long k = c; k <= cEnd; k++) {
                myPartialSum += Vec[k];
            }
        }
    }
    PartialSum[myIndex] = myPartialSum;
}

/**
 * @brief Executa a Fase 2 da Soma de Prefixos para a thread myIndex:
 *        Lê PartialSum[0 .. myIndex-1] para obter o offset local myPrefixSum
 *        e atualiza o vetor Vec in-place diretamente.
 */
static void computePhase2(int myIndex, volatile TYPE *Vec, long nTotalElmts, long chunkSize, int nThreads) {
    long nElements = (nTotalElmts + (long)nThreads - 1) / (long)nThreads;
    long first = (long)myIndex * nElements;
    long last = (long)(myIndex + 1) * nElements;
    if (last > nTotalElmts) {
        last = nTotalElmts;
    }
    last--; /* Limite inclusivo */

    /* Acumula o offset dos prefixos de todas as threads anteriores */
    TYPE myPrefixSum = 0;
    for (int k = 0; k < myIndex; k++) {
        myPrefixSum += PartialSum[k];
    }

    if (first <= last) {
        TYPE running = myPrefixSum;
        long effChunk = (chunkSize > 0) ? chunkSize : (last - first + 1);
        for (long c = first; c <= last; c += effChunk) {
            long cEnd = c + effChunk - 1;
            if (cEnd > last) {
                cEnd = last;
            }
            for (long k = c; k <= cEnd; k++) {
                running += Vec[k];
                Vec[k] = running;
            }
        }
    }
}

/**
 * @brief Função executada pelas worker threads (threads 1 .. nThreads-1)
 *        no Pool de Threads com ciclo de vida contínuo.
 */
static void *workerKernel(void *arg) {
    int myIndex = *((int *)arg);

    while (1) {
        /* BARREIRA 1 (Pool): Aguarda sinal da thread chamadora */
        pthread_barrier_wait(&pool_barrier);

        if (pool_shutdown) {
            break;
        }

        /* FASE 1: Soma Parcial Pré-Barreira */
        computePhase1(myIndex, g_Vec, g_nTotalElmts, g_chunkSize, g_nThreads);

        /* BARREIRA 2 (Algoritmo): Somas parciais salvas no vetor global */
        pthread_barrier_wait(&algo_barrier);

        /* FASE 2: Soma de Prefixos In-Place Pós-Barreira */
        computePhase2(myIndex, g_Vec, g_nTotalElmts, g_chunkSize, g_nThreads);

        /* SINALIZAÇÃO DE TÉRMINO: Incrementa atômico com semântica release */
        atomic_fetch_add_explicit(&threads_completed, 1, memory_order_release);
    }

    return NULL;
}

/**
 * @brief Inicializa o pool de threads e configura a afinidade de núcleos.
 */
static void initThreadPool(int nThreads) {
    pool_nThreads = nThreads;
    pool_shutdown = 0;
    atomic_store_explicit(&threads_completed, 0, memory_order_relaxed);

    /* Inicializa as 2 barreiras para nThreads */
    pthread_barrier_init(&pool_barrier, NULL, (unsigned int)nThreads);
    pthread_barrier_init(&algo_barrier, NULL, (unsigned int)nThreads);

    int total_logical_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (total_logical_cores <= 0) {
        total_logical_cores = 1;
    }

    /* Fixa a thread 0 (main) no primeiro núcleo da topologia */
    cpu_set_t cpuset0;
    CPU_ZERO(&cpuset0);
    CPU_SET(physical_core_map[0] % total_logical_cores, &cpuset0);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset0);

    thread_ids[0] = 0;

    /* Cria as worker threads (1 .. nThreads-1) com afinidade de core */
    for (int i = 1; i < nThreads; i++) {
        thread_ids[i] = i;
        int core_id = physical_core_map[i] % total_logical_cores;

        pthread_attr_init(&attr[i]);
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);
        pthread_attr_setaffinity_np(&attr[i], sizeof(cpu_set_t), &cpuset);

        pthread_create(&threads[i], &attr[i], workerKernel, &thread_ids[i]);
    }

    pool_initialized = 1;
}

void ParallelPrefixSumPth(volatile TYPE *Vec,
                           long nTotalElmts,
                           long chunkSize,
                           int nThreads) {
    if (nThreads <= 0) return;
    if (nThreads > MAX_THREADS) nThreads = MAX_THREADS;

    /* Se o pool já existe mas com número diferente de threads, reinicializa */
    if (pool_initialized && pool_nThreads != nThreads) {
        ParallelPrefixSumCleanup();
    }

    if (!pool_initialized) {
        initThreadPool(nThreads);
    }

    /* Define parâmetros globais da iteração */
    g_Vec = Vec;
    g_nTotalElmts = nTotalElmts;
    g_chunkSize = chunkSize;
    g_nThreads = nThreads;

    /* Reseta contador de término das workers antes de liberá-las */
    atomic_store_explicit(&threads_completed, 0, memory_order_relaxed);

    /* BARREIRA 1 (Pool): Libera a execução das worker threads */
    pthread_barrier_wait(&pool_barrier);

    /* FASE 1: Thread 0 processa a sua fração */
    computePhase1(0, Vec, nTotalElmts, chunkSize, nThreads);

    /* BARREIRA 2 (Algoritmo): Aguarda todas as threads salvarem somas parciais */
    pthread_barrier_wait(&algo_barrier);

    /* FASE 2: Thread 0 calcula a soma de prefixos in-place na sua faixa */
    computePhase2(0, Vec, nTotalElmts, chunkSize, nThreads);

    /* Aguarda workers (1 .. nThreads-1) terminarem a Fase 2 via spin suave com acquire */
    if (nThreads > 1) {
        while (atomic_load_explicit(&threads_completed, memory_order_acquire) < nThreads - 1) {
            #if defined(__x86_64__) || defined(__i386__)
            __builtin_ia32_pause();
            #endif
        }
    }
}

void ParallelPrefixSumCleanup(void) {
    if (!pool_initialized) return;

    pool_shutdown = 1;

    /* Acorda todas as threads na Barreira 1 para que possam sair */
    pthread_barrier_wait(&pool_barrier);

    for (int i = 1; i < pool_nThreads; i++) {
        pthread_join(threads[i], NULL);
        pthread_attr_destroy(&attr[i]);
    }

    pthread_barrier_destroy(&pool_barrier);
    pthread_barrier_destroy(&algo_barrier);

    pool_initialized = 0;
    pool_nThreads = 0;
    pool_shutdown = 0;
}
