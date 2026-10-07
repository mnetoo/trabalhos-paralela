echo sugestao com poucos elementos: 
echo    ./test-native 8000000 8
echo sugestao com MAIS elementos: \(mais facil de ver a otimização\)
echo    ./test-native 64000000 8

N=$1
nt=$2

echo --------------------------------------------
echo taskset -c 0-7 ./reduceSumPth-double-native $N $nt
echo compilado -O3 com -march=native
taskset -c 0-7 ./reduceSumPth-double-native $N $nt  | grep MOPS
taskset -c 0-7 ./reduceSumPth-double-native $N $nt | grep MOPS
taskset -c 0-7 ./reduceSumPth-double-native $N $nt | grep MOPS
taskset -c 0-7 ./reduceSumPth-double-native $N $nt | grep MOPS
taskset -c 0-7 ./reduceSumPth-double-native $N $nt | grep MOPS

echo
echo taskset -c 0-7 ./reduceSumPth-double $N $nt
echo compilado -O3 SEM -march=native
taskset -c 0-7 ./reduceSumPth-double $N $nt | grep MOPS
taskset -c 0-7 ./reduceSumPth-double $N $nt | grep MOPS
taskset -c 0-7 ./reduceSumPth-double $N $nt | grep MOPS
taskset -c 0-7 ./reduceSumPth-double $N $nt | grep MOPS
taskset -c 0-7 ./reduceSumPth-double $N $nt | grep MOPS
