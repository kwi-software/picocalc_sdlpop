#pragma once
#include <stddef.h>
void *game_malloc(size_t n);
void *game_calloc(size_t n, size_t size);
void game_free(void *p);
size_t game_heap_used(void);
size_t game_heap_peak(void);
#define malloc game_malloc
#define calloc game_calloc
#define free game_free
