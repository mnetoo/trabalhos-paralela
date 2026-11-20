/*
 * CI1316 - Programacao Paralela - Trabalho 2: Multiplicacao de Matrizes com MPI
 *
 * Autores:
 *   Henrique Villela Silva (GRR20242612)
 *   Marcus Sebastiao Adriano Rocha Neto (GRR20240710)
 */

#define _GNU_SOURCE

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "mmul.h"
#include "chrono.c"

/* Aloca matriz alinhada em 64 bytes (tamanho da linha de cache) */
static double *alloc_matrix_aligned(size_t n_elems)
{
    void *ptr = NULL;
    size_t bytes = n_elems * sizeof(double);
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
    if (posix_memalign(&ptr, 64, bytes) != 0) {
        return NULL;
    }
    return (double *)ptr;
#else
    ptr = malloc(bytes);
    return (double *)ptr;
#endif
}

/*
 * Preenche a matriz com inteiros pequenos em [-4, 4] guardados como double.
 * Assim as somas e produtos sao exatos em ponto flutuante e podemos comparar
 * o resultado com != na verificacao sem erro de arredondamento.
 */
void gera_matriz(double *X, size_t n)
{
    for (size_t i = 0; i < n; i++)
        X[i] = (double)(rand() % 9 - 4);
}

/*
 * Confere C = A x B no processo 0 comparando com a multiplicacao sequencial.
 * Retorna a quantidade de elementos diferentes (0 = tudo certo).
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
 * Versao A (Direta / Simples):
 *   Multiplicacao nos 3 lacos tradicionais (i -> j -> k), sem blocagem de cache.
 *   Como M eh multiplo de 8, o laco interno em k foi desdobrado de 8 em 8
 *   usando 4 acumuladores.
 */
void mmul_kernel_versao_A(int nl, int M, int Ncb,
                          const double * restrict A_local,
                          const double * restrict B,
                          double * restrict C_local)
{
    const size_t stride  = (size_t)Ncb;
    const size_t stride2 = stride * 2;
    const size_t stride3 = stride * 3;
    const size_t stride4 = stride * 4;

    for (int i = 0; i < nl; i++) {
        const double * restrict A_row = &A_local[(size_t)i * (size_t)M];
        double * restrict C_row = &C_local[(size_t)i * stride];

        for (int j = 0; j < Ncb; j++) {
            double sum0 = 0.0;
            double sum1 = 0.0;
            double sum2 = 0.0;
            double sum3 = 0.0;
            const double * restrict b_ptr = &B[j];

            for (int k = 0; k < M; k += 8) {
                sum0 += A_row[k + 0] * b_ptr[0];
                sum1 += A_row[k + 1] * b_ptr[stride];
                sum2 += A_row[k + 2] * b_ptr[stride2];
                sum3 += A_row[k + 3] * b_ptr[stride3];
                b_ptr += stride4;

                sum0 += A_row[k + 4] * b_ptr[0];
                sum1 += A_row[k + 5] * b_ptr[stride];
                sum2 += A_row[k + 6] * b_ptr[stride2];
                sum3 += A_row[k + 7] * b_ptr[stride3];
                b_ptr += stride4;
            }

            C_row[j] = (sum0 + sum1) + (sum2 + sum3);
        }
    }
}

/*
 * Versao B (Blocada com ladrilhos w x w):
 *   Lacos externos dos blocos: ii -> kk -> jj
 *   Lacos internos: i -> k -> j (percorre B e C por linha com passo 1)
 */
void mmul_kernel_versao_B(int nl, int M, int Ncb, int w,
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

    /* Restricoes do exercicio: M e Ncb multiplos de 8 e Nla divisivel por nprocs */
    if (M % 8 || Ncb % 8 || Nla % nprocs) {
        if (rank == 0) fprintf(stderr, "erro: M e Ncb devem ser multiplos de 8 "
                                       "e Nla divisivel por nprocs\n");
        MPI_Finalize();
        return 1;
    }

    if (use_tiling && (blockWidth % 8 != 0 || M % blockWidth != 0 || Ncb % blockWidth != 0)) {
        if (rank == 0) {
            fprintf(stderr, "aviso: blockWidth (%d) idealmente deve ser multiplo de 8 e divisor de M (%d) e Ncb (%d)\n",
                    blockWidth, M, Ncb);
        }
    }

    printf("Rank %d/%d rodando no host %s\n", rank, nprocs, hostname);
    fflush(stdout);

    int nl = Nla / nprocs;

    /* Apenas o rank 0 aloca A e C inteiras; os outros alocam so a faixa local e B */
    double *A = NULL;
    double *B = alloc_matrix_aligned((size_t)M * (size_t)Ncb);
    double *C = NULL;
    double *local_A = NULL;
    double *local_C = NULL;

    if (rank == 0) {
        A = alloc_matrix_aligned((size_t)Nla * (size_t)M);
        C = alloc_matrix_aligned((size_t)Nla * (size_t)Ncb);
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
        local_A = alloc_matrix_aligned((size_t)nl * (size_t)M);
        local_C = alloc_matrix_aligned((size_t)nl * (size_t)Ncb);
        if (local_A == NULL || local_C == NULL) {
            fprintf(stderr, "erro: falha na alocacao dos buffers locais no rank %d\n", rank);
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }
    }

    /* Gera A e B somente no rank 0 e fora da medicao de tempo */
    if (rank == 0) {
        srand(MMUL_RAND_SEED);
        gera_matriz(A, (size_t)Nla * M);
        gera_matriz(B, (size_t)M * Ncb);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    chronometer_t total_chrono, comp_chrono;
    chrono_reset(&total_chrono);
    chrono_reset(&comp_chrono);

    chrono_start(&total_chrono);

    if (nprocs == 1) {
        chrono_start(&comp_chrono);
        if (use_tiling) {
            mmul_kernel_versao_B(Nla, M, Ncb, blockWidth, A, B, C);
        } else {
            mmul_kernel_versao_A(Nla, M, Ncb, A, B, C);
        }
        chrono_stop(&comp_chrono);
    } else {
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

    if (rank == 0) {
        double total_time_s = (double)chrono_gettotal(&total_chrono) / 1.0e9;
        double comp_time_s  = (double)chrono_gettotal(&comp_chrono) / 1.0e9;
        double comm_time_s  = (nprocs == 1) ? 0.0 : (total_time_s - comp_time_s);
        if (comm_time_s < 0.0) {
            comm_time_s = 0.0;
        }

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

    if (local_A) free(local_A);
    if (local_C) free(local_C);
    if (A) free(A);
    if (B) free(B);
    if (C) free(C);

    MPI_Finalize();
    return 0;
}
