/*
 * CI1316 - Programacao Paralela - Trabalho 1 (v1.5)
 * Soma de Prefixos Paralela com Pool de Threads, Afinidade de Threads e 2 Barreiras
 *
 * Projeto modularizado composto por:
 * - prefixSum.h / prefixSum.c : Implementacao do algoritmo, Thread Pool, afinidade e 2 barreiras
 * - chrono.h / chrono.c       : Medicao de tempo de alta precisao
 * - verify.h / verify.c       : Verificacao sequencial de corretude (-v)
 * - prefixSumPth.c            : Programa principal, benchmark NTIMES=100 e saida formatada
 * - Makefile                  : Automacao da compilacao com gcc -O3 -pthread
 * - roda.sh                   : Script para automacao dos testes de 1 a 8 threads
 */

#include "prefixSum.h"
#include "chrono.c"
#include "verify.h"