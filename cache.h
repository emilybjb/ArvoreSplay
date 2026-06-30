#ifndef CACHE_H
#define CACHE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define BLOCK_SIZE 4096

typedef struct Cache Cache;

Cache* cache_open(const char* filename, size_t capacity);
int cache_read(Cache* cache, uint64_t page_id, void* buffer);
int cache_write(Cache* cache, uint64_t page_id, const void* buffer);
int cache_flush(Cache* cache);
void cache_print_stats(Cache* cache);
void cache_close(Cache* cache);
void cache_write_csv(FILE* csv,
    Cache* cache,
    const char* politica,
    const char* carga,
    int threads,
    int acessos,
    double tempo);

#endif