#include "cache.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int main() {
    Cache* cache = cache_open("data.bin", 8);

    if (cache == NULL) {
        printf("Erro ao abrir o cache.\n");
        return 1;
    }

    char buffer[BLOCK_SIZE];

    clock_t start = clock();

    for (int i = 0; i < 10000; i++) {
        uint64_t page_id = i % 4;
        cache_read(cache, page_id, buffer);
    }

    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    cache_print_stats(cache);

    printf("Tempo de execucao: %.6f segundos\n", elapsed);

    cache_close(cache);

    return 0;
}