#include "cache.h"
#include <stdio.h>
#include <string.h>

int main() {
    Cache* cache = cache_open("data.bin", 4);

    char write_buffer[BLOCK_SIZE];
    char read_buffer[BLOCK_SIZE];

    memset(write_buffer, 'A', BLOCK_SIZE);

    cache_write(cache, 0, write_buffer);

    memset(read_buffer, 0, BLOCK_SIZE);

    cache_read(cache, 0, read_buffer);

    printf("Primeiro byte lido: %c\n", read_buffer[0]);

    cache_close(cache);

    return 0;
}