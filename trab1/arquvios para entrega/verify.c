#include "verify.h"
#include <stdio.h>

void verifyPrefixSum(const TYPE *InputVec,
                     volatile TYPE *prefixSumVec,
                     long nTotalElmts) {
    long errors = 0;
    TYPE running_sum = 0;

    for (long i = 0; i < nTotalElmts; i++) {
        running_sum += InputVec[i];
        if (prefixSumVec[i] != running_sum) {
            if (errors < 10) {
                fprintf(stderr, "[ERRO] Divergencia no indice %ld: esperado %lld, obtido %lld\n",
                        i, (long long)running_sum, (long long)prefixSumVec[i]);
            }
            errors++;
        }
    }

    if (errors == 0) {
        printf(">> Verificacao: SUCESSO (todos os %ld elementos estao corretos)!\n", nTotalElmts);
    } else {
        printf(">> Verificacao: FALHOU com %ld divergencias no total!\n", errors);
    }
}
