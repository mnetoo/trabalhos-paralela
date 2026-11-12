CC = mpicc
CFLAGS = -Wall -Wextra -O3 -std=gnu11
LDFLAGS = -lm

TARGET = mmul

NLA = 2304
M = 960
NCB = 960
BLOCK_W = 192

.PHONY: all clean native debug test run run-b

all: $(TARGET)

$(TARGET): mmul.c mmul.h chrono.c chrono.h
	$(CC) $(CFLAGS) mmul.c -o $(TARGET) $(LDFLAGS)

native: CFLAGS += -march=native -funroll-loops
native: clean all

debug: CFLAGS = -Wall -Wextra -g -O0 -std=gnu11 -DDEBUG=1
debug: clean all

test: $(TARGET)
	@echo "=== Executando Testes de Corretude (-v) ==="
	@echo ">> Teste 1: Versao A - 1 processo MPI (sequencial)"
	mpirun --oversubscribe -np 1 ./$(TARGET) 384 192 192 -v
	@echo ""
	@echo ">> Teste 2: Versao A - 2 processos MPI"
	mpirun --oversubscribe -np 2 ./$(TARGET) 384 192 192 -v
	@echo ""
	@echo ">> Teste 3: Versao A - 4 processos MPI"
	mpirun --oversubscribe -np 4 ./$(TARGET) 384 192 192 -v
	@echo ""
	@echo ">> Teste 4: Versao B (w=64) - 1 processo MPI (sequencial)"
	mpirun --oversubscribe -np 1 ./$(TARGET) 384 192 192 -w 64 -v
	@echo ""
	@echo ">> Teste 5: Versao B (w=192) - 2 processos MPI"
	mpirun --oversubscribe -np 2 ./$(TARGET) $(NLA) $(M) $(NCB) -w $(BLOCK_W) -v
	@echo ""
	@echo ">> Teste 6: Versao B (w=192) - 4 processos MPI"
	mpirun --oversubscribe -np 4 ./$(TARGET) $(NLA) $(M) $(NCB) -w $(BLOCK_W) -v
	@echo ""
	@echo "=== Todos os testes foram executados com sucesso! ==="

run: $(TARGET)
	mpirun -np 4 ./$(TARGET) $(NLA) $(M) $(NCB)

run-b: $(TARGET)
	mpirun -np 4 ./$(TARGET) $(NLA) $(M) $(NCB) -w $(BLOCK_W)

clean:
	-rm -f *.o $(TARGET) $(TARGET).exe
	-del /f /q *.o $(TARGET).exe 2>nul
