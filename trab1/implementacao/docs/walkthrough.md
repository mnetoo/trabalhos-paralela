# Walkthrough: Soma de Prefixos Paralela com Pool de Threads (v1.5)

Concluímos a implementação modularizada em C do algoritmo de **Soma de Prefixos Paralela** (*Parallel Prefix Sum*) em conformidade com as especificações do **Trabalho 1 (v1.5)** e com o **Plano de Trabalho**.

---

## 1. Módulos Criados

| Arquivo | Descrição |
| :--- | :--- |
| [`chrono.h`](file:///c:/Users/marcu/Documents/Github/trabalhos-paralela/trab1/chrono.h) / [`chrono.c`](file:///c:/Users/marcu/Documents/Github/trabalhos-paralela/trab1/chrono.c) | Biblioteca de medição de tempo de alta precisão baseada na implementação de referência do professor Wagner, corrigida para linkage limpo em C moderno com GCC (`clock_gettime(CLOCK_MONOTONIC_RAW)`). |
| [`prefixSum.h`](file:///c:/Users/marcu/Documents/Github/trabalhos-paralela/trab1/prefixSum.h) | Cabeçalho principal com as definições globais (`TYPE long long`, `NTIMES 100`, `MAX_THREADS 64`) e os protótipos de `ParallelPrefixSumPth` e `ParallelPrefixSumCleanup`. |
| [`prefixSum.c`](file:///c:/Users/marcu/Documents/Github/trabalhos-paralela/trab1/prefixSum.c) | Núcleo do algoritmo com Pool de Threads persistente, afinidade rígida aos núcleos da CPU (`sched.h`, `pthread_attr_setaffinity_np`), suporte a `chunkSize`, vetor auxiliar `PartialSum` e **2 barreiras sincronizadas com sinalização atômica (`stdatomic.h`)**. |
| [`verify.h`](file:///c:/Users/marcu/Documents/Github/trabalhos-paralela/trab1/verify.h) / [`verify.c`](file:///c:/Users/marcu/Documents/Github/trabalhos-paralela/trab1/verify.c) | Módulo de verificação sequencial que compara o resultado in-place com o acumulado elemento a elemento do vetor original quando o argumento `-v` é utilizado. |
| [`prefixSumPth.c`](file:///c:/Users/marcu/Documents/Github/trabalhos-paralela/trab1/prefixSumPth.c) | Programa principal (`main`) com validação de argumentos da linha de comando, alocação dinâmica com `malloc`, benchmark de 100 iterações cronometradas, cálculo de vazão em OP/s e MOPS, e formatação compatível com a planilha do Lab 4. |
| [`Makefile`](file:///c:/Users/marcu/Documents/Github/trabalhos-paralela/trab1/Makefile) | Makefile completo para automação de build (`gcc -O3 -pthread -Wall -Wextra`), alvos `all`, `native`, `debug`, `test`, `run` e `clean`. |
| [`roda.sh`](file:///c:/Users/marcu/Documents/Github/trabalhos-paralela/trab1/roda.sh) | Script bash para automação de testes de 1 a 8 threads com 10 repetições cada, permitindo copiar diretamente para a planilha de resultados do laboratório. |

---

## 2. Detalhes Técnicos da Otimização com 2 Barreiras

Substituímos a 3ª barreira clássica por uma estratégia lock-free de sinalização atômica:
1. **Barreira 1 (`pool_barrier`):** Aguarda todas as $nThreads$ threads (thread 0 + $nThreads-1$ workers) para disparar a iteração.
2. **Fase 1 (Pré-Barreira):** Cada thread processa exclusivamente sua fração contígua $[first, last]$ (respeitando blocos de tamanho `chunkSize`) e salva sua soma em `PartialSum[myIndex]`.
3. **Barreira 2 (`algo_barrier`):** Sincroniza todas as threads após a escrita de `PartialSum`.
4. **Fase 2 (Pós-Barreira):** Cada thread calcula seu offset acumulando `PartialSum[0 ... myIndex-1]` e atualiza o vetor `Vec` diretamente *in-place*.
5. **Sinalização Atômica:**
   - Cada worker ($i \ge 1$) ao finalizar executa:
     ```c
     atomic_fetch_add_explicit(&threads_completed, 1, memory_order_release);
     ```
   - A thread 0, após terminar sua Fase 2, realiza espera ativa suave:
     ```c
     while (atomic_load_explicit(&threads_completed, memory_order_acquire) < nThreads - 1) {
         #if defined(__x86_64__) || defined(__i386__)
         __builtin_ia32_pause();
         #endif
     }
     ```
   - A thread 0 reseta `threads_completed = 0` antes do início da próxima iteração.

---

## 3. Testes e Resultados de Validação

### 3.1 Testes de Corretude Automatizados (`make test`)
Executados via `make test` cobrindo diferentes números de threads e tamanhos de vetores:
- **1 thread (chunkSize = 0, 100.000 elementos):** SUCESSO (100% corretos).
- **2 threads (chunkSize = 0, 100.000 elementos):** SUCESSO (100% corretos).
- **4 threads (chunkSize = 1024, 100.000 elementos):** SUCESSO (100% corretos).
- **8 threads (chunkSize = 512, 100.003 elementos - tamanho ímpar/primo):** SUCESSO (100% corretos).

### 3.2 Teste com Carga Completa (8 Milhões de Elementos)
```bash
./prefixSumPth 8000000 8 0 -v
```
**Resultado:**
- `total_time_in_seconds: 0.868990 s`
- `Throughput: 920.609241 MOPS`
- `>> Verificacao: SUCESSO (todos os 8000000 elementos estao corretos)!`

### 3.3 Teste do Script de Automação (`roda.sh`)
```bash
./roda.sh 100000 2 0
```
**Saída Gerada:**
```
Executando 10 vezes com 100000 elementos e 1 threads:
0.007126
0.008584
...
Executando 10 vezes com 100000 elementos e 2 threads:
0.011226
0.013968
...
```
Formato 100% compatível com a planilha do LibreOffice Calc utilizada no Lab 4.
