#define _GNU_SOURCE

#include "prefixSum.h"
#include "chrono.c"
#include "verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_cache_and_type_info(void) {
    printf("TYPE: long long (%zu bytes)\n", sizeof(TYPE));

    #if defined(_SC_LEVEL2_CACHE_SIZE)
    long l2 = sysconf(_SC_LEVEL2_CACHE_SIZE);
    if (l2 > 0) {
        printf("L2 cache: %ld bytes (%.2f KB)\n", l2, (double)l2 / 1024.0);
    } else {
        printf("L2 cache: 524288 bytes (512 KB)\n");
    }
    #else
    printf("L2 cache: 524288 bytes (512 KB)\n");
    #endif

    #if defined(_SC_LEVEL3_CACHE_SIZE)
    long l3 = sysconf(_SC_LEVEL3_CACHE_SIZE);
    if (l3 > 0) {
        printf("L3 cache: %ld bytes (%.2f MB)\n", l3, (double)l3 / (1024.0 * 1024.0));
    } else {
        printf("L3 cache: 0 bytes (None / Not detected)\n");
    }
    #else
    printf("L3 cache: 0 bytes (None / Not detected)\n");
    #endif
}

int main(int argc, char *argv[]) {
    long nTotalElements = 0;
    int nThreads = 0;
    long chunkSize = 0;
    int verify_flag = 0;

    if (argc < 4) {
        fprintf(stderr, "usage: %s <nTotalElements> <chunkSize> <nThreads> [-v]\n"
                        "   ou: %s <nTotalElements> <nThreads> <chunkSize> [-v]\n",
                argv[0], argv[0]);
        return 1;
    }

    nTotalElements = atol(argv[1]);
    long arg2 = atol(argv[2]);
    long arg3 = atol(argv[3]);

    /* Deteccao inteligente de ordem de argumentos para suportar:
     * 1) Scripts SLURM do professor: <nElements> <chunkSize> <nThreads>
     * 2) Linha 173 da especificacao: <nElements> <nThreads> <chunkSize>
     */
    if (arg2 > MAX_THREADS && arg3 <= MAX_THREADS) {
        chunkSize = arg2;
        nThreads = (int)arg3;
    } else if (arg2 <= MAX_THREADS && arg3 > MAX_THREADS) {
        nThreads = (int)arg2;
        chunkSize = arg3;
    } else if (arg2 == 0 && arg3 > 0 && arg3 <= MAX_THREADS) {
        /* Script de Exp B: chunkSize=0, nThreads=th */
        chunkSize = 0;
        nThreads = (int)arg3;
    } else if (arg3 == 0 && arg2 > 0 && arg2 <= MAX_THREADS) {
        nThreads = (int)arg2;
        chunkSize = 0;
    } else {
        /* Padrao do script SLURM do professor: <nElements> <chunkSize> <nThreads> */
        chunkSize = arg2;
        nThreads = (int)arg3;
    }

    for (int i = 4; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verify_flag = 1;
        }
    }

    if (nTotalElements <= 0) {
        fprintf(stderr, "Erro: <nTotalElements> deve ser positivo (> 0).\n");
        return 1;
    }

    if (nThreads <= 0 || nThreads > MAX_THREADS) {
        fprintf(stderr, "Erro: <nThreads> deve estar entre 1 e %d (recebido: %d).\n", MAX_THREADS, nThreads);
        return 1;
    }

    if (chunkSize < 0) {
        fprintf(stderr, "Erro: <chunkSize> nao pode ser negativo.\n");
        return 1;
    }

    /* Imprime informacoes de tipo e cache esperadas pelos scripts SLURM */
    print_cache_and_type_info();

    /* Alocacao do vetor de trabalho e do vetor de copia inicial */
    volatile TYPE *Vec = (volatile TYPE *)malloc(nTotalElements * sizeof(TYPE));
    TYPE *InitVector = (TYPE *)malloc(nTotalElements * sizeof(TYPE));

    if (Vec == NULL || InitVector == NULL) {
        fprintf(stderr, "Erro: Falha na alocacao de memoria para %ld elementos.\n", nTotalElements);
        if (Vec) free((void *)Vec);
        if (InitVector) free(InitVector);
        return 1;
    }

    /* Inicializacao sequencial com semente fixa */
    srand(42);
    for (long i = 0; i < nTotalElements; i++) {
        int r = rand();
        InitVector[i] = (r % 1000) - 500;
    }

    /* Medicao de tempo rigorosamente conforme a especificacao v1.7:
     * - Loop de NTIMES (100) chamadas
     * - memcpy antes de CADA chamada para manter operacao in-place intacta
     * - Cronometro auxiliar copyChrono medindo o tempo de memcpy
     * - Desconto do tempo de cópia ao final com chrono_decrease_time
     */
    chronometer_t t, copyChrono;
    chrono_reset(&t);
    chrono_reset(&copyChrono);

    chrono_start(&t);

    for (int i = 0; i < NTIMES; i++) {
        chrono_start(&copyChrono);
        memcpy((void *)Vec, (const void *)InitVector, nTotalElements * sizeof(TYPE));
        chrono_stop(&copyChrono);

        ParallelPrefixSumPth(Vec, nTotalElements, chunkSize, nThreads);
    }

    chrono_stop(&t);

    /* Desconta o tempo total gasto com copias de memoria */
    chrono_decrease_time(&t, &copyChrono);

    /* Relatorio de tempo e vazao */
    chrono_report_TimeInLoop(&t, "ParallelPrefixSumPth", NTIMES);

    double total_time_in_seconds = (double)chrono_gettotal(&t) / 1.0e9;
    printf("total_time_in_seconds: %lf s\n", total_time_in_seconds);
    printf("time_per_call_in_seconds: %lf s\n", total_time_in_seconds / (double)NTIMES);

    double total_ops = (double)nTotalElements * (double)NTIMES;
    double OPS = total_ops / total_time_in_seconds;
    printf("Throughput: %lf OP/s\n", OPS);

    double MOPS = OPS / 1.0e6;
    printf("Throughput: %lf MOPS\n", MOPS);

    /* Verificacao sequencial opcional (-v) sem influenciar as medicoes */
    if (verify_flag) {
        memcpy((void *)Vec, (const void *)InitVector, nTotalElements * sizeof(TYPE));
        ParallelPrefixSumPth(Vec, nTotalElements, chunkSize, nThreads);
        verifyPrefixSum(InitVector, Vec, nTotalElements);
    }

    /* Finalizacao graciosa do pool e liberacao de memoria */
    ParallelPrefixSumCleanup();
    free((void *)Vec);
    free(InitVector);

    return 0;
}
