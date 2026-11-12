/*
 * CI1316 - Programacao Paralela - Trabalho 2: Multiplicacao de Matrizes com MPI
 *
 * Autores:
 *   Henrique Villela Silva (GRR20242612)
 *   Marcus Sebastiao Adriano Rocha Neto (GRR20240710)
 *
 * Descricao:
 *   Multiplicacao paralela de matrizes C [Nla x Ncb] = A [Nla x M] * B [M x Ncb]
 *   em precisao dupla (double) utilizando MPI:
 *     - Versao A (direta/simples): sem otimizacao de cache (lacos classicos i -> j -> k).
 *     - Versao B (blocada/tiling): otimizada para cache com ladrilhos w x w
 *       (lacos externos ii -> kk -> jj e internos i -> k -> j em ordem row-major).
 */

#define _GNU_SOURCE

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "chrono.c"

/*
 * gera_matriz -- preenche uma matriz com inteiros pequenos em [-4, 4]
 *   X  vetor de n doubles (a matriz inteira, armazenada por linhas)
 *   n  numero de elementos: linhas x colunas
 *
 * Por que inteiros pequenos guardados em double:
 *   cada produto vale no maximo 16 em modulo, e cada elemento de C no maximo
 *   16*M. Esses valores, e todas as somas parciais, sao inteiros representados
 *   EXATAMENTE em double (bem abaixo de 2^53). Assim o resultado nao depende
 *   da ordem das somas, e a verificacao pode comparar com ==, sem tolerancia,
 *   mesmo que a versao blocada some os termos numa ordem
 *   diferente da sequencial.
 *
 * Regras de uso:
 *   - chamar so no rank 0, antes de enviar para os nodos;
 *   - fixar a semente com srand(1) ANTES da primeira chamada, para que toda
 *     execucao use as mesmas matrizes e um erro possa ser reproduzido.
 */
static void gera_matriz(double *X, size_t n)
{
    for (size_t i = 0; i < n; i++)
        X[i] = (double)(rand() % 9 - 4);
        /* rand() % 9 da 0..8; menos 4 da -4..4 */
}

/*
 * verifica_resultado -- confere C = A x B contra uma multiplicacao sequencial
 *   A [Nla x M]   matriz A completa
 *   B [M x Ncb]   matriz B completa
 *   C [Nla x Ncb] resultado a conferir (ja reunido pelo MPI_Gather/MPI_Gatherv)
 *   Todas as matrizes sao de double, armazenadas por linhas (row-major).
 *   Retorna o numero de elementos de C diferentes da referencia:
 *     0 = resultado correto.
 *   Se houver erro, imprime em stderr o primeiro elemento errado e o total.
 */
size_t verifica_resultado(int Nla, int M, int Ncb, const double *A,
                          const double *B, const double *C)
{
    size_t total_elems = (size_t)Nla * (size_t)Ncb;
    double *C_ref = (double *)calloc(total_elems, sizeof(double));
    if (C_ref == NULL) {
        fprintf(stderr, "erro: falha ao alocar matriz auxiliar em verifica_resultado\n");
        return total_elems;
    }

    /* Multiplicacao sequencial de referencia (ordem i -> k -> j para melhor localidade) */
    for (int i = 0; i < Nla; i++) {
        const double * restrict A_row = &A[(size_t)i * M];
        double * restrict C_ref_row = &C_ref[(size_t)i * Ncb];
        for (int k = 0; k < M; k++) {
            double a = A_row[k];
            const double * restrict B_row = &B[(size_t)k * Ncb];
            for (int j = 0; j < Ncb; j++) {
                C_ref_row[j] += a * B_row[j];
            }
        }
    }

    size_t diff_count = 0;
    int first_i = -1, first_j = -1;
    double first_got = 0.0, first_exp = 0.0;
    double max_abs_err = 0.0;

    for (int i = 0; i < Nla; i++) {
        for (int j = 0; j < Ncb; j++) {
            size_t idx = (size_t)i * Ncb + j;
            if (C[idx] != C_ref[idx]) {
                double err = fabs(C[idx] - C_ref[idx]);
                if (err > max_abs_err) {
                    max_abs_err = err;
                }
                if (diff_count == 0) {
                    first_i = i;
                    first_j = j;
                    first_got = C[idx];
                    first_exp = C_ref[idx];
                }
                diff_count++;
            }
        }
    }

    if (diff_count > 0) {
        fprintf(stderr,
                "[ERRO] verifica_resultado: primeiro elemento errado em C[%d][%d]: "
                "obtido = %.1f, esperado = %.1f (erro abs = %.1f) | "
                "total de elementos errados = %zu de %zu (erro max = %.1f)\n",
                first_i, first_j, first_got, first_exp,
                fabs(first_got - first_exp), diff_count, total_elems, max_abs_err);
    }

    free(C_ref);
    return diff_count;
}

/*
 * Kernel da Versao A (Direta / Simples):
 *   Multiplicacao classica em 3 lacos (i -> j -> k), sem otimizacao de cache.
 *   Cada processo multiplica sua faixa horizontal A_local [nl x M]
 *   pela matriz B inteira [M x Ncb], produzindo sua faixa C_local [nl x Ncb].
 */
static void mmul_kernel_versao_A(int nl, int M, int Ncb,
                                 const double * restrict A_local,
                                 const double * restrict B,
                                 double * restrict C_local)
{
    for (int i = 0; i < nl; i++) {
        const double * restrict A_row = &A_local[(size_t)i * M];
        double * restrict C_row = &C_local[(size_t)i * Ncb];
        for (int j = 0; j < Ncb; j++) {
            double sum = 0.0;
            for (int k = 0; k < M; k++) {
                sum += A_row[k] * B[(size_t)k * Ncb + j];
            }
            C_row[j] = sum;
        }
    }
}

/*
 * Kernel da Versao B (Blocada / Tiling com ladrilhos w x w):
 *   Lacos externos dos blocos: ii -> kk -> jj
 *   Lacos internos escalares:  i  -> k  -> j (acesso consecutivo row-major em B e C)
 *   Enquanto um ladrilho w x w de B permanece em cache, cada linha k do bloco
 *   de B eh reaproveitada pelas w linhas i do bloco de A.
 */
static void mmul_kernel_versao_B(int nl, int M, int Ncb, int w,
                                 const double * restrict A_local,
                                 const double * restrict B,
                                 double * restrict C_local)
{
    memset(C_local, 0, (size_t)nl * (size_t)Ncb * sizeof(double));

    for (int ii = 0; ii < nl; ii += w) {
        int ie = (ii + w < nl) ? (ii + w) : nl;
        for (int kk = 0; kk < M; kk += w) {
            int ke = (kk + w < M) ? (kk + w) : M;
            for (int jj = 0; jj < Ncb; jj += w) {
                int je = (jj + w < Ncb) ? (jj + w) : Ncb;
                for (int i = ii; i < ie; i++) {
                    const double * restrict A_row = &A_local[(size_t)i * M];
                    double * restrict C_row = &C_local[(size_t)i * Ncb];
                    for (int k = kk; k < ke; k++) {
                        double a = A_row[k];
                        const double * restrict B_row = &B[(size_t)k * Ncb];
                        for (int j = jj; j < je; j++) {
                            C_row[j] += a * B_row[j];
                        }
                    }
                }
            }
        }
    }
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "Uso:\n"
            "  Versao A (simples): mpirun -np <n> %s <Nla> <M> <Ncb> [-v]\n"
            "  Versao B (blocada): mpirun -np <n> %s <Nla> <M> <Ncb> -w <blockWidth> [-v]\n",
            prog, prog);
}

int main(int argc, char *argv[])
{
    int rank = 0, nprocs = 1;
    char hostname[MPI_MAX_PROCESSOR_NAME];
    int hostname_len = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Get_processor_name(hostname, &hostname_len);

    int Nla = 0, M = 0, Ncb = 0;
    int verify_flag = 0;
    int use_tiling = 0;
    int blockWidth = 0;

    /* Processamento dos argumentos da linha de comando */
    int pos_args[3] = {0, 0, 0};
    int pos_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verify_flag = 1;
        } else if (strcmp(argv[i], "-w") == 0) {
            if (i + 1 >= argc) {
                if (rank == 0) {
                    fprintf(stderr, "erro: a opcao -w requer o valor de blockWidth\n");
                    print_usage(argv[0]);
                }
                MPI_Finalize();
                return 1;
            }
            use_tiling = 1;
            blockWidth = atoi(argv[++i]);
        } else if (strncmp(argv[i], "-w", 2) == 0 && argv[i][2] != '\0') {
            use_tiling = 1;
            blockWidth = atoi(&argv[i][2]);
        } else if (argv[i][0] == '-') {
            if (rank == 0) {
                fprintf(stderr, "erro: opcao desconhecida '%s'\n", argv[i]);
                print_usage(argv[0]);
            }
            MPI_Finalize();
            return 1;
        } else {
            if (pos_count < 3) {
                pos_args[pos_count++] = atoi(argv[i]);
            } else {
                if (rank == 0) {
                    fprintf(stderr, "erro: excesso de argumentos posicionais\n");
                    print_usage(argv[0]);
                }
                MPI_Finalize();
                return 1;
            }
        }
    }

    if (pos_count < 3) {
        if (rank == 0) {
            print_usage(argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    Nla = pos_args[0];
    M   = pos_args[1];
    Ncb = pos_args[2];

    if (Nla <= 0 || M <= 0 || Ncb <= 0) {
        if (rank == 0) {
            fprintf(stderr, "erro: as dimensoes Nla, M e Ncb devem ser inteiros positivos\n");
        }
        MPI_Finalize();
        return 1;
    }

    if (use_tiling && blockWidth <= 0) {
        if (rank == 0) {
            fprintf(stderr, "erro: blockWidth (-w) deve ser um inteiro positivo\n");
        }
        MPI_Finalize();
        return 1;
    }

    /* Restricoes do exercicio: colunas multiplas de 8 (linhas alinhadas)
     * e Nla divisivel por nprocs. */
    if (M % 8 || Ncb % 8 || Nla % nprocs) {
        if (rank == 0) fprintf(stderr, "erro: M e Ncb devem ser multiplos de 8 "
                                       "e Nla divisivel por nprocs\n");
        MPI_Finalize();
        return 1;
    }

    /* Na versao B, verifica tambem se w atende as restricoes do ladrilho quando aplicavel */
    if (use_tiling && (blockWidth % 8 != 0 || M % blockWidth != 0 || Ncb % blockWidth != 0)) {
        if (rank == 0) {
            fprintf(stderr, "aviso: blockWidth (%d) idealmente deve ser multiplo de 8 e divisor de M (%d) e Ncb (%d)\n",
                    blockWidth, M, Ncb);
        }
    }

    /* a) CADA processo MPI imprime seu rank e o nome do host onde rodou */
    printf("Rank %d/%d rodando no host %s\n", rank, nprocs, hostname);
    fflush(stdout);

    int nl = Nla / nprocs;

    /* Alocacao das matrizes (vetores 1D contiguos em armazenamento row-major) */
    double *A = NULL;
    double *B = (double *)malloc((size_t)M * (size_t)Ncb * sizeof(double));
    double *C = NULL;
    double *local_A = NULL;
    double *local_C = NULL;

    if (rank == 0) {
        A = (double *)malloc((size_t)Nla * (size_t)M * sizeof(double));
        C = (double *)malloc((size_t)Nla * (size_t)Ncb * sizeof(double));
        if (A == NULL || B == NULL || C == NULL) {
            fprintf(stderr, "erro: falha na alocacao de memoria no rank 0\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }
    } else {
        if (B == NULL) {
            fprintf(stderr, "erro: falha na alocacao de B no rank %d\n", rank);
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }
    }

    if (nprocs > 1) {
        local_A = (double *)malloc((size_t)nl * (size_t)M * sizeof(double));
        local_C = (double *)malloc((size_t)nl * (size_t)Ncb * sizeof(double));
        if (local_A == NULL || local_C == NULL) {
            fprintf(stderr, "erro: falha na alocacao dos buffers locais no rank %d\n", rank);
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }
    }

    /* Inicializacao das matrizes A e B APENAS no nodo 0, FORA da medicao de tempo */
    if (rank == 0) {
        srand(1);                        /* semente fixa: execucoes reproduziveis */
        gera_matriz(A, (size_t)Nla * M); /* A [Nla x M] */
        gera_matriz(B, (size_t)M * Ncb); /* B [M x Ncb] */
    }

    /* Sincroniza todos os processos antes de iniciar a cronometragem */
    MPI_Barrier(MPI_COMM_WORLD);

    chronometer_t total_chrono, comp_chrono;
    chrono_reset(&total_chrono);
    chrono_reset(&comp_chrono);

    chrono_start(&total_chrono);

    if (nprocs == 1) {
        /*
         * Caso -np 1: executa apenas no nodo 0 a versao SEQUENCIAL
         * (sem overhead de comunicacao MPI).
         */
        chrono_start(&comp_chrono);
        if (use_tiling) {
            mmul_kernel_versao_B(Nla, M, Ncb, blockWidth, A, B, C);
        } else {
            mmul_kernel_versao_A(Nla, M, Ncb, A, B, C);
        }
        chrono_stop(&comp_chrono);
    } else {
        /*
         * Caso paralelo (-np > 1):
         *   1. MPI_Scatter distribui faixas horizontais de A (nl linhas por processo)
         *   2. MPI_Bcast replica a matriz B inteira para todos os processos
         *   3. Kernel local calcula a faixa local_C = local_A * B
         *   4. MPI_Gather recolhe as faixas local_C na matriz C do processo 0
         */
        MPI_Scatter(A, nl * M, MPI_DOUBLE,
                    local_A, nl * M, MPI_DOUBLE,
                    0, MPI_COMM_WORLD);

        MPI_Bcast(B, M * Ncb, MPI_DOUBLE, 0, MPI_COMM_WORLD);

        chrono_start(&comp_chrono);
        if (use_tiling) {
            mmul_kernel_versao_B(nl, M, Ncb, blockWidth, local_A, B, local_C);
        } else {
            mmul_kernel_versao_A(nl, M, Ncb, local_A, B, local_C);
        }
        chrono_stop(&comp_chrono);

        MPI_Gather(local_C, nl * Ncb, MPI_DOUBLE,
                   C, nl * Ncb, MPI_DOUBLE,
                   0, MPI_COMM_WORLD);
    }

    chrono_stop(&total_chrono);

    /*
     * Reporte de tempos e vazao (GFLOPS) no processo 0:
     *   - Tempo Total (medido com total_chrono no rank 0)
     *   - Tempo de Computacao (medido com comp_chrono)
     *   - Tempo de Comunicacao = Tempo Total - Tempo de Computacao
     *   - Vazao em GFLOPS = (2 * Nla * M * Ncb) / (Tempo Total * 10^9)
     */
    if (rank == 0) {
        double total_time_s = (double)chrono_gettotal(&total_chrono) / 1.0e9;
        double comp_time_s  = (double)chrono_gettotal(&comp_chrono) / 1.0e9;
        double comm_time_s  = (nprocs == 1) ? 0.0 : (total_time_s - comp_time_s);
        if (comm_time_s < 0.0) {
            comm_time_s = 0.0;
        }

        /* Cada elemento de C [Nla x Ncb] requer M multiplicacoes e M adicoes = 2*M FLOPs */
        double total_flops = 2.0 * (double)Nla * (double)M * (double)Ncb;
        double gflops = (total_time_s > 0.0) ? (total_flops / (total_time_s * 1.0e9)) : 0.0;

        printf("\n=== Resultados Multiplicacao de Matrizes MPI (Versao %s) ===\n",
               use_tiling ? "B - Blocada" : "A - Simples");
        if (use_tiling) {
            printf("Parametros: A[%d x %d] * B[%d x %d] = C[%d x %d] | nprocs = %d | blockWidth (w) = %d\n",
                   Nla, M, M, Ncb, Nla, Ncb, nprocs, blockWidth);
        } else {
            printf("Parametros: A[%d x %d] * B[%d x %d] = C[%d x %d] | nprocs = %d\n",
                   Nla, M, M, Ncb, Nla, Ncb, nprocs);
        }

        chrono_reportTime(&total_chrono, "mmul_total");
        chrono_reportTime(&comp_chrono, "mmul_computacao");

        printf("total_time_in_seconds: %lf s\n", total_time_s);
        printf("computation_time_in_seconds: %lf s\n", comp_time_s);
        printf("communication_time_in_seconds: %lf s\n", comm_time_s);
        printf("Throughput: %lf GFLOPS\n", gflops);

        /* Modo verificador (-v): executa APENAS no rank 0, FORA da medicao de tempo */
        if (verify_flag) {
            size_t erros = verifica_resultado(Nla, M, Ncb, A, B, C);
            if (erros == 0) {
                printf(">> Verificacao (-v): RESULTADO CORRETO (0 divergencias em %zu elementos; erro = 0.0)\n",
                       (size_t)Nla * (size_t)Ncb);
            } else {
                printf(">> Verificacao (-v): RESULTADO INCORRETO (%zu elementos divergentes de %zu)\n",
                       erros, (size_t)Nla * (size_t)Ncb);
            }
        }
    }

    /* Desalocacao de memoria antes de MPI_Finalize */
    if (local_A) free(local_A);
    if (local_C) free(local_C);
    if (A) free(A);
    if (B) free(B);
    if (C) free(C);

    MPI_Finalize();
    return 0;
}
