#!/bin/bash
# Script para rodar no nodo de login (ex: w00) SEM usar slurm, apenas para
# verificar a correcao (-v) das Versoes A e B com 1, 2, 3 e 4 processos MPI.

NLA=${1:-2304}
M=${2:-960}
NCB=${3:-960}
BLOCK_W=${4:-192}

if [ ! -x ./mmul ]; then
    echo "Compilando mmul..."
    make
fi

echo "================================================================"
echo "VERIFICACAO DE CORRECAO (-v) - VERSAO A (Simples/Direta)"
echo "Matrizes: A[${NLA}x${M}] * B[${M}x${NCB}] = C[${NLA}x${NCB}]"
echo "================================================================"
for np in 1 2 3 4
do
    echo ""
    echo ">>> Testando Versao A com -np $np:"
    mpirun --oversubscribe -np $np ./mmul $NLA $M $NCB -v
done

echo ""
echo "================================================================"
echo "VERIFICACAO DE CORRECAO (-v) - VERSAO B (Blocada, -w ${BLOCK_W})"
echo "Matrizes: A[${NLA}x${M}] * B[${M}x${NCB}] = C[${NLA}x${NCB}]"
echo "================================================================"
for np in 1 2 3 4
do
    echo ""
    echo ">>> Testando Versao B (-w ${BLOCK_W}) com -np $np:"
    mpirun --oversubscribe -np $np ./mmul $NLA $M $NCB -w $BLOCK_W -v
done
