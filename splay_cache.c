#include "splay_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SplayNode {
    uint64_t page_id;
    char data[BLOCK_SIZE];
    int dirty;

    struct SplayNode *left;
    struct SplayNode *right;
    struct SplayNode *parent;

} SplayNode;

struct SplayCache {

    FILE *file;

    size_t capacity;
    size_t size;

    SplayNode *root;

    size_t hits;
    size_t misses;
    size_t total_accesses;

    size_t evictions;
    size_t dirty_writes;
};

SplayCache *splay_cache_open(const char *filename, size_t capacity)
{
    SplayCache *sc = calloc(1, sizeof(SplayCache));

    if (sc == NULL)
        return NULL;

    sc->file = fopen(filename, "r+b");

    if (sc->file == NULL)
        sc->file = fopen(filename, "w+b");

    if (sc->file == NULL) {
        free(sc);
        return NULL;
    }

    sc->capacity = capacity;
    sc->size = 0;
    sc->root = NULL;

    return sc;
}