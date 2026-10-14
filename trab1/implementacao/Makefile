CC = gcc
CFLAGS = -Wall -Wextra -O3 -pthread -std=c11
LDFLAGS = -pthread

SRCS = chrono.c verify.c prefixSum.c prefixSumPth.c
OBJS = $(SRCS:.c=.o)
TARGET = prefixSumPth

.PHONY: all clean native debug test run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

native: CFLAGS += -march=native
native: clean $(TARGET)

debug: CFLAGS = -Wall -Wextra -g -O0 -pthread -std=c11 -DDEBUG=1
debug: clean $(TARGET)

test: $(TARGET)
	@echo "=== Executando Testes de Corretude (-v) ==="
	@echo ">> Teste 1: 1 thread, 100000 elementos, chunkSize=0"
	./$(TARGET) 100000 1 0 -v
	@echo "\n>> Teste 2: 2 threads, 100000 elementos, chunkSize=0"
	./$(TARGET) 100000 2 0 -v
	@echo "\n>> Teste 3: 4 threads, 100000 elementos, chunkSize=1024"
	./$(TARGET) 100000 4 1024 -v
	@echo "\n>> Teste 4: 8 threads, 100003 elementos (tamanho primo), chunkSize=512"
	./$(TARGET) 100003 8 512 -v
	@echo "\n=== Todos os testes foram executados com sucesso! ==="

run: $(TARGET)
	./$(TARGET) 8000000 8 0

clean:
	rm -f $(OBJS) $(TARGET) saida.txt resultados
