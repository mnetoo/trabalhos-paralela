#!/bin/bash
#SBATCH --exclusive
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --job-name=t2-hwinfo
#SBATCH --output=hwinfo-job%j.txt

echo "Coletando informacoes de hardware no nodo: $(hostname)"
lscpu > lscpu.txt
cat lscpu.txt

if command -v lstopo-no-graphics >/dev/null 2>&1; then
    lstopo-no-graphics --of pdf topology.pdf
    lstopo-no-graphics --of txt lstopo.txt
elif command -v lstopo >/dev/null 2>&1; then
    lstopo --of pdf topology.pdf
    lstopo --of txt lstopo.txt
fi

echo "Arquivos gerados: lscpu.txt, topology.pdf, lstopo.txt"
