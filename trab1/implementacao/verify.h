#ifndef VERIFY_H
#define VERIFY_H

#include "prefixSum.h"

// Verifica sequencialmente se o vetor de saida esta correto
void verifyPrefixSum(const TYPE *InputVec,
                     volatile TYPE *prefixSumVec,
                     long nTotalElmts);

#endif // VERIFY_H
