#ifndef PREFIX_SUM_H
#define PREFIX_SUM_H

#define TYPE long long
#define NTIMES 100
#define MAX_THREADS 64

/**
 * @brief Executa a soma de prefixos paralela no vetor Vec ("in-place")
 *        utilizando um pool de threads, afinidade de núcleos e 2 barreiras.
 *
 * @param Vec Vetor de entrada e saída (modificado in-place).
 * @param nTotalElmts Quantidade total de elementos no vetor.
 * @param chunkSize Tamanho do bloco de cache (0 = chunk único por thread).
 * @param nThreads Número de threads a utilizar (1 .. MAX_THREADS).
 */
void ParallelPrefixSumPth(volatile TYPE *Vec,
                           long nTotalElmts,
                           long chunkSize,
                           int nThreads);

/**
 * @brief Finaliza graciosamente o pool de threads e libera recursos
 *        de sincronização (barreiras).
 */
void ParallelPrefixSumCleanup(void);

#endif // PREFIX_SUM_H
