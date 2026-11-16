#!/bin/bash
#SBATCH --exclusive
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=1
#SBATCH --job-name=t2-hwinfo
#SBATCH --output=hwinfo-job%j.txt

echo "Coletando informacoes de hardware no nodo: $(hostname)"
lscpu > lscpu.txt
cat lscpu.txt

# No cluster Xeon (wz*), lstopo-no-graphics suporta svg, ascii, console, xml, fig
if command -v lstopo-no-graphics >/dev/null 2>&1; then
    lstopo-no-graphics --of svg topology.svg
    lstopo-no-graphics --of ascii lstopo.txt
elif command -v lstopo >/dev/null 2>&1; then
    lstopo --of svg topology.svg
    lstopo --of ascii lstopo.txt
fi

echo "Arquivos gerados: lscpu.txt, topology.svg, lstopo.txt"
