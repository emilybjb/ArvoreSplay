// cache.c
#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Page {
    uint64_t page_id;
    char data[BLOCK_SIZE];
    int dirty;
} Page;

struct Cache {
    FILE* file;
    size_t capacity;
    size_t size;

    Page* pages;

    size_t hits;
    size_t misses;
    size_t total_accesses;
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
    cache->pages = calloc(capacity, sizeof(Page));

    if (!cache->pages) {
        fclose(cache->file);
        free(cache);
        return NULL;
    }

    return cache;
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

    int index = find_page(cache, page_id);

    if (index >= 0) {
        cache->hits++;
        memcpy(buffer, cache->pages[index].data, BLOCK_SIZE);
        return 1; 
    }

    cache->misses++;

    if (cache->size >= cache->capacity) {
        if (cache->pages[0].dirty) {
            fseek(cache->file, cache->pages[0].page_id * BLOCK_SIZE, SEEK_SET);
            fwrite(cache->pages[0].data, 1, BLOCK_SIZE, cache->file);
        }

        memmove(&cache->pages[0], &cache->pages[1],
                sizeof(Page) * (cache->capacity - 1));

        cache->size--;
    }

    Page* page = &cache->pages[cache->size];
    page->page_id = page_id;
    page->dirty = 0;

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
            cache->pages[i].dirty = 0;
        }
    }

    fflush(cache->file);
    return 0;
}

void cache_close(Cache* cache) {
    if (!cache) return;

    cache_flush(cache);

    fclose(cache->file);
    free(cache->pages);
    free(cache);
}