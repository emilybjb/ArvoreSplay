#include "splay_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

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

    pthread_mutex_t mutex;
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
     if (!filename || capacity == 0) return NULL;

    SplayCache *sc = calloc(1, sizeof(SplayCache));

    if (sc == NULL)
        return NULL;

    sc->file = fopen(filename, "r+b");

    if (!sc->file)
        sc->file = fopen(filename, "w+b");

    if (!sc->file) {
        free(sc);
        return NULL;
    }

    sc->capacity = capacity;
    sc->size = 0;
    sc->root = NULL;

    pthread_mutex_init(&sc->mutex, NULL);

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

static int flush_node(SplayCache *cache, SplayNode *node) {
    if (!node->dirty) return 1;
 
    long offset = (long)(node->page_id) * BLOCK_SIZE;
    if (fseek(cache->file, offset, SEEK_SET) != 0) return 0;
    if (fwrite(node->data, 1, BLOCK_SIZE, cache->file) != BLOCK_SIZE) return 0;
 
    node->dirty = 0;
    cache->dirty_writes++;
    return 1;
}

static SplayNode *find_cold_leaf(SplayNode *root) {
    if (!root) return NULL;
 
    int cap = 64;
    SplayNode **stack  = malloc(cap * sizeof(SplayNode *));
    int       *depth_a = malloc(cap * sizeof(int));
 
    if (!stack || !depth_a) {
        free(stack);
        free(depth_a);
        return NULL;
    }
 
    int top = 0;
    SplayNode *best = NULL;
    int best_depth  = -1;
 
    stack[top]   = root;
    depth_a[top++] = 0;
 
    while (top) {
        if (top + 2 > cap) {
            cap *= 2;
            SplayNode **ns = realloc(stack,  cap * sizeof(SplayNode *));
            int       *nd  = realloc(depth_a, cap * sizeof(int));
            if (!ns || !nd) { free(stack); free(depth_a); return best; }
            stack   = ns;
            depth_a = nd;
        }
 
        SplayNode *n = stack[--top];
        int d        = depth_a[top];
 
        if (!n->left && !n->right) {
            if (d > best_depth) { best = n; best_depth = d; }
        }
 
        if (n->left)  { stack[top] = n->left;  depth_a[top++] = d + 1; }
        if (n->right) { stack[top] = n->right; depth_a[top++] = d + 1; }
    }
 
    free(stack);
    free(depth_a);
    return best;
}

static void remove_node(SplayCache *cache, SplayNode *node) {
    if (!node) return;

    splay(cache, node);

    if (!node->left) {
        cache->root = node->right;
        if(cache->root)
            cache->root->parent = NULL;
    } else if (!node->right) {
        cache->root = node->left;
        if(cache->root)
            cache->root->parent = NULL;
    } else {
        SplayNode *right_subtree = node->right;
        cache->root = node->left;
        cache->root->parent = NULL;

        SplayNode *x = cache->root;
        while (x->right) x = x->right;

        splay(cache, x);
        x->right = right_subtree;
        right_subtree->parent = x;
        cache->root = x;
    }

    free(node);
    cache->size--;
}

static void evict_if_needed(SplayCache *cache) {
    while (cache->size >= cache->capacity) {

        SplayNode *victim = find_cold_leaf(cache->root);

        if (!victim)
            return;

        flush_node(cache, victim);

        remove_node(cache, victim);

        cache->evictions++;
    }
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
    if (!node) return NULL;
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

int splay_cache_read(SplayCache *cache, uint64_t page_id, void *buffer) {
    if (!cache || !buffer) return -1;

    pthread_mutex_lock(&cache->mutex);

    cache->total_accesses++;

    SplayNode *node = find_node(cache, page_id);

    if (node) {
        cache->hits++;
        memcpy(buffer, node->data, BLOCK_SIZE);

        pthread_mutex_unlock(&cache->mutex);
        return 1;
    }

    cache->misses++;

    evict_if_needed(cache);

    node = insert_node(cache, page_id);
    if (!node) {
        pthread_mutex_unlock(&cache->mutex);
        return -1;
    }

    long offset = (long)page_id * BLOCK_SIZE;

    if (fseek(cache->file, offset, SEEK_SET) == 0) {
        size_t n = fread(node->data, 1, BLOCK_SIZE, cache->file);

        if (n < BLOCK_SIZE) {
            memset(node->data + n, 0, BLOCK_SIZE - n);
        }
    }

    node->dirty = 0;

    memcpy(buffer, node->data, BLOCK_SIZE);

    pthread_mutex_unlock(&cache->mutex);
    return 0;
}

int splay_cache_write(SplayCache *cache, uint64_t page_id, const void *buffer) {
    if (!cache || !buffer) return -1;

    pthread_mutex_lock(&cache->mutex);

    cache->total_accesses++;

    SplayNode *node = find_node(cache, page_id);

    if (node) {
        cache->hits++;

        memcpy(node->data, buffer, BLOCK_SIZE);
        node->dirty = 1;

        pthread_mutex_unlock(&cache->mutex);
        return 1;
    }

    cache->misses++;

    evict_if_needed(cache);

    node = insert_node(cache, page_id);
    if (!node) {
        pthread_mutex_unlock(&cache->mutex);
        return -1;
    }

    memcpy(node->data, buffer, BLOCK_SIZE);
    node->dirty = 1;

    pthread_mutex_unlock(&cache->mutex);
    return 0;
}

static int calc_depth(SplayNode *root, int d) {
    if (!root) return 0;

    return d +
        calc_depth(root->left, d + 1) +
        calc_depth(root->right, d + 1);
}

double splay_cache_avg_depth(SplayCache *cache) {
    if (!cache) return 0.0;

    pthread_mutex_lock(&cache->mutex);

    double resultado = 0.0;

    if (cache->root && cache->size > 0) {
        resultado = (double)calc_depth(cache->root, 1) / (double)cache->size;
    }

    pthread_mutex_unlock(&cache->mutex);

    return resultado;
}

void splay_cache_print_stats(SplayCache *cache) {
    if (!cache)
        return;

    double hit_rate = 0.0;

    if (cache->total_accesses > 0)
        hit_rate = (100.0 * cache->hits) / cache->total_accesses;

    printf("===== Splay Cache =====\n");
    printf("Pages in cache : %zu/%zu\n",
           cache->size,
           cache->capacity);

    printf("Hits           : %zu\n", cache->hits);
    printf("Misses         : %zu\n", cache->misses);
    printf("Hit rate       : %.2f%%\n", hit_rate);

    printf("Evictions      : %zu\n", cache->evictions);
    printf("Dirty writes   : %zu\n", cache->dirty_writes);

    printf("Average depth  : %.2f\n",
           splay_cache_avg_depth(cache));

    printf("=======================\n");
}

SplayCacheStats splay_cache_get_stats(SplayCache* cache) {
    SplayCacheStats stats;

    stats.hits = 0;
    stats.misses = 0;
    stats.total_accesses = 0;
    stats.evictions = 0;
    stats.dirty_writes = 0;
    stats.avg_depth = 0.0;

    if (!cache) {
        return stats;
    }

    pthread_mutex_lock(&cache->mutex);

    stats.hits = cache->hits;
    stats.misses = cache->misses;
    stats.total_accesses = cache->total_accesses;
    stats.evictions = cache->evictions;
    stats.dirty_writes = cache->dirty_writes;

    if (cache->root && cache->size > 0) {
        stats.avg_depth = (double)calc_depth(cache->root, 1) / (double)cache->size;
    }

    pthread_mutex_unlock(&cache->mutex);

    return stats;
}

static void flush_tree(SplayCache *cache, SplayNode *root) {
    if (!root) return;
    flush_tree(cache, root->left);
    flush_tree(cache, root->right);
    flush_node(cache, root);
}

static void free_tree(SplayNode *root) {
    if (!root) return;

    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

void splay_cache_close(SplayCache *cache) {
    if (!cache)
        return;

    pthread_mutex_lock(&cache->mutex);

    flush_tree(cache, cache->root);

    fflush(cache->file);
    fclose(cache->file);

    free_tree(cache->root);

    pthread_mutex_unlock(&cache->mutex);

    pthread_mutex_destroy(&cache->mutex);

    free(cache);
}