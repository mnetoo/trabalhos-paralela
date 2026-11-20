/*
 * CI1316 - Programacao Paralela - Trabalho 2: Multiplicacao de Matrizes com MPI
 *
 * Autores:
 *   Henrique Villela Silva (GRR20242612)
 *   Marcus Sebastiao Adriano Rocha Neto (GRR20240710)
 */

#ifndef MMUL_H
#define MMUL_H

#include <stddef.h>

#define DEFAULT_NLA     2304
#define DEFAULT_M       960
#define DEFAULT_NCB     960
#define DEFAULT_BLOCK_W 192

#define MMUL_RAND_SEED  1

void gera_matriz(double *X, size_t n);

size_t verifica_resultado(int Nla, int M, int Ncb, const double *A,
                          const double *B, const double *C);

void mmul_kernel_versao_A(int nl, int M, int Ncb,
                          const double * restrict A_local,
                          const double * restrict B,
                          double * restrict C_local);

void mmul_kernel_versao_B(int nl, int M, int Ncb, int w,
                          const double * restrict A_local,
                          const double * restrict B,
                          double * restrict C_local);

#endif /* MMUL_H */
