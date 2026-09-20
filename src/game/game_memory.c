#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Core 0 only; coalescing arena, bounded identically on host and Pico. */
/* Leave 4 KiB more static RAM headroom for newer SDK/toolchain builds. */
#define HEAP_BYTES (164u * 1024u)
typedef union Block Block;
union Block {
  struct {
    size_t bytes;
    Block *next;
    unsigned free;
  } h;
  max_align_t align;
};
static union {
  max_align_t align;
  unsigned char bytes[HEAP_BYTES];
} arena;
static Block *first;
static size_t used, peak;
extern void game_fatal(const char *why);
void *game_malloc(size_t n) {
  if (!n)
    n = 1;
  const size_t a = _Alignof(max_align_t);
  if (n > HEAP_BYTES - a)
    game_fatal("GAME ALLOC TOO BIG");
  n = (n + a - 1) & ~(a - 1);
  if (!first) {
    first = (Block *)arena.bytes;
    first->h = (typeof(first->h)){HEAP_BYTES - sizeof(Block), NULL, 1};
  }
  for (Block *b = first; b; b = b->h.next)
    if (b->h.free && b->h.bytes >= n) {
      if (b->h.bytes >= n + sizeof(Block) + a) {
        Block *next = (Block *)((unsigned char *)(b + 1) + n);
        next->h =
            (typeof(next->h)){b->h.bytes - n - sizeof(Block), b->h.next, 1};
        b->h.next = next;
        b->h.bytes = n;
      }
      b->h.free = 0;
      used += b->h.bytes + sizeof(Block);
      if (used > peak)
        peak = used;
      return b + 1;
    }
  game_fatal("GAME HEAP FULL");
  return NULL;
}
void *game_calloc(size_t n, size_t size) {
  if (size && n > HEAP_BYTES / size)
    game_fatal("GAME CALLOC TOO BIG");
  void *p = game_malloc(n * size);
  memset(p, 0, n * size);
  return p;
}
void game_free(void *p) {
  if (!p)
    return;
  if ((uintptr_t)p < (uintptr_t)arena.bytes ||
      (uintptr_t)p >= (uintptr_t)arena.bytes + HEAP_BYTES)
    game_fatal("BAD GAME FREE");
  Block *b = (Block *)p - 1;
  if (b->h.free)
    game_fatal("DOUBLE GAME FREE");
  used -= b->h.bytes + sizeof(Block);
  b->h.free = 1;
  for (b = first; b && b->h.next;) {
    if (b->h.free && b->h.next->h.free) {
      b->h.bytes += sizeof(Block) + b->h.next->h.bytes;
      b->h.next = b->h.next->h.next;
    } else
      b = b->h.next;
  }
}
size_t game_heap_used(void) { return used; }
size_t game_heap_peak(void) { return peak; }
