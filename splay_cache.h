#ifndef SPLAY_CACHE_H
#define SPLAY_CACHE_H

#include <stddef.h>
#include <stdint.h>

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 4096
#endif

typedef struct SplayCache SplayCache;

SplayCache* splay_cache_open(const char* filename, size_t capacity);


int splay_cache_read(SplayCache* cache, uint64_t page_id, void* buffer);

int splay_cache_write(SplayCache* cache, uint64_t page_id, const void* buffer);

int splay_cache_flush(SplayCache* cache);

double splay_cache_avg_depth(SplayCache* cache);

void splay_cache_print_stats(SplayCache* cache);

void splay_cache_close(SplayCache* cache);

#endif