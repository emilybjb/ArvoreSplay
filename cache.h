#ifndef CACHE_H
#define CACHE_H

#include <stddef.h>
#include <stdint.h>

#define BLOCK_SIZE 4096

typedef struct Cache Cache;

typedef struct {
    size_t hits;
    size_t misses;
    size_t total_accesses;
    size_t evictions;
    size_t dirty_writes;
    double avg_depth;
} CacheStats;

Cache* cache_open(const char* filename, size_t capacity);

int cache_read(Cache* cache, uint64_t page_id, void* buffer);

int cache_write(Cache* cache, uint64_t page_id, const void* buffer);

int cache_flush(Cache* cache);

void cache_print_stats(Cache* cache);

void cache_close(Cache* cache);

CacheStats cache_get_stats(Cache* cache);

#endif