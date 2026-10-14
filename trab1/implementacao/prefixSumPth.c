#include "prefixSum.h"
#include "chrono.h"
#include "verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    long nTotalElements;
    int nThreads;
    long chunkSize;
    int verify_flag = 0;

    if (argc < 4) {
        fprintf(stderr, "usage: %s <nTotalElements> <nThreads> <chunkSize> [-v]\n", argv[0]);
        return 1;
    }

    nTotalElements = atol(argv[1]);
    nThreads = atoi(argv[2]);
    chunkSize = atol(argv[3]);

    if (argc >= 5 && strcmp(argv[4], "-v") == 0) {
        verify_flag = 1;
    }

    if (nTotalElements <= 0) {
        fprintf(stderr, "Erro: <nTotalElements> deve ser positivo (> 0).\n");
        return 1;
    }

    if (nThreads <= 0 || nThreads > MAX_THREADS) {
        fprintf(stderr, "Erro: <nThreads> deve estar entre 1 e %d.\n", MAX_THREADS);
        return 1;
    }

    if (chunkSize < 0) {
        fprintf(stderr, "Erro: <chunkSize> nao pode ser negativo.\n");
        return 1;
    }

    /* Alocação do vetor dinâmico de trabalho e do vetor inicial */
    volatile TYPE *Vec = (volatile TYPE *)malloc(nTotalElements * sizeof(TYPE));
    TYPE *InitVector = (TYPE *)malloc(nTotalElements * sizeof(TYPE));

    if (Vec == NULL || InitVector == NULL) {
        fprintf(stderr, "Erro: Falha na alocacao de memoria para %ld elementos.\n", nTotalElements);
        if (Vec) free((void *)Vec);
        if (InitVector) free(InitVector);
        return 1;
    }

    /* Inicialização sequencial com semente fixa para reproducibilidade */
    srand(42);
    for (long i = 0; i < nTotalElements; i++) {
        int r = rand();
        InitVector[i] = (r % 1000) - 500;
        Vec[i] = InitVector[i];
    }

    /* Medição de tempo sobre NTIMES iterações */
    chronometer_t t;
    chrono_reset(&t);
    chrono_start(&t);

    for (int i = 0; i < NTIMES; i++) {
        ParallelPrefixSumPth(Vec, nTotalElements, chunkSize, nThreads);
    }

    chrono_stop(&t);

    /* Relatório padronizado de tempo e vazão */
    chrono_report_TimeInLoop(&t, "ParallelPrefixSumPth", NTIMES);

    double total_time_in_seconds = (double)chrono_gettotal(&t) / 1.0e9;
    printf("total_time_in_seconds: %lf s\n", total_time_in_seconds);
    printf("time_per_call_in_seconds: %lf s\n", total_time_in_seconds / (double)NTIMES);

    double total_ops = (double)nTotalElements * (double)NTIMES;
    double OPS = total_ops / total_time_in_seconds;
    printf("Throughput: %lf OP/s\n", OPS);

    double MOPS = OPS / 1.0e6;
    printf("Throughput: %lf MOPS\n", MOPS);

    /* Verificação sequencial opcional (-v) sem influenciar a medição de tempo */
    if (verify_flag) {
        for (long i = 0; i < nTotalElements; i++) {
            Vec[i] = InitVector[i];
        }
        ParallelPrefixSumPth(Vec, nTotalElements, chunkSize, nThreads);
        verifyPrefixSum(InitVector, Vec, nTotalElements);
    }

    /* Finalização graciosa do pool e liberação de memória */
    ParallelPrefixSumCleanup();
    free((void *)Vec);
    free(InitVector);

    return 0;
}
