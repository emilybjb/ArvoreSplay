#include "experiments.h"

#include "cache.h"
#include "splay_cache.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include "metrics.h"

#define NUM_ACESSOS 10000
#define CACHE_CAPACITY 8
#define NUM_PAGINAS 64

#define NUM_THREADS 4
#define ACESSOS_POR_THREAD (NUM_ACESSOS / NUM_THREADS)

typedef enum {
    PADRAO_UNIFORME,
    PADRAO_ZIPFIANO
} PadraoAcesso;

typedef struct {
    Cache* lru_cache;
    SplayCache* splay_cache;
    PadraoAcesso padrao;
    int usar_splay;
    unsigned int seed;
} ThreadArgs;

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

void* worker_thread(void* arg) {
    ThreadArgs* args = (ThreadArgs*) arg;

    char buffer[BLOCK_SIZE];

    for (int i = 0; i < ACESSOS_POR_THREAD; i++) {
        uint64_t page_id;

        double r = (double) rand_r(&args->seed) / RAND_MAX;

        if (args->padrao == PADRAO_UNIFORME) {
            page_id = rand_r(&args->seed) % NUM_PAGINAS;
        } else {
            if (r < 0.50) {
                page_id = rand_r(&args->seed) % 4;
            } else if (r < 0.80) {
                page_id = 4 + (rand_r(&args->seed) % 12);
            } else {
                page_id = 16 + (rand_r(&args->seed) % 48);
            }
        }

        if (args->usar_splay) {
            splay_cache_read(args->splay_cache, page_id, buffer);
        } else {
            cache_read(args->lru_cache, page_id, buffer);
        }
    }

    return NULL;
}

void testar_lru(PadraoAcesso padrao, const char* nome_carga, FILE* csv) {

    Cache* cache = cache_open("data.bin", CACHE_CAPACITY);

    if (cache == NULL) {
        printf("Erro ao abrir cache LRU.\n");
        return;
    }

    pthread_t threads[NUM_THREADS];
    ThreadArgs args[NUM_THREADS];

    clock_t inicio = clock();

    for (int i = 0; i < NUM_THREADS; i++) {

        args[i].lru_cache = cache;
        args[i].splay_cache = NULL;
        args[i].padrao = padrao;
        args[i].usar_splay = 0;
        args[i].seed = 42 + i;

        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_t fim = clock();

    double tempo = (double)(fim - inicio) / CLOCKS_PER_SEC;

    printf("\n========== RESULTADO LRU ==========\n");
    cache_print_stats(cache);
    printf("Tempo LRU: %.6f segundos\n", tempo);

    CacheStats stats = cache_get_stats(cache);

double hit_ratio = 0.0;

if (stats.total_accesses > 0) {
    hit_ratio = (double) stats.hits / stats.total_accesses * 100.0;
}

ResultadoBenchmark resultado = {
    .politica = "LRU",
    .carga = nome_carga,
    .threads = NUM_THREADS,
    .acessos = NUM_ACESSOS,
    .hits = stats.hits,
    .misses = stats.misses,
    .hit_ratio = hit_ratio,
    .evictions = stats.evictions,
    .dirty_writes = stats.dirty_writes,
    .tempo = tempo,
    .avg_depth = 0.0
};

metrics_write_result(csv, resultado);

    cache_close(cache);
}

void testar_splay(PadraoAcesso padrao, const char* nome_carga, FILE* csv) {

    SplayCache* cache = splay_cache_open("data.bin", CACHE_CAPACITY);

    if (cache == NULL) {
        printf("Erro ao abrir cache Splay.\n");
        return;
    }

    pthread_t threads[NUM_THREADS];
    ThreadArgs args[NUM_THREADS];

    clock_t inicio = clock();

    for (int i = 0; i < NUM_THREADS; i++) {

        args[i].lru_cache = NULL;
        args[i].splay_cache = cache;
        args[i].padrao = padrao;
        args[i].usar_splay = 1;
        args[i].seed = 42 + i;

        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_t fim = clock();

    double tempo = (double)(fim - inicio) / CLOCKS_PER_SEC;

    printf("\n========== RESULTADO SPLAY ==========\n");
    splay_cache_print_stats(cache);
    printf("Profundidade media da Splay: %.2f\n",
           splay_cache_avg_depth(cache));
    printf("Tempo Splay: %.6f segundos\n", tempo);

    SplayCacheStats stats = splay_cache_get_stats(cache);

double hit_ratio = 0.0;

if (stats.total_accesses > 0) {
    hit_ratio = (double) stats.hits / stats.total_accesses * 100.0;
}

ResultadoBenchmark resultado = {
    .politica = "SPLAY",
    .carga = nome_carga,
    .threads = NUM_THREADS,
    .acessos = NUM_ACESSOS,
    .hits = stats.hits,
    .misses = stats.misses,
    .hit_ratio = hit_ratio,
    .evictions = stats.evictions,
    .dirty_writes = stats.dirty_writes,
    .tempo = tempo,
    .avg_depth = stats.avg_depth
};

metrics_write_result(csv, resultado);

    splay_cache_close(cache);
}

void executar_benchmark(const char* nome, PadraoAcesso padrao, FILE* csv) {
    printf("\n\n=====================================\n");
    printf("BENCHMARK: %s\n", nome);
    printf("Acessos: %d\n", NUM_ACESSOS);
    printf("Capacidade do cache: %d paginas\n", CACHE_CAPACITY);
    printf("Total de paginas possiveis: %d\n", NUM_PAGINAS);
    printf("=====================================\n");

    srand(42);
    testar_lru(padrao, nome, csv);

    srand(42);
    testar_splay(padrao, nome, csv);
}

void executar_experimentos(void) {
    FILE* csv = fopen("resultados.csv", "w");

    if (!csv) {
        printf("Erro ao criar resultados.csv\n");
        return;
    }

    metrics_write_header(csv);

    executar_benchmark("uniforme", PADRAO_UNIFORME, csv);
    executar_benchmark("zipfiana", PADRAO_ZIPFIANO, csv);

    fclose(csv);

    printf("\nArquivo resultados.csv gerado com sucesso.\n");
}