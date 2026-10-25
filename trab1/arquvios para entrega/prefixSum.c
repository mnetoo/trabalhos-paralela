/*
 * CI1316 - Programação Paralela - Trabalho 1: Prefix Sum Paralelo com Pthreads
 *
 * Autores:
 *   Henrique Villela Silva (GRR20242612)
 *   Marcus Sebastião Adriano Rocha Neto (GRR20240710)
 */

#define _GNU_SOURCE

#include "prefixSum.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <unistd.h>

// Mapeamento dos cores
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

// Variaveis do pool de threads
static pthread_t threads[MAX_THREADS];
static pthread_attr_t attr[MAX_THREADS];
static int thread_ids[MAX_THREADS];

static int pool_initialized = 0;
static int pool_nThreads = 0;
static volatile int pool_shutdown = 0;

// Sincronizacao:
// pool_barrier: barreira 1 (libera as threads do pool)
// algo_barrier: barreira 2 (espera as somas parciais ficarem prontas)
// round_barrier: sincroniza a troca de rodada quando usa chunks
// threads_completed: contador atomico para substituir a 3a barreira no final
static pthread_barrier_t pool_barrier;
static pthread_barrier_t algo_barrier;
static pthread_barrier_t round_barrier;
static atomic_int threads_completed;

// Argumentos compartilhados com as threads
static volatile TYPE *g_Vec = NULL;
static long g_nTotalElmts = 0;
static long g_chunkSize = 0;
static int g_nThreads = 0;

// Vetor auxiliar de somas parciais (1 posicao por thread)
static TYPE PartialSum[MAX_THREADS];

// Funcao das threads trabalhadoras (1 .. nThreads-1)
static void *workerKernel(void *arg) {
    int myIndex = *((int *)arg);

    while (1) {
        // Espera a thread 0 liberar o inicio do trabalho
        pthread_barrier_wait(&pool_barrier);

        if (pool_shutdown) {
            break;
        }

        long nTotalElmts = g_nTotalElmts;
        long chunkSize = g_chunkSize;
        int nThreads = g_nThreads;

        // Se chunkSize == 0, divide o vetor em 1 unico bloco por thread
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

            // Fase 1: calcula a soma parcial da faixa e salva em PartialSum
            TYPE myPartialSum = 0;
            if (first <= last) {
                for (long k = first; k <= last; k++) {
                    myPartialSum += g_Vec[k];
                }
            }
            PartialSum[myIndex] = myPartialSum;

            // Espera todas as threads gravarem suas somas parciais
            pthread_barrier_wait(&algo_barrier);

            // Fase 2: soma os prefixos das threads anteriores e atualiza o vetor in-place
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

            // Se tiver mais rodadas de chunks, acumula a soma da rodada e sincroniza
            if (r + 1 < totalRounds) {
                TYPE roundTotal = 0;
                for (int k = 0; k < nThreads; k++) {
                    roundTotal += PartialSum[k];
                }
                myBase += roundTotal;

                pthread_barrier_wait(&round_barrier);
            }
        }

        // Avisa a thread 0 que terminou os calculos
        atomic_fetch_add_explicit(&threads_completed, 1, memory_order_release);
    }

    return NULL;
}

// Cria o pool de threads e fixa afinidade por core
static void initThreadPool(int nThreads) {
    pool_nThreads = nThreads;
    pool_shutdown = 0;
    atomic_store_explicit(&threads_completed, 0, memory_order_relaxed);

    pthread_barrier_init(&pool_barrier, NULL, (unsigned int)nThreads);
    pthread_barrier_init(&algo_barrier, NULL, (unsigned int)nThreads);
    pthread_barrier_init(&round_barrier, NULL, (unsigned int)nThreads);

#if defined(__linux__)
    int total_logical_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (total_logical_cores <= 0) {
        total_logical_cores = 1;
    }

    cpu_set_t cpuset0;
    CPU_ZERO(&cpuset0);
    CPU_SET(physical_core_map[0] % total_logical_cores, &cpuset0);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset0);
#endif

    thread_ids[0] = 0;

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

    if (pool_initialized && pool_nThreads != nThreads) {
        ParallelPrefixSumCleanup();
    }

    if (!pool_initialized) {
        initThreadPool(nThreads);
    }

    g_Vec = Vec;
    g_nTotalElmts = nTotalElmts;
    g_chunkSize = chunkSize;
    g_nThreads = nThreads;

    atomic_store_explicit(&threads_completed, 0, memory_order_relaxed);

    // Libera as threads do pool para comecarem a rodada
    pthread_barrier_wait(&pool_barrier);

    long effChunk = (chunkSize > 0) ? chunkSize : ((nTotalElmts + (long)nThreads - 1) / (long)nThreads);
    long roundStride = (long)nThreads * effChunk;
    long totalRounds = (nTotalElmts + roundStride - 1) / roundStride;

    TYPE myBase = 0;
    for (long r = 0; r < totalRounds; r++) {
        long roundOffset = r * roundStride;
        long first = roundOffset; // indice da thread 0
        long last = first + effChunk - 1;
        if (last >= nTotalElmts) {
            last = nTotalElmts - 1;
        }

        // Fase 1 da thread 0
        TYPE myPartialSum = 0;
        if (first <= last) {
            for (long k = first; k <= last; k++) {
                myPartialSum += Vec[k];
            }
        }
        PartialSum[0] = myPartialSum;

        // Sincroniza somas parciais
        pthread_barrier_wait(&algo_barrier);

        // Fase 2 da thread 0
        TYPE myPrefixSum = myBase;
        if (first <= last) {
            TYPE running = myPrefixSum;
            for (long k = first; k <= last; k++) {
                running += Vec[k];
                Vec[k] = running;
            }
        }

        if (r + 1 < totalRounds) {
            TYPE roundTotal = 0;
            for (int k = 0; k < nThreads; k++) {
                roundTotal += PartialSum[k];
            }
            myBase += roundTotal;

            pthread_barrier_wait(&round_barrier);
        }
    }

    // Espera todas as outras threads terminarem antes de retornar
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

    // Libera as threads que estao paradas na barreira do pool para sairem do while(1)
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
