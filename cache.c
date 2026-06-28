#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Page {
    uint64_t page_id;
    char data[BLOCK_SIZE];
    int dirty;
    size_t last_used;
} Page;

struct Cache {
    FILE* file;
    size_t capacity;
    size_t size;

    Page* pages;

    size_t hits;
    size_t misses;
    size_t total_accesses;

    size_t evictions;
    size_t dirty_writes;

    size_t clock;
};

Cache* cache_open(const char* filename, size_t capacity) {
    Cache* cache = malloc(sizeof(Cache));
    if (!cache) return NULL;

    cache->file = fopen(filename, "r+b");

    if (!cache->file) {
        cache->file = fopen(filename, "w+b");
        if (!cache->file) {
            free(cache);
            return NULL;
        }
    }

    cache->capacity = capacity;
    cache->size = 0;
    cache->hits = 0;
    cache->misses = 0;
    cache->total_accesses = 0;
    cache->evictions = 0;
    cache->dirty_writes = 0;
    cache->clock = 0;
    cache->pages = calloc(capacity, sizeof(Page));

    if (!cache->pages) {
        fclose(cache->file);
        free(cache);
        return NULL;
    }

    return cache;
}

static int find_lru_page(Cache* cache) {
    size_t lru_index = 0;
    size_t menor_uso = cache->pages[0].last_used;

    for (size_t i = 1; i < cache->size; i++) {
        if (cache->pages[i].last_used < menor_uso) {
            menor_uso = cache->pages[i].last_used;
            lru_index = i;
        }
    }

    return (int) lru_index;
}

static int find_page(Cache* cache, uint64_t page_id) {
    for (size_t i = 0; i < cache->size; i++) {
        if (cache->pages[i].page_id == page_id) {
            return (int)i;
        }
    }

    return -1;
}

int cache_read(Cache* cache, uint64_t page_id, void* buffer) {
    cache->total_accesses++;
    cache->clock++;

    int index = find_page(cache, page_id);

    if (index >= 0) {
        cache->hits++;

        cache->pages[index].last_used = cache->clock;

        memcpy(buffer, cache->pages[index].data, BLOCK_SIZE);
        return 1;
    }

    cache->misses++;

    if (cache->size >= cache->capacity) {
        cache->evictions++;

        int lru_index = find_lru_page(cache);

        if (cache->pages[lru_index].dirty) {
            fseek(cache->file, cache->pages[lru_index].page_id * BLOCK_SIZE, SEEK_SET);
            fwrite(cache->pages[lru_index].data, 1, BLOCK_SIZE, cache->file);

            cache->dirty_writes++;
        }

        cache->pages[lru_index] = cache->pages[cache->size - 1];
        cache->size--;
    }

    Page* page = &cache->pages[cache->size];

    page->page_id = page_id;
    page->dirty = 0;
    page->last_used = cache->clock;

    fseek(cache->file, page_id * BLOCK_SIZE, SEEK_SET);
    fread(page->data, 1, BLOCK_SIZE, cache->file);

    memcpy(buffer, page->data, BLOCK_SIZE);

    cache->size++;

    return 0;
}

int cache_write(Cache* cache, uint64_t page_id, const void* buffer) {
    char temp[BLOCK_SIZE];

    int hit = cache_read(cache, page_id, temp);

    int index = find_page(cache, page_id);
    if (index < 0) return -1;

    memcpy(cache->pages[index].data, buffer, BLOCK_SIZE);
    cache->pages[index].dirty = 1;

    return hit;
}

int cache_flush(Cache* cache) {
    for (size_t i = 0; i < cache->size; i++) {
        if (cache->pages[i].dirty) {
            fseek(cache->file, cache->pages[i].page_id * BLOCK_SIZE, SEEK_SET);
            fwrite(cache->pages[i].data, 1, BLOCK_SIZE, cache->file);
        
            cache->dirty_writes++;
        
            cache->pages[i].dirty = 0;
        }
    }

    fflush(cache->file);
    return 0;
}

void cache_print_stats(Cache* cache) {
    double hit_ratio = 0.0;

    if (cache->total_accesses > 0) {
        hit_ratio = (double) cache->hits / cache->total_accesses;
    }

    printf("\n===== Estatísticas do Cache =====\n");
    printf("Total de acessos: %zu\n", cache->total_accesses);
    printf("Acertos (Hits): %zu\n", cache->hits);
    printf("Faltas (Misses): %zu\n", cache->misses);
    printf("Taxa de acertos: %.2f%%\n", hit_ratio * 100.0);
    printf("Páginas removidas: %zu\n", cache->evictions);
    printf("Páginas sujas gravadas: %zu\n", cache->dirty_writes);
}

void cache_close(Cache* cache) {
    if (!cache) return;

    cache_flush(cache);

    fclose(cache->file);
    free(cache->pages);
    free(cache);
}