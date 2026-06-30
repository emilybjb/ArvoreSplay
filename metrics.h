#ifndef METRICS_H
#define METRICS_H

#include <stdio.h>

typedef struct {
    const char* politica;
    const char* carga;
    int threads;
    int acessos;
    size_t hits;
    size_t misses;
    double hit_ratio;
    size_t evictions;
    size_t dirty_writes;
    double tempo;
    double avg_depth;
} ResultadoBenchmark;

void metrics_write_header(FILE* csv);
void metrics_write_result(FILE* csv, ResultadoBenchmark r);

#endif