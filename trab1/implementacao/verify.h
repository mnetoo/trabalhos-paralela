#ifndef VERIFY_H
#define VERIFY_H

#include "prefixSum.h"

/**
 * @brief Verifica sequencialmente se o vetor prefixSumVec corresponde à soma de prefixos
 *        correta do vetor original InputVec.
 *
 * @param InputVec Vetor com os dados originais (não modificados).
 * @param prefixSumVec Vetor resultante da soma de prefixos paralela.
 * @param nTotalElmts Total de elementos a serem verificados.
 */
void verifyPrefixSum(const TYPE *InputVec,
                     volatile TYPE *prefixSumVec,
                     long nTotalElmts);

#endif // VERIFY_H
