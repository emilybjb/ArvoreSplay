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

static void rotate_right(SplayCache* sc, SplayNode* y) {
    SplayNode* x = y->left;

    y->left = x->right;
    if (x->right)
        x->right->parent = y;

    x->parent = y->parent;

    if (!y->parent)
        sc->root = x;
    else if (y == y->parent->right)
        y->parent->right = x;
    else
        y->parent->left = x;

    x->right = y;
    y->parent = x;
}

static void rotate_left(SplayCache* sc, SplayNode* x) {
    SplayNode* y = x->right;

    x->right = y->left;
    if (y->left)
        y->left->parent = x;

    y->parent = x->parent;

    if (!x->parent)
        sc->root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;

    y->left = x;
    x->parent = y;
}

static void splay(SplayCache* sc, SplayNode* x) {
    while (x->parent) {

        SplayNode* p = x->parent;
        SplayNode* g = p->parent;

        if (!g) {

            if (x == p->left)
                rotate_right(sc, p);
            else
                rotate_left(sc, p);

        } else if (x == p->left && p == g->left) {

            rotate_right(sc, g);
            rotate_right(sc, p);

        } else if (x == p->right && p == g->right) {

            rotate_left(sc, g);
            rotate_left(sc, p);

        } else if (x == p->right && p == g->left) {

            rotate_left(sc, p);
            rotate_right(sc, g);

        } else {

            rotate_right(sc, p);
            rotate_left(sc, g);
        }
    }
}

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

static SplayNode *find_node(SplayCache *cache, uint64_t page_id) {
    SplayNode *cur = cache->root;

    while (cur) {
        if (page_id == cur->page_id) {
            splay(cache, cur);
            return cur;
        }

        cur = (page_id < cur->page_id) ? cur->left : cur->right;
    }

    return NULL;
}

static SplayNode *insert_node(SplayCache *cache, uint64_t page_id) {
    SplayNode *parent = NULL;
    SplayNode *cur = cache->root;

    while (cur) {
        parent = cur;

        if (page_id < cur->page_id)
            cur = cur->left;
        else if (page_id > cur->page_id)
            cur = cur->right;
        else {
            splay(cache, cur);
            return cur;
        }
    }

    SplayNode *node = calloc(1, sizeof(SplayNode));
    node->page_id = page_id;
    node->parent = parent;

    if (!parent) {
        cache->root = node;
    } else if (page_id < parent->page_id) {
        parent->left = node;
    } else {
        parent->right = node;
    }

    splay(cache, node);
    cache->root = node;

    cache->size++;
    return node;
}