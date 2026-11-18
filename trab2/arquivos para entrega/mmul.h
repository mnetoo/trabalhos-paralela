/*
 * CI1316 - Programacao Paralela - Trabalho 2: Multiplicacao de Matrizes com MPI
 *
 * Autores:
 *   Henrique Villela Silva (GRR20242612)
 *   Marcus Sebastiao Adriano Rocha Neto (GRR20240710)
 *
 * Arquivo: mmul.h
 * Descricao:
 *   Definicoes, constantes e prototipos para a multiplicacao paralela de
 *   matrizes em precisao dupla (double) com MPI (Versao A direta e Versao B blocada).
 */

#ifndef MMUL_H
#define MMUL_H

#include <stddef.h>

/* Dimensoes padrao especificadas no enunciado */
#define DEFAULT_NLA     2304
#define DEFAULT_M       960
#define DEFAULT_NCB     960
#define DEFAULT_BLOCK_W 192

/* Semente fixa para reproducibilidade na geracao de A e B */
#define MMUL_RAND_SEED  1

/*
 * gera_matriz -- preenche uma matriz com inteiros pequenos em [-4, 4]
 *   X  vetor de n doubles (a matriz inteira, armazenada por linhas)
 *   n  numero de elementos: linhas x colunas
 */
void gera_matriz(double *X, size_t n);

/*
 * verifica_resultado -- confere C = A x B contra uma multiplicacao sequencial
 *   A [Nla x M]   matriz A completa
 *   B [M x Ncb]   matriz B completa
 *   C [Nla x Ncb] resultado a conferir (ja reunido pelo MPI_Gather/MPI_Gatherv)
 *   Retorna o numero de elementos de C diferentes da referencia (0 = correto).
 */
size_t verifica_resultado(int Nla, int M, int Ncb, const double *A,
                          const double *B, const double *C);

/*
 * mmul_kernel_versao_A -- kernel local da Versao A (direta/simples, sem tiling)
 *   Executa os 3 lacos classicos (i -> j -> k) multiplicando a faixa local
 *   A_local [nl x M] pela matriz B [M x Ncb], gravando em C_local [nl x Ncb].
 */
void mmul_kernel_versao_A(int nl, int M, int Ncb,
                          const double * restrict A_local,
                          const double * restrict B,
                          double * restrict C_local);

/*
 * mmul_kernel_versao_B -- kernel local da Versao B (blocada com ladrilhos w x w)
 *   Executa os lacos externos de blocos (ii -> kk -> jj) e os lacos internos
 *   escalares (i -> k -> j) em ordem row-major para maximizar o reuso de cache.
 */
void mmul_kernel_versao_B(int nl, int M, int Ncb, int w,
                          const double * restrict A_local,
                          const double * restrict B,
                          double * restrict C_local);

#endif /* MMUL_H */
