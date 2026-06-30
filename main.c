#include "cache.h"
#include "splay_cache.h"

#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define NUM_ACESSOS 10000
#define CACHE_CAPACITY 8

void testar_lru() {
    Cache* cache = cache_open("data.bin", CACHE_CAPACITY);

    if (cache == NULL) {
        printf("Erro ao abrir cache LRU.\n");
        return;
    }

    char buffer[BLOCK_SIZE];

    clock_t inicio = clock();

    for (int i = 0; i < NUM_ACESSOS; i++) {
        uint64_t page_id = i % 16;
        cache_read(cache, page_id, buffer);
    }

    clock_t fim = clock();

    double tempo = (double)(fim - inicio) / CLOCKS_PER_SEC;

    printf("\n========== RESULTADO LRU ==========\n");
    cache_print_stats(cache);
    printf("Tempo LRU: %.6f segundos\n", tempo);

    cache_close(cache);
}

void testar_splay() {
    SplayCache* cache = splay_cache_open("data.bin", CACHE_CAPACITY);

    if (cache == NULL) {
        printf("Erro ao abrir cache Splay.\n");
        return;
    }

    char buffer[BLOCK_SIZE];

    clock_t inicio = clock();

    for (int i = 0; i < NUM_ACESSOS; i++) {
        uint64_t page_id = i % 16;
        splay_cache_read(cache, page_id, buffer);
    }

    clock_t fim = clock();

    double tempo = (double)(fim - inicio) / CLOCKS_PER_SEC;

    printf("\n========== RESULTADO SPLAY ==========\n");
    splay_cache_print_stats(cache);
    printf("Profundidade media da Splay: %.2f\n", splay_cache_avg_depth(cache));
    printf("Tempo Splay: %.6f segundos\n", tempo);

    splay_cache_close(cache);
}

int main() {
    printf("Benchmark: LRU x Arvore Splay\n");
    printf("Acessos: %d\n", NUM_ACESSOS);
    printf("Capacidade do cache: %d paginas\n", CACHE_CAPACITY);
    printf("Padrao de acesso: conjunto maior que o cache, page_id = i %% 16\n");

    testar_lru();
    testar_splay();

    return 0;
}