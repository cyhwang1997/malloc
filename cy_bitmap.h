#ifndef CY_BITMAP_H
#define CY_BITMAP_H

#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

struct bitmap *bitmap_create_in_buf(size_t bit_cnt, void *block, size_t block_size);
size_t bitmap_buf_size(size_t bit_cnt);

size_t bitmap_size(const struct bitmap *b);

void bitmap_set(struct bitmap *b, size_t idx, bool value);
void bitmap_mark(struct bitmap *b, size_t bit_idx);
void bitmap_reset(struct bitmap *b, size_t bit_idx);
bool bitmap_test(const struct bitmap *b, size_t idx);

void bitmap_set_all(struct bitmap *b, bool value);
void bitmap_set_multiple(struct bitmap *b, size_t start, size_t cnt, bool value);
bool bitmap_contains(const struct bitmap *b, size_t start, size_t cnt, bool value);
bool bitmap_all(const struct bitmap *b, size_t start, size_t cnt);

#define BITMAP_ERROR SIZE_MAX
size_t bitmap_scan(const struct bitmap *b, size_t start, size_t cnt, bool value);
size_t bitmap_scan_and_flip(struct bitmap *b, size_t start, size_t cnt, bool value);

#endif
