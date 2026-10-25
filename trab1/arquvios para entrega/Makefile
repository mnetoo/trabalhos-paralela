CC = gcc
CFLAGS = -Wall -Wextra -O3 -pthread -std=gnu11
LDFLAGS = -pthread

SRCS = verify.c prefixSum.c prefixSumPth.c
OBJS = $(SRCS:.c=.o)

TARGET = prefixSumPth
TARGET_SLURM = prefixSumPth-v3-longlong
TARGET_ALT = prefixSumPth-longlong

.PHONY: all clean native debug test run

all: $(TARGET) $(TARGET_SLURM) $(TARGET_ALT)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(TARGET_SLURM): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(TARGET_ALT): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

native: CFLAGS += -march=native
native: clean all

debug: CFLAGS = -Wall -Wextra -g -O0 -pthread -std=gnu11 -DDEBUG=1
debug: clean all

test: $(TARGET)
	@echo "=== Executando Testes de Corretude (-v) ==="
	@echo ">> Teste 1: 1 thread, 100000 elementos, chunkSize=0"
	./$(TARGET) 100000 0 1 -v
	@echo ""
	@echo ">> Teste 2: 2 threads, 100000 elementos, chunkSize=0"
	./$(TARGET) 100000 0 2 -v
	@echo ""
	@echo ">> Teste 3: 4 threads, 100000 elementos, chunkSize=1024"
	./$(TARGET) 100000 1024 4 -v
	@echo ""
	@echo ">> Teste 4: 8 threads, 100003 elementos (tamanho primo), chunkSize=512"
	./$(TARGET) 100003 512 8 -v
	@echo ""
	@echo "=== Todos os testes foram executados com sucesso! ==="

run: $(TARGET)
	./$(TARGET) 8000000 0 8

clean:
	-rm -f $(OBJS) $(TARGET) $(TARGET_SLURM) $(TARGET_ALT) $(TARGET).exe $(TARGET_SLURM).exe $(TARGET_ALT).exe saida.txt resultados saida*.txt resultados*.txt
	-del /f /q $(OBJS) $(TARGET).exe $(TARGET_SLURM).exe $(TARGET_ALT).exe saida.txt resultados 2>nul
