#!/bin/bash
#SBATCH --exclusive
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --job-name=t2-hwinfo
#SBATCH --output=hwinfo-job%j.txt

# CI1316 - Programacao Paralela - Trabalho 2
# Autores:
#   Henrique Villela Silva (GRR20242612)
#   Marcus Sebastiao Adriano Rocha Neto (GRR20240710)

echo "Coletando informacoes de hardware no nodo: $(hostname)"
lscpu > lscpu.txt
cat lscpu.txt

if command -v lstopo-no-graphics >/dev/null 2>&1; then
    lstopo-no-graphics --of svg topology.svg
    lstopo-no-graphics --of ascii lstopo.txt
elif command -v lstopo >/dev/null 2>&1; then
    lstopo --of svg topology.svg
    lstopo --of ascii lstopo.txt
fi

echo "Arquivos gerados: lscpu.txt, topology.svg, lstopo.txt"
