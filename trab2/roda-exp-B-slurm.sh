#!/bin/bash
#SBATCH --exclusive
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=1
#SBATCH --job-name=t2-expB
#SBATCH --output=resultados-expB-blocada-job%j.txt

# OBS: a saida resumida do SLURM vai para resultados-expB-blocada-job%j.txt
#      e todos os detalhes de cada execucao vao para saida-expB-job$SLURM_JOBID.txt

NLA=${1:-2304}
M=${2:-960}
NCB=${3:-960}
BLOCK_W=${4:-192}
MAX_EXECS=${5:-8}

echo "USAGE: sbatch roda-exp-B-slurm.sh [Nla] [M] [Ncb] [blockWidth] [MAX_EXECS]"
echo "$0 rodando no host " `hostname`

SAIDA=saida-expB-w${BLOCK_W}-job$SLURM_JOBID.txt
echo "$0 rodando no host " `hostname` >$SAIDA

echo "SLURM_JOB_NAME: " $SLURM_JOB_NAME | tee -a $SAIDA
echo "SLURM_NODELIST: " $SLURM_NODELIST | tee -a $SAIDA
echo "SLURM_JOB_NODELIST: " $SLURM_JOB_NODELIST | tee -a $SAIDA
echo "SLURM_JOB_CPUS_PER_NODE: " $SLURM_JOB_CPUS_PER_NODE | tee -a $SAIDA

NTIMES=10
echo "nt " $NTIMES | tee -a $SAIDA
echo "MAX_EXECS " $MAX_EXECS | tee -a $SAIDA

echo "------- rodando a Experiencia B (Versao B - Blocada w=${BLOCK_W}): A[${NLA}x${M}] * B[${M}x${NCB}] -------" | tee -a $SAIDA
echo "(tempo acumulado: $SECONDS segundos)" | tee -a $SAIDA

for np in 1 2 3 4
do
    echo "================================================================" | tee -a $SAIDA
    echo "Executando $MAX_EXECS vezes Versao B (${NLA}x${M} * ${M}x${NCB}, -w ${BLOCK_W}) com $np processos MPI (nodos distintos): (tempo acumulado: $SECONDS segundos)" | tee -a $SAIDA
    for j in $(seq 1 $NTIMES)
    do
        echo "-----------------------" >>$SAIDA
        if [ $j -le $MAX_EXECS ]; then
            mpirun -np $np --map-by node ./mmul $NLA $M $NCB -w $BLOCK_W | tee -a $SAIDA | grep -E "Rank|total_time_in_seconds|computation_time_in_seconds|communication_time_in_seconds|Throughput"
        else
            echo "nao executado, j=" $j | tee -a $SAIDA
        fi
    done
done

echo "O tempo total dessa shell foi de" $SECONDS "segundos" | tee -a $SAIDA
echo "SLURM_JOB_NAME: " $SLURM_JOB_NAME
echo "SLURM_NODELIST: " $SLURM_NODELIST
echo "SLURM_JOB_NODELIST: " $SLURM_JOB_NODELIST
echo "SLURM_JOB_CPUS_PER_NODE: " $SLURM_JOB_CPUS_PER_NODE
squeue -j $SLURM_JOBID
