#include "cy_bitmap.h"
#include "cy_malloc.h"
#include <stdio.h>
#include "round.h"
#include <assert.h>

/* Element type. 

   This must be an unsigned integer type at least as wide as int.

   Each bit represents one bit in the bitmap.
   If bit 0 in an element represents bit K in the bitmap,
   then bit 1 in the element represents bit K+1 in the bitmap,
   and so on. */
typedef unsigned long elem_type;

/* Number of bits in an element. */
#define ELEM_BITS (sizeof (elem_type) * 8)

/* From the outside, a bitmap is an array of bits.
   From the inside, it's an array of elem_type (defined above)
   that simulates an array of bits. */
struct bitmap
{
  size_t bit_cnt;   /* Number of bits. */
  elem_type *bits;  /* Elements that represent bits. */
};

/* Returns the index of the element that contains the bit
   numbered BIT_IDX. */
static inline size_t elem_idx(size_t bit_idx) 
{
  return bit_idx / ELEM_BITS;
}

/* Returns an elem_type where only the bit corresponding to
   BIT_IDX is turned on. */
static inline elem_type bit_mask(size_t bit_idx) 
{
  return (elem_type) 1 << (bit_idx % ELEM_BITS);
}

/* Returns the number of elements required for BIT_CNT bits. */
static inline size_t elem_cnt(size_t bit_cnt)
{
  return DIV_ROUND_UP(bit_cnt, ELEM_BITS);
}

/* Returns the number of bytes required for BIT_CNT bits. */
static inline size_t byte_cnt(size_t bit_cnt)
{
  return sizeof (elem_type) * elem_cnt(bit_cnt);
}

/* For debugging, print bitmap*/
/*static inline void bitmap_print(struct bitmap *b);*/

/* Returns a bit mask in which the bits actually used in the last
   element of B's bits are set to 1 and the rest are set to 0. */
/*static inline elem_type last_mask(const struct bitmap *b) 
{
  int last_bits = b->bit_cnt % ELEM_BITS;
  return last_bits ? ((elem_type) 1 << last_bits) - 1 : (elem_type) -1;
}
*/
/* Creation and destruction. */


/* Creates and returns a bitmap with BIT_CNT bits in the
   BLOCK_SIZE bytes of storage preallocated at BLOCK.
   BLOCK_SIZE must be at least bitmap_needed_bytes(BIT_CNT). */
struct bitmap *bitmap_create_in_buf(size_t bit_cnt, void *block, size_t block_size)
{
  struct bitmap *b = block;

  assert(block_size >= bitmap_buf_size(bit_cnt));

  b->bit_cnt = bit_cnt;
  b->bits = (elem_type *) (b + 1);
  bitmap_set_all(b, false);
  return b;
}

/* Returns the number of bytes required to accomodate a bitmap
   with BIT_CNT bits (for use with bitmap_create_in_buf()). */
size_t bitmap_buf_size(size_t bit_cnt)
{
  return sizeof(struct bitmap) + byte_cnt(bit_cnt);
}

/* Returns the number of bits in B. */
size_t bitmap_size(const struct bitmap *b)
{
  return b->bit_cnt;
}

/* Atomically sets the bit numbered IDX in B to VALUE. */
void bitmap_set(struct bitmap *b, size_t idx, bool value) 
{
  assert(b != NULL);
  assert(idx < b->bit_cnt);
  if (value)
    bitmap_mark(b, idx);
  else
    bitmap_reset(b, idx);
}

/* Atomically sets the bit numbered BIT_IDX in B to true. */
void bitmap_mark(struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx(bit_idx);
  elem_type mask = bit_mask(bit_idx);

  b->bits[idx] |= mask;
}

/* Atomically sets the bit numbered BIT_IDX in B to false. */
void bitmap_reset(struct bitmap *b, size_t bit_idx) 
{
  size_t idx = elem_idx(bit_idx);
  elem_type mask = bit_mask(bit_idx);

  b->bits[idx] &= ~mask;
}

/* Returns the value of the bit numbered IDX in B. */
bool bitmap_test(const struct bitmap *b, size_t idx) 
{
  assert(b != NULL);
  assert(idx < b->bit_cnt);
  return (b->bits[elem_idx (idx)] & bit_mask(idx)) != 0;
}

void bitmap_set_all(struct bitmap *b, bool value)
{
  assert(b != NULL);

  bitmap_set_multiple(b, 0, bitmap_size (b), value);
}

void bitmap_set_multiple(struct bitmap *b, size_t start, size_t cnt, bool value)
{
  size_t i;

  assert(b != NULL);
  assert(start <= b->bit_cnt);
  assert(start + cnt <= b->bit_cnt);

  for (i = 0; i < cnt; i++)
    bitmap_set(b, start + i, value);
}

/* Returns true if any bits in B between START and START + CNT,
   exclusive, are set to VALUE, and false otherwise. */
bool bitmap_contains(const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  size_t i;
  
  assert(b != NULL);
  assert(start <= b->bit_cnt);
  assert(start + cnt <= b->bit_cnt);

  for (i = 0; i < cnt; i++)
    if (bitmap_test (b, start + i) == value)
      return true;
  return false;
}

/* Returns true if every bit in B between START and START + CNT,
   exclusive, is set to true, and false otherwise. */
bool bitmap_all(const struct bitmap *b, size_t start, size_t cnt) 
{
  return !bitmap_contains(b, start, cnt, false);
}

/* Finds and returns the starting index of the first group of CNT
   consecutive bits in B at or after START that are all set to
   VALUE.
   If there is no such group, returns BITMAP_ERROR. */
size_t bitmap_scan(const struct bitmap *b, size_t start, size_t cnt, bool value) 
{
  assert (b != NULL);
  assert (start <= b->bit_cnt);

  if (cnt <= b->bit_cnt) {
      size_t last = b->bit_cnt - cnt;
      size_t i;
      for (i = start; i <= last; i++)
        if (!bitmap_contains(b, i, cnt, !value))
          return i; 
  }
  return BITMAP_ERROR;
}


/* Finds the first group of CNT consecutive bits in B at or after
   START that are all set to VALUE, flips them all to !VALUE,
   and returns the index of the first bit in the group.
   If there is no such group, returns BITMAP_ERROR.
   If CNT is zero, returns 0.
   Bits are set atomically, but testing bits is not atomic with
   setting them. */
size_t bitmap_scan_and_flip(struct bitmap *b, size_t start, size_t cnt, bool value)
{
  size_t idx = bitmap_scan(b, start, cnt, value);
  if (idx != BITMAP_ERROR) 
    bitmap_set_multiple(b, idx, cnt, !value);
  return idx;
}

/* Print bitmap. */
/*static inline void bitmap_print(struct bitmap *b)
{
  for (size_t i = 0; i < b->bit_cnt; i++) {
    printf("[CYDBG] bitmap[%zu]: %d\n", i, bitmap_test(b, i) ? 1 : 0);
  }
}*/
