#include "metrics.h"

void metrics_write_header(FILE* csv) {
    fprintf(csv,
            "politica,carga,threads,acessos,hits,misses,hit_ratio,evictions,dirty_writes,tempo,avg_depth\n");
}

void metrics_write_result(FILE* csv, ResultadoBenchmark r) {
    fprintf(csv,
            "%s,%s,%d,%d,%zu,%zu,%.2f,%zu,%zu,%.6f,%.2f\n",
            r.politica,
            r.carga,
            r.threads,
            r.acessos,
            r.hits,
            r.misses,
            r.hit_ratio,
            r.evictions,
            r.dirty_writes,
            r.tempo,
            r.avg_depth);
}