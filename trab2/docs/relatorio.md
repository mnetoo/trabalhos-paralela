# Relatório Técnico — Trabalho 2: Multiplicação Paralela de Matrizes com MPI e Otimização de Cache por Ladrilhos (Tiling)

- **Disciplina:** Programação Paralela (CI1316) — DInF / UFPR
- **Professor:** Prof. Dr. Wagner M. Nunan Zola
- **Autores:**
  - Henrique Villela Silva (GRR20242612)
  - Marcus Sebastião Adriano Rocha Neto (GRR20240710)

---

## 1. Implementação e Algoritmos Paralelos em MPI

O objetivo deste trabalho é implementar e analisar o desempenho da multiplicação paralela de matrizes $C = A \times B$ em memória distribuída utilizando MPI (*Message Passing Interface*), comparando a abordagem direta de multiplicação (**Versão A**) com a abordagem blocada por ladrilhos (**Versão B — Loop Tiling**) voltada ao aproveitamento da hierarquia de memória cache L2.

### 1.1 Dimensões do Problema e Volume de Operações
Nos experimentos oficiais realizados no cluster, foram adotadas as dimensões especificadas:
- **Matriz $A$:** $N_{la} \times M = 2304 \times 960$ elementos do tipo `double` ($8\text{ bytes}$), ocupando $17.694.720\text{ bytes}$ ($\approx 16,88\text{ MiB}$).
- **Matriz $B$:** $M \times N_{cb} = 960 \times 960$ elementos do tipo `double`, ocupando $7.372.800\text{ bytes}$ ($\approx 7,03\text{ MiB}$).
- **Matriz Resultado $C$:** $N_{la} \times N_{cb} = 2304 \times 960$ elementos do tipo `double`, ocupando $17.694.720\text{ bytes}$ ($\approx 16,88\text{ MiB}$).
- **Total de Operações (FLOPs):**
  $$\text{Total FLOPs} = 2 \times 2304 \times 960 \times 960 = 4.246.732.800\text{ FLOPs} \approx 4,246733\text{ GFLOP}$$

### 1.2 Particionamento Distribuído e Comunicação Coletiva MPI
1. **Verificação de Restrições:** O programa confere se $M$ e $N_{cb}$ são múltiplos de 8 e se $N_{la}$ é perfeitamente divisível por $nprocs$ (`if (M % 8 || Ncb % 8 || Nla % nprocs)`). Na Versão B, confere também se $nl = N_{la} / nprocs$, $M$ e $N_{cb}$ são múltiplos de $w$ (`blockWidth`).
2. **Alocação Sob Demanda:** Apenas o processo 0 aloca as matrizes completas $A$ e $C$. Os processos com $rank > 0$ alocam somente os buffers correspondentes à sua faixa horizontal ($A_{local}[nl \times M]$ e $C_{local}[nl \times N_{cb}]$) e a matriz $B[M \times N_{cb}]$.
3. **Fluxo de Execução Paralela ($nprocs > 1$):**
   - **Distribuição (`MPI_Scatter` + `MPI_Bcast`):** O rank 0 distribui faixas horizontais de $nl = N_{la}/nprocs$ linhas de $A$ entre todos os processos via `MPI_Scatter` (com `MPI_IN_PLACE` no rank 0) e transmite a matriz $B$ inteira via `MPI_Bcast`.
   - **Computação Local:** Cada processo multiplica sua faixa $A_{local}[nl \times M]$ pela matriz $B[M \times N_{cb}]$, gerando $C_{local}[nl \times N_{cb}]$.
   - **Coleta (`MPI_Gather`):** O rank 0 recolhe as faixas $C_{local}$ de todos os processos para formar a matriz $C$ completa.
   - Para $nprocs = 1$, a multiplicação é executada diretamente na memória do processo 0 sem chamadas de comunicação coletiva.

### 1.3 Versão A (Direta) vs. Versão B (Blocada com Ladrilhos)
- **Versão A (`mmul_kernel_versao_A`):** Usa os três laços aninhados na ordem convencional $i \to j \to k$. Enquanto $A[i \times M + k]$ é lida sequencialmente com passo 1, a matriz $B[k \times N_{cb} + j]$ é percorrida por coluna com passo $N_{cb} = 960$ elementos ($7.680\text{ bytes}$). Como $B$ ocupa $7,03\text{ MiB}$ (maior que os $6\text{ MiB}$ da cache L2 compartilhada do Xeon E5410), as linhas de cache trazidas de $B$ sofrem *cache misses* recorrentes.
- **Versão B (`mmul_kernel_versao_B`):** Divide o processamento em ladrilhos quadrados de lado $w = 192$ com três laços externos de bloco ($ii \to kk \to jj$) e três laços internos permutados ($i \to k \to j$). O valor $A[i \times M + k]$ é mantido em registrador (`a_ik`) enquanto o laço mais interno percorre $B[k \times N_{cb} + j]$ e $C[i \times N_{cb} + j]$ contiguamente com passo 1, aproveitando integralmente as linhas de cache e mantendo os blocos ativos residentes na cache L2.

### 1.4 Geração Reprodutível e Verificação Exata (`-v`)
- **`gera_matriz`:** Executada apenas no processo 0 fora do cronômetro, usa `srand(1)` e preenche $A$ e $B$ com inteiros no intervalo $[-4, 4]$ convertidos para `double`.
- **`verifica_resultado`:** Executada apenas no processo 0 fora do cronômetro quando `-v` é especificado. Compara o resultado paralelo com a multiplicação sequencial de referência usando `!=`, retornando `0 erros` em todos os $2.211.840$ elementos para $1, 2, 3$ e $4$ processos em ambas as versões.

---

## 2. Arquitetura do Cluster e Dimensionamento do Ladrilho ($w = 192$)

### 2.1 Nós Alocados (`wz10`, `wz11`, `wz12`, `wz15`)
- **Alocação SLURM:** `--exclusive`, `--nodes=4`, `--ntasks-per-node=1` (1 processo MPI por nó físico, comunicação inter-nós via rede Ethernet).
- **Processador por Nó:** 2x `Intel(R) Xeon(R) CPU E5410 @ 2.33GHz` (4 núcleos por socket, 8 núcleos físicos por nó, sem Hyper-Threading).
- **Memória Cache por Nó:**
  - **L1d / L1i:** $32\text{ KiB}$ + $32\text{ KiB}$ por núcleo ($256\text{ KiB}$ no nó).
  - **L2 Compartilhada:** **$6\text{ MiB}$ ($6.144\text{ KiB}$)** por par de núcleos ($24\text{ MiB}$ no nó). Não há cache L3.

### 2.2 Escolha de $w = 192$
- **Divisibilidade:** $w = 192$ divide perfeitamente $M = 960$ ($5 \times 192$), $N_{cb} = 960$ ($5 \times 192$) e todas as alturas de faixa local $nl \in \{2304, 1152, 768, 576\}$ para $1, 2, 3$ e $4$ processos ($12, 6, 4$ e $3 \times 192$).
- **Ocupação na Cache L2:**
  $$\text{1 Ladrilho } (192 \times 192 \times 8\text{ B}) = 294.912\text{ bytes} = 288\text{ KiB}$$
  $$\text{3 Ladrilhos Ativos } (A, B, C) = 3 \times 288\text{ KiB} = 864\text{ KiB} \ll 6.144\text{ KiB (Cache L2)}$$

---

## 3. Resultados Experimentais

### 3.1 Experiência A — Versão A Direta (8 execuções por configuração, Job 19172)

| Processos (Nós) | Tempo Total Médio (s) | Tempo Comp Médio (s) | Tempo Comm Médio (s) | % Comm | Vazão Média (GFLOPS) | Mín / Máx (GFLOPS) | Speedup Total | Speedup Comp |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **1 Proc (`wz10`)** | 12,8297 | 12,8297 | 0,0000 | 0,00% | **0,3310** | 0,3289 / 0,3329 | 1,00x | 1,00x |
| **2 Procs (`wz10,11`)** | 6,8002 | 6,5511 | 0,2491 | 3,66% | **0,6245** | 0,6205 / 0,6269 | 1,89x | 1,96x |
| **3 Procs (`wz10-12`)** | 4,5800 | 4,2681 | 0,3119 | 6,81% | **0,9273** | 0,9236 / 0,9326 | 2,80x | 3,01x |
| **4 Procs (`wz10-12,15`)** | 3,5545 | 3,2374 | 0,3172 | 8,92% | **1,1948** | 1,1729 / 1,2031 | 3,61x | 3,96x |

### 3.2 Experiência B — Versão B Blocada com $w = 192$ (10 execuções por configuração, Job 19173)

| Processos (Nós) | Tempo Total Médio (s) | Tempo Comp Médio (s) | Tempo Comm Médio (s) | % Comm | Vazão Média (GFLOPS) | Mín / Máx (GFLOPS) | Speedup Total | Speedup Comp |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **1 Proc (`wz10`)** | 2,3829 | 2,3829 | 0,0000 | 0,00% | **1,7822** | 1,7721 / 1,7893 | 1,00x | 1,00x |
| **2 Procs (`wz10,11`)** | 1,4450 | 1,1923 | 0,2527 | 17,49% | **2,9393** | 2,8453 / 2,9790 | 1,65x | 2,00x |
| **3 Procs (`wz10-12`)** | 1,0829 | 0,7904 | 0,2925 | 27,01% | **3,9215** | 3,8912 / 3,9439 | 2,20x | 3,01x |
| **4 Procs (`wz10-12,15`)** | 0,9108 | 0,5949 | 0,3159 | 34,69% | **4,6632** | 4,5807 / 4,7139 | 2,62x | 4,01x |

### 3.3 Comparativo Direto: Experiência A vs. Experiência B

| Processos (Nós) | Tempo Total A (s) | Tempo Total B (s) | Comp A (s) | Comp B (s) | Vazão A (GFLOPS) | Vazão B (GFLOPS) | Aceleração Total (A/B) | Aceleração Comp (A/B) | Ganho de Vazão (B vs A) |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **1** | 12,8297 | 2,3829 | 12,8297 | 2,3829 | 0,3310 | 1,7822 | **5,38x** | **5,38x** | **+438,40%** |
| **2** | 6,8002 | 1,4450 | 6,5511 | 1,1923 | 0,6245 | 2,9393 | **4,71x** | **5,49x** | **+370,66%** |
| **3** | 4,5800 | 1,0829 | 4,2681 | 0,7904 | 0,9273 | 3,9215 | **4,23x** | **5,40x** | **+322,92%** |
| **4** | 3,5545 | 0,9108 | 3,2374 | 0,5949 | 1,1948 | 4,6632 | **3,90x** | **5,44x** | **+290,29%** |

---

## 4. Discussão e Conclusões

1. **Impacto da Blocagem em Ladrilhos ($w = 192$):** A Versão B acelerou o tempo de computação entre **5,38x e 5,49x** em todas as configurações de nós, elevando a vazão máxima média de $1,1948\text{ GFLOPS}$ para **$4,6632\text{ GFLOPS}$** em 4 nós (pico de $4,7139\text{ GFLOPS}$).
2. **Escalabilidade Linear da Computação:** O tempo exclusivo de computação apresentou Speedup de **3,96x** na Versão A e **4,01x** na Versão B com 4 nós, demonstrando balanceamento de carga ideal entre os processos MPI.
3. **Peso Relativo da Comunicação na Rede Ethernet:** O tempo absoluto de comunicação (`MPI_Scatter` + `MPI_Bcast` + `MPI_Gather`) permaneceu em $\approx 0,25\text{ s}$ (2 nós) a $\approx 0,316\text{ s}$ (4 nós) em ambas as versões. Contudo, como a computação na Versão B ficou $5,44\times$ mais rápida ($0,5949\text{ s}$ em 4 nós), a comunicação passou a representar $34,69\%$ do tempo total na Versão B contra apenas $8,92\%$ na Versão A, explicando a diferença no Speedup Total ($2,62\times$ vs. $3,61\times$) conforme previsto pela Lei de Amdahl.
