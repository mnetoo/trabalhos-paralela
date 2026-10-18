#define _GNU_SOURCE

#include "prefixSum.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <unistd.h>

/* Vetor de mapeamento de nucleos fisicos para priorizar nucleos distintos */
static const int physical_core_map[MAX_THREADS] __attribute__((unused)) = {
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

/* Estruturas de Sincronizacao:
 * - pool_barrier: BARREIRA 1 do Pool (disparo de cada chamada de ParallelPrefixSumPth)
 * - algo_barrier: BARREIRA 2 do Algoritmo (somas parciais prontas no vetor PartialSum)
 * - round_barrier: sincronizacao entre rodadas consecutivas de chunks (usada se chunkSize > 0)
 * - threads_completed: contador atomico lock-free para finalizacao suave (elimina a barreira de termino!)
 */
static pthread_barrier_t pool_barrier;
static pthread_barrier_t algo_barrier;
static pthread_barrier_t round_barrier;
static atomic_int threads_completed;

/* Parametros Globais do Kernel */
static volatile TYPE *g_Vec = NULL;
static long g_nTotalElmts = 0;
static long g_chunkSize = 0;
static int g_nThreads = 0;

/* Vetor Auxiliar Global de Somas Parciais (1 elemento por thread) */
static TYPE PartialSum[MAX_THREADS];

/**
 * @brief Funcao executada pelas worker threads (threads 1 .. nThreads-1)
 *        no Pool de Threads com ciclo de vida continuo.
 */
static void *workerKernel(void *arg) {
    int myIndex = *((int *)arg);

    while (1) {
        /* BARREIRA 1 (Pool): Aguarda sinal da thread chamadora */
        pthread_barrier_wait(&pool_barrier);

        if (pool_shutdown) {
            break;
        }

        long nTotalElmts = g_nTotalElmts;
        long chunkSize = g_chunkSize;
        int nThreads = g_nThreads;

        long effChunk = (chunkSize > 0) ? chunkSize : ((nTotalElmts + (long)nThreads - 1) / (long)nThreads);
        long roundStride = (long)nThreads * effChunk;
        long totalRounds = (nTotalElmts + roundStride - 1) / roundStride;

        TYPE myBase = 0;
        for (long r = 0; r < totalRounds; r++) {
            long roundOffset = r * roundStride;
            long first = roundOffset + (long)myIndex * effChunk;
            long last = first + effChunk - 1;
            if (last >= nTotalElmts) {
                last = nTotalElmts - 1;
            }

            /* FASE 1: Soma Parcial do chunk desta rodada */
            TYPE myPartialSum = 0;
            if (first <= last) {
                for (long k = first; k <= last; k++) {
                    myPartialSum += g_Vec[k];
                }
            }
            PartialSum[myIndex] = myPartialSum;

            /* BARREIRA 2 (Algoritmo): Aguarda somas parciais de todas as threads */
            pthread_barrier_wait(&algo_barrier);

            /* FASE 2: Soma de Prefixos In-Place do chunk mantido no cache L2 */
            TYPE myPrefixSum = myBase;
            for (int k = 0; k < myIndex; k++) {
                myPrefixSum += PartialSum[k];
            }

            if (first <= last) {
                TYPE running = myPrefixSum;
                for (long k = first; k <= last; k++) {
                    running += g_Vec[k];
                    g_Vec[k] = running;
                }
            }

            /* Se houver proxima rodada de chunks, sincroniza */
            if (r + 1 < totalRounds) {
                TYPE roundTotal = 0;
                for (int k = 0; k < nThreads; k++) {
                    roundTotal += PartialSum[k];
                }
                myBase += roundTotal;

                pthread_barrier_wait(&round_barrier);
            }
        }

        /* SINALIZACAO DE TERMINO LOCK-FREE: Incrementa atomico com release */
        atomic_fetch_add_explicit(&threads_completed, 1, memory_order_release);
    }

    return NULL;
}

/**
 * @brief Inicializa o pool de threads e configura a afinidade de nucleos.
 */
static void initThreadPool(int nThreads) {
    pool_nThreads = nThreads;
    pool_shutdown = 0;
    atomic_store_explicit(&threads_completed, 0, memory_order_relaxed);

    /* Inicializa as barreiras para nThreads */
    pthread_barrier_init(&pool_barrier, NULL, (unsigned int)nThreads);
    pthread_barrier_init(&algo_barrier, NULL, (unsigned int)nThreads);
    pthread_barrier_init(&round_barrier, NULL, (unsigned int)nThreads);

#if defined(__linux__)
    int total_logical_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (total_logical_cores <= 0) {
        total_logical_cores = 1;
    }

    /* Fixa a thread 0 (main) no primeiro nucleo da topologia */
    cpu_set_t cpuset0;
    CPU_ZERO(&cpuset0);
    CPU_SET(physical_core_map[0] % total_logical_cores, &cpuset0);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset0);
#endif

    thread_ids[0] = 0;

    /* Cria as worker threads (1 .. nThreads-1) com afinidade de core */
    for (int i = 1; i < nThreads; i++) {
        thread_ids[i] = i;
        pthread_attr_init(&attr[i]);

#if defined(__linux__)
        int core_id = physical_core_map[i] % total_logical_cores;
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);
        pthread_attr_setaffinity_np(&attr[i], sizeof(cpu_set_t), &cpuset);
#endif

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

    /* Se o pool ja existe com numero diferente de threads, reinicializa */
    if (pool_initialized && pool_nThreads != nThreads) {
        ParallelPrefixSumCleanup();
    }

    if (!pool_initialized) {
        initThreadPool(nThreads);
    }

    /* Define parametros globais da iteracao */
    g_Vec = Vec;
    g_nTotalElmts = nTotalElmts;
    g_chunkSize = chunkSize;
    g_nThreads = nThreads;

    /* Reseta contador de termino das workers antes de libera-las */
    atomic_store_explicit(&threads_completed, 0, memory_order_relaxed);

    /* BARREIRA 1 (Pool): Libera a execucao das worker threads */
    pthread_barrier_wait(&pool_barrier);

    long effChunk = (chunkSize > 0) ? chunkSize : ((nTotalElmts + (long)nThreads - 1) / (long)nThreads);
    long roundStride = (long)nThreads * effChunk;
    long totalRounds = (nTotalElmts + roundStride - 1) / roundStride;

    TYPE myBase = 0;
    for (long r = 0; r < totalRounds; r++) {
        long roundOffset = r * roundStride;
        long first = roundOffset; /* myIndex = 0 */
        long last = first + effChunk - 1;
        if (last >= nTotalElmts) {
            last = nTotalElmts - 1;
        }

        /* FASE 1: Thread 0 processa a sua fracao do chunk */
        TYPE myPartialSum = 0;
        if (first <= last) {
            for (long k = first; k <= last; k++) {
                myPartialSum += Vec[k];
            }
        }
        PartialSum[0] = myPartialSum;

        /* BARREIRA 2 (Algoritmo): Aguarda todas as threads salvarem somas parciais */
        pthread_barrier_wait(&algo_barrier);

        /* FASE 2: Thread 0 calcula a soma de prefixos in-place no seu chunk */
        TYPE myPrefixSum = myBase;
        if (first <= last) {
            TYPE running = myPrefixSum;
            for (long k = first; k <= last; k++) {
                running += Vec[k];
                Vec[k] = running;
            }
        }

        /* Se houver proxima rodada de chunks, acumula roundTotal e sincroniza */
        if (r + 1 < totalRounds) {
            TYPE roundTotal = 0;
            for (int k = 0; k < nThreads; k++) {
                roundTotal += PartialSum[k];
            }
            myBase += roundTotal;

            pthread_barrier_wait(&round_barrier);
        }
    }

    /* Aguarda workers (1 .. nThreads-1) terminarem a Fase 2 via spin suave com acquire (2 Barreiras!) */
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
    pthread_barrier_destroy(&round_barrier);

    pool_initialized = 0;
    pool_nThreads = 0;
    pool_shutdown = 0;
}
