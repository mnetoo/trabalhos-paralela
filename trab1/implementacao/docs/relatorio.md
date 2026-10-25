# Relatório do Trabalho 1 — Prefix Sum Paralelo com Pthreads

**Disciplina:** CI1316 — Programação Paralela (UFPR)  
**Professor:** Prof. Dr. Wagner M. Nunan Zola  
**Semestre:** 2026/1  

**Identificação da Equipe:**
- **Aluno 1:** Henrique Villela Silva — **GRR:** GRR20242612
- **Aluno 2:** Marcus Sebastião Adriano Rocha Neto — **GRR:** GRR20240710

---

## 1. Implementação e Algoritmo Paralelo

O trabalho implementa o cálculo de **Prefix Sum Exclusivo** (*in-place*) sobre um vetor `Vec` de $N = 8.000.000$ elementos do tipo `long long` ($64\text{ MB}$ de memória contígua), utilizando a biblioteca POSIX Threads (`pthreads`) em linguagem C (`-O3`).

### 1.1. Pool de Threads e Afinidade de Cores (Core Pinning)
Para evitar o custo de criação (`pthread_create`) e destruição (`pthread_join`) de threads a cada chamada de `ParallelPrefixSumPth` dentro do laço de repetição (`NTIMES = 100`), foi implementado um **Pool de Threads** na função `initThreadPool`:
- São criadas `nThreads - 1` threads trabalhadoras (`workerKernel`) apenas uma vez antes do início das medições. A própria thread `main` atua como a thread de índice `id = 0`.
- Cada thread (incluindo a `main`) é fixada em um núcleo lógico específico do processador por meio de `pthread_setaffinity_np` (`CPU_SET(id, &cpuset)`), evitando migrações de contexto entre núcleos pelo escalonador do sistema operacional e preservando a localidade das caches L1 e L2.

### 1.2. Algoritmo em 2 Fases
Para um bloco de elementos (ou *chunk*), o algoritmo divide o trabalho contiguamente entre as `nThreads` threads e executa em **duas fases**:

1. **Fase 1 (Leitura e Soma Local):** Cada thread `id` percorre sua fatia `[start, end)` acumulando a soma total dos seus elementos e grava esse total em `PartialSum[id]`. Nenhuma escrita é feita em `Vec` nesta fase.
2. **Barreira Algorítmica (`algo_barrier`):** Garante que todas as threads tenham escrito seus valores em `PartialSum` antes que qualquer thread calcule seu prefixo acumulado.
3. **Fase 2 (Acumulação de Prefixos e Escrita In-Place):** Cada thread `id` soma `PartialSum[0 ... id-1]` (mais o acumulador `chunkBase` vindo de chunks anteriores) para obter o valor inicial exclusivo de sua fatia, e então percorre novamente `[start, end)` substituindo cada posição `Vec[i]` pela soma acumulada até `i - 1`.

### 1.3. Otimização de Sincronização: Duas Barreiras e Sinalização Atômica
Em uma implementação convencional com pool de threads, seriam necessárias três barreiras por chamada:
1. Uma barreira na entrada da função para liberar as threads trabalhadoras do pool;
2. Uma barreira no meio (entre a Fase 1 e a Fase 2);
3. Uma terceira barreira no final da Fase 2 para que a thread `main` espere todas as threads terminarem antes de retornar.

No entanto, o uso excessivo de barreiras POSIX (`pthread_barrier_wait`) impõe sobrecarga de sincronização. Para otimizar o fluxo e manter o algoritmo com rigorosamente **apenas duas barreiras por chamada**, substituímos a barreira final de encerramento por uma sinalização leve com **contador atômico em C11** (`atomic_int threads_completed`):
- **1ª Barreira (`pool_barrier`):** Sincroniza a thread `main` com as trabalhadoras para iniciarem a execução do algoritmo juntas.
- **2ª Barreira (`algo_barrier`):** Sincroniza a passagem entre a Fase 1 e a Fase 2.
- **Término sem 3ª Barreira:** Ao terminar a Fase 2, cada trabalhadora executa `atomic_fetch_add(&threads_completed, 1)` e já retorna diretamente para o início do seu laço, ficando posicionada na `pool_barrier` para a próxima iteração.
- A thread `main` (que atua como a thread 0) faz apenas uma espera atômica em memória (`while (atomic_load(&threads_completed) < nThreads - 1)`), que é satisfeita assim que a última trabalhadora conclui sua escrita.

Com esse desenho, o algoritmo executa exatamente duas barreiras por chamada a `ParallelPrefixSumPth`, reduzindo a latência de sincronização no retorno da função.

### 1.4. Processamento em Chunks para Reúso de Cache L2 (`chunkSize > 0`)
Como o algoritmo lê o vetor na Fase 1 e o lê/escreve novamente na Fase 2, processar o vetor inteiro de $8.000.000$ de elementos ($64\text{ MB}$) de uma só vez (`chunkSize = 0`) faz com que os dados lidos no início da Fase 1 sejam expulsos (*evicted*) da cache L2 antes do início da Fase 2, gerando *cache misses* e tráfego excessivo na memória RAM principal.

Quando `chunkSize > 0`, o vetor total é fatiado em pedaços menores de tamanho `chunkSize` que cabem folgadamente na cache L2:
- Em cada rodada (*round*), todas as `nThreads` dividem apenas o chunk atual `[chunkOffset, chunkOffset + currentChunkSize)`.
- Como o chunk inteiro cabe na cache L2, na Fase 2 os dados recém-lidos na Fase 1 ainda estão residentes na cache (**reúso temporal e espacial de cache**), acelerando drasticamente a segunda passada.
- Ao final de cada chunk, a variável `chunkBase` acumula a soma total daquele chunk (`PartialSum[0 ... nThreads-1]`) para servir de base ao chunk seguinte, e uma barreira de rodada (`round_barrier`) sincroniza a passagem para o próximo chunk. No último chunk do vetor, a `round_barrier` é omitida e o término é sinalizado diretamente pelo contador atômico `threads_completed`.

---

## 2. Arquitetura do Processador e Topologia de Hardware

Os experimentos foram executados no nó **`wz10`** do cluster computacional da disciplina, com as características reais de hardware obtidas diretamente pelo comando `lscpu` e pelo utilitário `lstopo`:

### 2.1. Processador e Topologia dos Cores (`wz10`)
- **Modelo do Processador:** **Intel(R) Xeon(R) CPU E5410 @ 2.33GHz** (microarquitetura Intel Core / Harpertown, 45nm).
- **Configuração de Sockets e Núcleos Físicos:**
  - **2 Sockets físicos** (`Socket 0` e `Socket 1`).
  - **4 núcleos físicos por socket** (`Core(s) per socket: 4`), totalizando **8 núcleos físicos reais** (`0` a `7`).
  - **1 thread por núcleo** (`Thread(s) per core: 1` — sem Hyper-Threading / SMT). Todas as 8 CPUs do sistema são núcleos físicos reais independentes.
  - Frequência de operação: **2.33 GHz** (2333 MHz).

### 2.2. Hierarquia de Memória Cache
A microarquitetura do Xeon E5410 é baseada em módulos multi-chip onde cada par de núcleos físicos compartilha um bloco unificado de cache L2:
- **Cache L1:** 32 KiB de dados (L1d) + 32 KiB de instruções (L1i) privativos por núcleo (totalizando 256 KiB L1d e 256 KiB L1i no sistema).
- **Cache L2 por par de núcleos:** Cada bloco de dois núcleos físicos compartilha uma cache L2 de **6.291.456 bytes (6 MB / 6.144 KB)**.
- **Cache L2 total do nó:** 24 MiB (4 blocos de 6 MB distribuídos entre os 2 sockets).
- **Cache L3:** Ausente (0 bytes — a microarquitetura Penryn/Harpertown não utiliza cache L3; a L2 é o último nível de cache antes da memória principal).
- **Barramento e Memória:** Conexão dos 2 sockets via Front Side Bus (FSB) a um controlador de memória unificado (MCH).

### 2.3. Relação Direta entre a Arquitetura e os Resultados Experimentais
Esta topologia explica com exatidão física os resultados obtidos nas Experiências A e B:
1. **Escolha de `chunkSize = 196608` (Experiência 0):** Como o par de núcleos compartilha uma cache L2 de 6 MB ($6.291.456\text{ bytes} / 8 = 786.432$ elementos `long long`), a escolha de $1/4$ da cache L2 resulta exatamente em:
   $$\text{chunkSize} = \frac{786.432}{4} = 196.608\text{ elementos} \quad (1,5\text{ MB})$$
   Garantindo que o chunk caiba confortavelmente na cache L2 sem sofrer expulsão por contenção entre os núcleos parceiros.
2. **Ponto ótimo em 4 threads e contenção dual-socket acima de 4 threads:**
   - Com **1 a 4 threads**, as threads executam concentradas no **Socket 0** (4 núcleos físicos), aproveitando o barramento local de memória e as caches L2 daquele socket. A vazão cresce rapidamente até o pico médio de **494,48 MOPS (Exp A)** e **334,62 MOPS (Exp B)** com 4 threads.
   - De **5 a 8 threads**, o escalonador e a afinidade ativam os núcleos do **Socket 1**. A comunicação e o tráfego de memória passam a disputar o barramento compartilhado do controlador de memória (FSB MCH entre os dois sockets). Como o Prefix Sum é fortemente limitado por banda de memória (*memory-bound*), o barramento de memória satura, mantendo a vazão estável no patamar de ~474–494 MOPS na Experiência A.

---

## 3. Metodologia de Medição e Experimentos

### 3.1. Desconto do Tempo de `memcpy` nas Medições (`NTIMES = 100`)
Como `ParallelPrefixSumPth` opera **in-place** (sobrescrevendo o vetor `Vec` com o resultado), para executar o algoritmo `NTIMES = 100` vezes seguidas em cada bateria de medição é necessário restaurar o conteúdo original do vetor antes de cada chamada usando `memcpy(Vec, InitVector, ...)`.
Para que o tempo gasto nessa cópia de restauração não contamine o tempo medido do algoritmo paralelo:
1. O cronômetro principal (`parallelPrefixSumTime`) é iniciado antes do laço de `NTIMES` iterações.
2. Em cada iteração `i`, um cronômetro auxiliar (`copyChrono`) mede exclusivamente a duração do `memcpy` e, em seguida, a função `chrono_decrease_time(&parallelPrefixSumTime, &copyChrono)` subtrai exatamente o tempo da cópia do cronômetro principal.
3. Assim, o tempo final reportado reflete **apenas** o tempo líquido de execução de `ParallelPrefixSumPth`, dividido por `NTIMES = 100`.

### 3.2. Experiência 0 — Escolha do Tamanho de Chunk (`chunkSize`)
O tamanho da cache L2 do nó `wz10` é de $6\text{ MB}$ ($6.291.456\text{ bytes}$). Como cada elemento `long long` ocupa $8\text{ bytes}$:
$$\text{Capacidade total da L2} = \frac{6.291.456\text{ bytes}}{8\text{ bytes/elemento}} = 786.432\text{ elementos}$$

Para garantir que os elementos processados na Fase 1 permaneçam residentes na cache L2 durante a Fase 2 sem sofrer expulsão por conflitos de associatividade, variáveis auxiliares ou compartilhamento entre núcleos, escolhemos um `chunkSize` correspondente a **$1/4$ da capacidade da cache L2**:
$$\text{chunkSize} = \frac{786.432}{4} = 196.608\text{ elementos} \quad (1.572.864\text{ bytes} = 1,5\text{ MB})$$

Dessa forma, o vetor de $8.000.000$ de elementos é processado em $\lceil 8.000.000 / 196.608 \rceil = 41\text{ chunks}$ de $1,5\text{ MB}$ cada.

---

## 4. Resultados Experimentais e Análise de Desempenho

Todos os dados brutos, fórmulas (`AVERAGE`, `MIN`, `MAX`) e gráficos de **Tempo (ms) × Vazão (MOPS)** encontram-se na planilha **`Planilha-PrefixSum-Pthreads-WZ-ExpA-ExpB.xlsx`** e nos arquivos de saída do SLURM (`resultados-expA-chunk-196608-job18977.txt` e `resultados-expB-semChunk-job18980.txt`).

### 4.1. Experiência A — Com Chunks (`chunkSize = 196608`, 6 execuções por nº de threads)
- **Script SLURM:** `roda-exp-A-chunk-196608-slurm.sh` (Job `18977`, tempo total do job: `263s` / `04:24`)

#### Tabela 1A — Tempos de Execução em Milissegundos (`ms`) — Experiência A (`chunkSize = 196608`)

| Execução \ Threads | 1 Thread | 2 Threads | 3 Threads | 4 Threads | 5 Threads | 6 Threads | 7 Threads | 8 Threads |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Execução 1** | 32,20 | 18,92 | 19,21 | 15,82 | 17,96 | 16,30 | 16,80 | 16,74 |
| **Execução 2** | 32,05 | 19,21 | 19,22 | 15,80 | 18,16 | 15,59 | 16,86 | 18,52 |
| **Execução 3** | 32,68 | 19,43 | 20,03 | 16,38 | 18,01 | 16,35 | 17,05 | 18,54 |
| **Execução 4** | 32,45 | 19,58 | 19,59 | 16,00 | 17,67 | 16,49 | 17,22 | **15,45** |
| **Execução 5** | 32,14 | 19,20 | 19,55 | 16,58 | 18,22 | 16,97 | 16,44 | 16,31 |
| **Execução 6** | 32,08 | 19,05 | 19,49 | 16,52 | 17,91 | 16,50 | 16,37 | 16,41 |
| **Média (ms)** | **32,27** | **19,23** | **19,51** | **16,18** | **17,99** | **16,37** | **16,79** | **17,00** |
| **Mínimo (ms)** | 32,05 | 18,92 | 19,21 | 15,80 | 17,67 | 15,59 | 16,37 | **15,45** |
| **Máximo (ms)** | 32,68 | 19,58 | 20,03 | 16,58 | 18,22 | 16,97 | 17,22 | 18,54 |

#### Tabela 2A — Vazão em `MOPS` (Milhões de Operações por Segundo) e Speedup — Experiência A

| Execução \ Threads | 1 Thread | 2 Threads | 3 Threads | 4 Threads | 5 Threads | 6 Threads | 7 Threads | 8 Threads |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Execução 1** | 248,47 | 422,90 | 416,52 | 505,74 | 445,42 | 490,71 | 476,20 | 477,94 |
| **Execução 2** | 249,59 | 416,48 | 416,18 | 506,33 | 440,56 | 513,18 | 474,47 | 431,88 |
| **Execução 3** | 244,77 | 411,64 | 399,45 | 488,43 | 444,18 | 489,33 | 469,30 | 431,58 |
| **Execução 4** | 246,56 | 408,52 | 408,33 | 500,06 | 452,85 | 485,09 | 464,46 | **517,92** |
| **Execução 5** | 248,94 | 416,75 | 409,21 | 482,42 | 439,07 | 471,44 | 486,47 | 490,37 |
| **Execução 6** | 249,34 | 419,75 | 410,59 | 483,91 | 446,31 | 484,50 | 488,49 | 486,65 |
| **Média (MOPS)** | **247,94** | **416,01** | **410,05** | **494,48** | **444,73** | **489,04** | **476,56** | **474,39** |
| **Mínimo (MOPS)** | 244,77 | 408,52 | 399,45 | 482,42 | 439,07 | 471,44 | 464,46 | 431,58 |
| **Máximo (MOPS)** | 249,59 | 422,90 | 416,52 | 506,33 | 452,85 | 513,18 | 488,49 | **517,92** |
| **Speedup Médio** | **1,00x** | **1,68x** | **1,65x** | **1,99x** | **1,79x** | **1,97x** | **1,92x** | **1,91x** |

---

### 4.2. Experiência B — Sem Chunks (`chunkSize = 0`, 5 execuções por nº de threads)
- **Script SLURM:** `roda-exp-B-chunk-0-slurm.sh` (Job `18980`, tempo total do job: `254s` / `04:14`)

#### Tabela 1B — Tempos de Execução em Milissegundos (`ms`) — Experiência B (`chunkSize = 0`)

| Execução \ Threads | 1 Thread | 2 Threads | 3 Threads | 4 Threads | 5 Threads | 6 Threads | 7 Threads | 8 Threads |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Execução 1** | 44,98 | 26,64 | 27,92 | 23,86 | 26,95 | 25,08 | 25,53 | 24,57 |
| **Execução 2** | 45,07 | 26,38 | 28,28 | 24,33 | 26,62 | 24,23 | 25,65 | **23,60** |
| **Execução 3** | 44,86 | 26,39 | 28,23 | 23,80 | 26,61 | 23,90 | 25,62 | 24,49 |
| **Execução 4** | 44,76 | 26,37 | 27,82 | 23,82 | 26,69 | 24,78 | 25,84 | 24,09 |
| **Execução 5** | 44,80 | 26,60 | 27,94 | 23,73 | 26,69 | 24,05 | 25,35 | 24,62 |
| **Média (ms)** | **44,89** | **26,48** | **28,04** | **23,91** | **26,71** | **24,41** | **25,60** | **24,27** |
| **Mínimo (ms)** | 44,76 | 26,37 | 27,82 | 23,73 | 26,61 | 23,90 | 25,35 | **23,60** |
| **Máximo (ms)** | 45,07 | 26,64 | 28,28 | 24,33 | 26,95 | 25,08 | 25,84 | 24,62 |

#### Tabela 2B — Vazão em `MOPS` (Milhões de Operações por Segundo) e Speedup — Experiência B

| Execução \ Threads | 1 Thread | 2 Threads | 3 Threads | 4 Threads | 5 Threads | 6 Threads | 7 Threads | 8 Threads |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Execução 1** | 177,85 | 300,34 | 286,52 | 335,23 | 296,87 | 319,04 | 313,34 | 325,63 |
| **Execução 2** | 177,49 | 303,26 | 282,91 | 328,87 | 300,56 | 330,22 | 311,88 | **338,94** |
| **Execução 3** | 178,34 | 303,18 | 283,40 | 336,08 | 300,66 | 334,76 | 312,26 | 326,62 |
| **Execução 4** | 178,73 | 303,38 | 287,60 | 335,91 | 299,70 | 322,90 | 309,59 | 332,05 |
| **Execução 5** | 178,59 | 300,70 | 286,29 | 337,03 | 299,72 | 332,71 | 315,60 | 324,96 |
| **Média (MOPS)** | **178,22** | **302,17** | **285,33** | **334,62** | **299,50** | **327,93** | **312,53** | **329,64** |
| **Mínimo (MOPS)** | 177,49 | 300,34 | 282,91 | 328,87 | 296,87 | 319,04 | 309,59 | 324,96 |
| **Máximo (MOPS)** | 178,73 | 303,38 | 287,60 | 337,03 | 300,66 | 334,76 | 315,60 | **338,94** |
| **Speedup Médio** | **1,00x** | **1,70x** | **1,60x** | **1,88x** | **1,68x** | **1,84x** | **1,75x** | **1,85x** |

---

### 4.3. Comparação entre Experiência A (Com Chunks) e Experiência B (Sem Chunks)

A tabela abaixo resume a comparação direta entre as médias da **Experiência A (`chunkSize = 196608`)** e da **Experiência B (`chunkSize = 0`)** para cada número de threads (`1` a `8`), apresentando tanto o fator de aceleração $\text{Vazão}(A)/\text{Vazão}(B)$ quanto a razão $\text{Vazão}(B)/\text{Vazão}(A)$ solicitada na especificação:

#### Tabela 3 — Comparativo Direto: Experiência A vs. Experiência B

| Nº Threads | Tempo Médio Exp A (ms) | Tempo Médio Exp B (ms) | Vazão Média Exp A (MOPS) | Vazão Média Exp B (MOPS) | Aceleração Vazão(A) / Vazão(B) | Razão Vazão(B) / Vazão(A) | Ganho Percentual de A sobre B (%) |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **1** | 32,27 | 44,89 | 247,94 | 178,22 | **1,39x** | 0,72 | +39,12% |
| **2** | 19,23 | 26,48 | 416,01 | 302,17 | **1,38x** | 0,73 | +37,67% |
| **3** | 19,51 | 28,04 | 410,05 | 285,33 | **1,44x** | 0,70 | +43,71% |
| **4** | **16,18** | **23,91** | **494,48** | **334,62** | **1,48x** | 0,68 | +47,77% |
| **5** | 17,99 | 26,71 | 444,73 | 299,50 | **1,48x** | 0,67 | +48,49% |
| **6** | 16,37 | 24,41 | 489,04 | 327,93 | **1,49x** | 0,67 | +49,13% |
| **7** | 16,79 | 25,60 | 476,56 | 312,53 | **1,52x** | 0,66 | **+52,48%** |
| **8** | 17,00 | 24,27 | 474,39 | 329,64 | **1,44x** | 0,69 | +43,91% |

---

### 4.4. Discussão e Conclusões dos Resultados

1. **Melhor Desempenho Absoluto:**
   - **Na Experiência A (com chunks L2):** A maior vazão média foi obtida com **4 threads**, atingindo **494,48 MOPS** (tempo médio de **16,18 ms** e speedup de **1,99x** sobre 1 thread com chunks). Considerando execuções individuais, o pico absoluto foi alcançado com **8 threads** na 4ª execução (**517,92 MOPS** / **15,45 ms**), seguido de **6 threads** na 2ª execução (**513,18 MOPS** / **15,59 ms**).
   - **Na Experiência B (sem chunks):** A maior vazão média também ocorreu com **4 threads**, atingindo **334,62 MOPS** (tempo médio de **23,91 ms** e speedup de **1,88x**), com pico individual de **338,94 MOPS** (**23,60 ms**) em 8 threads.

2. **Impacto do Reúso de Cache L2 (Experiência A vs. Experiência B):**
   - A divisão do vetor em chunks de `196.608` elementos ($1,5\text{ MB} = 1/4$ da cache L2) proporcionou ganhos expressivos e consistentes em **todas** as configurações de threads, variando de **1,38x (+37,67%)** com 2 threads até **1,52x (+52,48%)** com 7 threads.
   - Mesmo com **1 única thread** (execução puramente sequencial dentro do kernel paralelo), a versão em chunks reduziu o tempo médio de `44,89 ms` para `32,27 ms` (**+39,12% de vazão**). Isso comprova experimentalmente que o gargalo principal do Prefix Sum é a largura de banda de memória (*memory-bound*): quando `chunkSize = 0`, os $64\text{ MB}$ do vetor excedem em mais de 10 vezes os $6\text{ MB}$ da cache L2, forçando o processador a buscar todo o vetor na memória RAM duas vezes (uma na Fase 1 e outra na Fase 2). Com `chunkSize = 196608`, a segunda passada (Fase 2) encontra o chunk inteiramente aquecido na cache L2, economizando metade das transferências com a DRAM.

3. **Escalabilidade com o Número de Threads (1 a 8 Threads):**
   - Em ambas as experiências, observa-se um ganho acentuado de 1 para 2 threads (`1,68x` na Exp A; `1,70x` na Exp B) e o ponto ótimo médio em **4 threads** (`1,99x` na Exp A; `1,88x` na Exp B).
   - Acima de 4 threads (5 a 8 threads), o ganho estabiliza em um patamar próximo de `474–494 MOPS` na Experiência A (`300–334 MOPS` na Experiência B), com pequenas oscilações para números ímpares de threads (3, 5 e 7 threads) devido ao compartilhamento de recursos de hardware (como controladores de memória, barramentos de cache L2 compartilhados entre pares de núcleos e SMT/Hyper-Threading) e à saturação da banda de memória principal do nó `wz10`.

---

## 5. Instruções de Compilação, Execução e Arquivos Entregues

### 5.1. Como Compilar o Projeto
O projeto inclui um `Makefile` configurado com `gcc -Wall -Wextra -O3 -pthread -std=gnu11`:
```bash
make clean && make all
```
Isso gera o executável principal `prefixSumPth` e os aliases requeridos pelos scripts de teste (`prefixSumPth-v3-longlong` e `prefixSumPth-longlong`).

### 5.2. Como Executar Manualmente
A sintaxe de execução manual é:
```bash
./prefixSumPth <nElements> <chunkSize> <nThreads> [-v]
```
- `<nElements>`: Número de elementos do vetor (ex: `8000000`).
- `<chunkSize>`: Tamanho do bloco para a cache L2 (use `196608` para rodar com chunks ou `0` para sem chunks).
- `<nThreads>`: Quantidade de threads a serem utilizadas (de `1` a `8`).
- `[-v]`: Parâmetro opcional de verificação de corretude. Quando ativado, calcula a versão sequencial do Prefix Sum e compara elemento por elemento contra o resultado paralelo, exibindo a mensagem de sucesso ou erro.

### 5.3. Como Submeter os Experimentos via SLURM
No nó de login do cluster da UFPR, as baterias de testes oficiais são submetidas com:
```bash
sbatch roda-exp-A-chunk-196608-slurm.sh 8000000
sbatch roda-exp-B-chunk-0-slurm.sh 8000000
```

### 5.4. Estrutura dos Arquivos Entregues
O pacote compactado entregue contém os seguintes arquivos organizados por função:

- **Códigos em C e Cabeçalhos:**
  - `prefixSumPth.c`: programa principal e bateria de medições;
  - `prefixSum.c` e `prefixSum.h`: pool de threads, barreiras e algoritmo;
  - `chrono.c`: cronômetro de alta precisão com suporte a desconto de tempo;
  - `verify.c` e `verify.h`: verificação de corretude (flag `-v`).

- **Scripts de Compilação e Automação:**
  - `Makefile`: regras de compilação (`make clean && make all`);
  - `roda.sh`: script de teste sequencial local;
  - `roda-exp-A-chunk-196608-slurm.sh`: bateria da Experiência A no SLURM;
  - `roda-exp-B-chunk-0-slurm.sh`: bateria da Experiência B no SLURM.

- **Planilha e Dados Brutos do SLURM:**
  - `Planilha-PrefixSum-Pthreads-WZ-ExpA-ExpB.xlsx`: planilha com 4 abas e 6 gráficos de Tempo x Vazão;
  - `resultados-expA-chunk-196608-job18977.txt`: resumo de vazão da Exp. A;
  - `saida-expA-job18977.txt`: log completo de execução da Exp. A;
  - `resultados-expB-semChunk-job18980.txt`: resumo de vazão da Exp. B;
  - `saida-expB-job18980.txt`: log completo de execução da Exp. B.

- **Documentação e Topologia:**
  - `topology.pdf`: diagrama de topologia gerado pelo `lstopo` no nó `wz10`;
  - `relatorio.md` e `relatorio.txt`: versões em Markdown e texto puro;
  - `Base_para_trabalhos_UFPR/`: código-fonte LaTeX do relatório.

---

## Apêndice A — Saída Completa do Comando `lscpu` no Nó `wz10`

Saída obtida via `srun -w wz10 lscpu`:

```text
Architecture:                    x86_64
CPU op-mode(s):                  32-bit, 64-bit
Byte Order:                      Little Endian
Address sizes:                   38 bits physical, 48 bits virtual
CPU(s):                          8
On-line CPU(s) list:             0-7
Thread(s) per core:              1
Core(s) per socket:              4
Socket(s):                       2
NUMA node(s):                    1
Vendor ID:                       GenuineIntel
CPU family:                      6
Model:                           23
Model name:                      Intel(R) Xeon(R) CPU           E5410  @ 2.33GHz
Stepping:                        10
CPU MHz:                         2333.000
CPU max MHz:                     2333.0000
CPU min MHz:                     2000.0000
BogoMIPS:                        4655.52
Virtualization:                  VT-x
L1d cache:                       256 KiB
L1i cache:                       256 KiB
L2 cache:                        24 MiB
NUMA node0 CPU(s):               0-7
Vulnerability Itlb multihit:     KVM: Mitigation: VMX disabled
Vulnerability L1tf:              Mitigation; PTE Inversion; VMX EPT disabled
Vulnerability Mds:               Vulnerable: Clear CPU buffers attempted, no microcode; SMT disabled
Vulnerability Meltdown:          Mitigation; PTI
Vulnerability Spec store bypass: Vulnerable
Vulnerability Spectre v1:        Mitigation; usercopy/swapgs barriers and __user pointer sanitization
Vulnerability Spectre v2:        Mitigation; Full generic retpoline, STIBP disabled, RSB filling
Vulnerability Srbds:             Not affected
Vulnerability Tsx async abort:   Not affected
Flags:                           fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ht tm pbe syscall nx lm constant_tsc arch_perfmon pebs bts rep_good nopl cpuid aperfmperf pni dtes64 monitor ds_cpl vmx est tm2 ssse3 cx16 xtpr pdcm dca sse4_1 xsave lahf_lm pti tpr_shadow vnmi flexpriority vpid dtherm
```
