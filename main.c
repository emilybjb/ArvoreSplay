#include "cache.h"
#include "splay_cache.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define NUM_ACESSOS 10000
#define CACHE_CAPACITY 8
#define NUM_PAGINAS 64

typedef enum {
    PADRAO_UNIFORME,
    PADRAO_ZIPFIANO
} PadraoAcesso;

uint64_t gerar_uniforme() {
    return rand() % NUM_PAGINAS;
}

uint64_t gerar_zipfiano() {
    double r = (double) rand() / RAND_MAX;

    if (r < 0.50) {
        return rand() % 4;          
    } else if (r < 0.80) {
        return 4 + (rand() % 12);  
    } else {
        return 16 + (rand() % 48); 
    }
}

uint64_t gerar_pagina(PadraoAcesso padrao) {
    if (padrao == PADRAO_UNIFORME) {
        return gerar_uniforme();
    }

    return gerar_zipfiano();
}

void testar_lru(PadraoAcesso padrao) {
    Cache* cache = cache_open("data.bin", CACHE_CAPACITY);

    if (cache == NULL) {
        printf("Erro ao abrir cache LRU.\n");
        return;
    }

    char buffer[BLOCK_SIZE];

    clock_t inicio = clock();

    for (int i = 0; i < NUM_ACESSOS; i++) {
        uint64_t page_id = gerar_pagina(padrao);
        cache_read(cache, page_id, buffer);
    }

    clock_t fim = clock();

    double tempo = (double)(fim - inicio) / CLOCKS_PER_SEC;

    printf("\n========== RESULTADO LRU ==========\n");
    cache_print_stats(cache);
    printf("Tempo LRU: %.6f segundos\n", tempo);

    cache_close(cache);
}

void testar_splay(PadraoAcesso padrao) {
    SplayCache* cache = splay_cache_open("data.bin", CACHE_CAPACITY);

    if (cache == NULL) {
        printf("Erro ao abrir cache Splay.\n");
        return;
    }

    char buffer[BLOCK_SIZE];

    clock_t inicio = clock();

    for (int i = 0; i < NUM_ACESSOS; i++) {
        uint64_t page_id = gerar_pagina(padrao);
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

void executar_benchmark(const char* nome, PadraoAcesso padrao) {
    printf("\n\n=====================================\n");
    printf("BENCHMARK: %s\n", nome);
    printf("Acessos: %d\n", NUM_ACESSOS);
    printf("Capacidade do cache: %d paginas\n", CACHE_CAPACITY);
    printf("Total de paginas possiveis: %d\n", NUM_PAGINAS);
    printf("=====================================\n");

    srand(42);
    testar_lru(padrao);

    srand(42);
    testar_splay(padrao);
}

int main() {
    executar_benchmark("Carga uniforme", PADRAO_UNIFORME);
    executar_benchmark("Carga zipfiana", PADRAO_ZIPFIANO);

    return 0;
}