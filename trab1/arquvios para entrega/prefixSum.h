#ifndef PREFIX_SUM_H
#define PREFIX_SUM_H

#define TYPE long long
#define NTIMES 100
#define MAX_THREADS 64

// Soma de prefixos paralela in-place usando pool de threads
void ParallelPrefixSumPth(volatile TYPE *Vec,
                           long nTotalElmts,
                           long chunkSize,
                           int nThreads);

// Encerra o pool de threads e destroi as barreiras
void ParallelPrefixSumCleanup(void);

#endif // PREFIX_SUM_H
