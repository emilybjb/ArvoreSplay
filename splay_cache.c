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

int splay_cache_read(SplayCache *cache, uint64_t page_id, char *out) {
    SplayNode *node = find_node(cache, page_id);

    if (node) {
        memcpy(out, node->data, BLOCK_SIZE);
        return 1;
    }

    node = insert_node(cache, page_id);
    memcpy(out, node->data, BLOCK_SIZE);

    return 0;
}

void splay_cache_write(SplayCache *cache, uint64_t page_id, const char *data) {
    SplayNode *node = find_node(cache, page_id);

    if (!node) {
        node = insert_node(cache, page_id);
    }

    memcpy(node->data, data, BLOCK_SIZE);
}

static int calc_depth(SplayNode *root, int d) {
    if (!root) return 0;

    return d +
        calc_depth(root->left, d + 1) +
        calc_depth(root->right, d + 1);
}

double splay_cache_avg_depth(SplayCache *cache) {
    if (!cache->root) return 0.0;

    int total = calc_depth(cache->root, 1);
    return (double)total / cache->size;
}

void splay_cache_print_stats(SplayCache *cache) {
    printf("size=%zu avg_depth=%.2f\n",
           cache->size,
           splay_cache_avg_depth(cache));
}

static SplayNode *find_cold_leaf(SplayNode *root) {
    if (!root) return NULL;

    SplayNode *stack[1024];
    int depth[1024];
    int top = 0;

    SplayNode *best = NULL;
    int best_depth = -1;

    stack[top] = root;
    depth[top++] = 0;

    while (top) {
        SplayNode *n = stack[--top];
        int d = depth[top];

        if (!n->left && !n->right) {
            if (d > best_depth) {
                best = n;
                best_depth = d;
            }
        }

        if (n->left) {
            stack[top] = n->left;
            depth[top++] = d + 1;
        }

        if (n->right) {
            stack[top] = n->right;
            depth[top++] = d + 1;
        }
    }

    return best;
}

static void remove_node(SplayCache *cache, SplayNode *node) {
    if (!node) return;

    splay(cache, node);

    if (!node->left) {
        cache->root = node->right;
    } else {
        SplayNode *x = node->left;
        while (x->right) x = x->right;

        splay(cache, x);
        x->right = node->right;
        cache->root = x;
    }

    free(node);
    cache->size--;
}

static void flush_tree(SplayNode *root) {
    if (!root) return;

    flush_tree(root->left);
    flush_tree(root->right);
}

static void free_tree(SplayNode *root) {
    if (!root) return;

    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

void splay_cache_close(SplayCache *cache) {
    if (!cache) return;

    free_tree(cache->root);
    free(cache);
}