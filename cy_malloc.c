#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "cy_malloc.h"
#include "cy_list.h"
#include "round.h"
#include "cy_bitmap.h"
#include "cy_vaddr.h"

/* A memory pool. */
struct pool
{
	struct bitmap *used_map;			/* Bitmap of free pages. */
	uint32_t *base;								/* Base of pool. */
};

static struct pool mem_pool;

/* Descriptor */
struct desc
{
    size_t block_size;          /* Size of each element in bytes */
    size_t blocks_per_arena;    /* Number of blocks in an arena */
    struct list free_list;      /* List of free blocks */
};

/* Magic number for detecting arena corruption. */
#define ARENA_MAGIC 0x9a548eed

/* Arena. */
struct arena 
{
    unsigned magic;             /* Always set to ARENA_MAGIC. */
    struct desc *desc;          /* Owning descriptor, NULL for big block. */
    size_t free_cnt;            /* Free blocks; pages in big block. */
};

/* Free block. */
struct block 
{
    struct list_elem free_elem; /* Free list element. */
};

/* Our set of descriptors. */
static struct desc descs[100];   /* Descriptors. */
static size_t desc_cnt;         /* Number of descriptors. */
static struct desc *requested_desc;	/* Descriptor for frequently requested size. */

static struct arena *block_to_arena (struct block *);
static struct block *arena_to_block (struct arena *, size_t idx);

static void init_pool(struct pool *p, void *base, size_t page_cnt);

/* Divides the given address area to pages with PGSIZE. 
	 With the calculated free pages, it initializes memory pool.*/
void init_memory_allocator(uint32_t start_addr, uint32_t end_addr, uint32_t requested_size)
{
	/* Error handling. */
	if (start_addr >= end_addr) {
    printf("[ERROR] start_addr >= end_addr");
    return;
  }

	/* Calculates number of free pages and initializes pool. */
	size_t free_pages = (end_addr - start_addr) / PGSIZE;
	init_pool(&mem_pool, (void *)start_addr, free_pages);


	/* Initializes malloc() descriptors. */
	size_t block_size;
	bool requested_size_in_desc = false;
	for (block_size = 16; block_size < PGSIZE / 2; block_size *= 2) {
	  if (block_size == requested_size)
	    requested_size_in_desc = true;
	  struct desc *d = &descs[desc_cnt++];
	  assert(desc_cnt <= sizeof descs / sizeof *descs);
	  d->block_size = block_size;
	  d->blocks_per_arena = (PGSIZE - sizeof (struct arena)) / block_size;
	  list_init(&d->free_list);
	}

	/* Initializes descriptor for requested_size. */
	if (requested_size == 0 || requested_size_in_desc)
	  return;
	
	/* block size smaller than PGSIZE/2, and the size is not in desc */
	if (requested_size < PGSIZE/2) {
	  struct desc *d = requested_desc;
	  d->block_size = requested_size;
	  d->blocks_per_arena = (PGSIZE - sizeof (struct arena)) / requested_size;
	  list_init(&d->free_list);
	}
}

void *cy_malloc(size_t n) 
{
  struct desc *d;
  struct block *b;
  struct arena *a;

  /* A null pointer satisfies a request for 0 bytes. */
  if (n == 0)
    return NULL;
	
  if (n == requested_desc->block_size)
    d = requested_desc;

  /* Find the smallest descriptor that satisfies a SIZE-byte
     request. */
  else {
    for (d = descs; d < descs + desc_cnt; d++)
      if (d->block_size >= n)
        break;
  }
	
  /* Big Block */
  if (d == descs + desc_cnt) 
  {
    /* SIZE is too big for any descriptor.
       Allocate enough pages to hold SIZE plus an arena.*/
    size_t page_cnt = DIV_ROUND_UP(n + sizeof *a, PGSIZE);
    a = palloc_get_page(page_cnt); 
    if (a == NULL)
      return NULL;

    /* Initialize the arena to indicate a big block of PAGE_CNT
       pages, and return it.*/
    a->magic = ARENA_MAGIC;
    a->desc = NULL;
    a->free_cnt = page_cnt;
    return a + 1;
  }

  /* If the free list is empty, create a new area */
  if (list_empty(&d->free_list))
  {
    size_t i;

  	/* Allocate a page. */
	  a = palloc_get_page(1);
    if (a == NULL)
      return NULL; 

    /* Initialize arena and add its blocks to the free list. */
    a->magic = ARENA_MAGIC;
    a->desc = d;
    a->free_cnt = d->blocks_per_arena;
    for (i = 0; i < d->blocks_per_arena; i++) {
      struct block *b = arena_to_block (a, i);
      list_push_back(&d->free_list, &b->free_elem);
    }
  }

  /* Get a block from free list and return it. */
  b = list_entry(list_pop_front (&d->free_list), struct block, free_elem);
  a = block_to_arena(b);
  a->free_cnt--;
  return b;
}    

void cy_free(void *p)
{
  /* Error handling */
  if (p == NULL) {
    printf("[ERROR] Failed to free %p\n", p);
    return;
  }

  struct block *b = p;
  struct arena *a = block_to_arena(b);
  struct desc *d = a->desc;

  /* Normal Block */
  if (d != NULL) {
    /* Add block to free list. */
    list_push_front(&d->free_list, &b->free_elem);

    /* If the arena is now entirely unused, free it. */
    if (++a->free_cnt >= d->blocks_per_arena) {
      size_t i;

      if (a->free_cnt != d->blocks_per_arena)
        return;
      for (i = 0; i < d->blocks_per_arena; i++) {
        struct block *b = arena_to_block (a, i);
        list_remove(&b->free_elem);
      }
      palloc_free_page(a, 1);
    }
  }

  /* Big Block */
  else {
    palloc_free_page(a, a->free_cnt);
    return;
  }
}                        

/* Initializes pool P */
static void init_pool(struct pool *p, void *base, size_t page_cnt)
{
  /* We'll put the pool's used_map at its base. 
	 Calculate the space needed for the bitmap
	 and subtract it from the pool's size. */
  size_t bm_pages = DIV_ROUND_UP(bitmap_buf_size (page_cnt), PGSIZE);
  /* Error handling */
  if (bm_pages > page_cnt) {
  	printf("Not enough memory for bitmap.");
	  return;
  }
  page_cnt -= bm_pages; 
	
  p->used_map = bitmap_create_in_buf(page_cnt, base, bm_pages * PGSIZE);
  p->base = base + bm_pages * PGSIZE;
}

void *palloc_get_page(size_t page_cnt)
{
  struct pool *pool = &mem_pool;
  void *pages;
  size_t page_idx;

  if (page_cnt == 0)
  	return NULL;

  page_idx = bitmap_scan_and_flip(pool->used_map, 0, page_cnt, false);

  if (page_idx != BITMAP_ERROR)
    pages = pool->base + PGSIZE * page_idx;
  else
    pages = NULL;

  return pages;
}

void palloc_free_page(void *pages, size_t page_cnt)
{
  struct pool *pool;
  size_t page_idx;

  assert(pg_ofs(pages) == 0);
  if (pages == NULL || page_cnt == 0)
    return;

	pool = &mem_pool;
  page_idx = pg_no(pages) - pg_no(pool->base);

  assert(bitmap_all (pool->used_map, page_idx, page_cnt));
  bitmap_set_multiple(pool->used_map, page_idx, page_cnt, false);  
}

/* Returns the arena that block B is inside. */
struct arena *block_to_arena(struct block *b)
{
  struct arena *a = pg_round_down(b);

  /* Check that the arena is valid. */
  assert(a != NULL);
  assert(a->magic == ARENA_MAGIC);

  /* Check that the block is properly aligned for the arena.*/
  assert(a->desc == NULL
          || (pg_ofs (b) - sizeof *a) % a->desc->block_size == 0);
  assert(a->desc != NULL || pg_ofs (b) == sizeof *a); 

  return a;
}

/* Returns the (IDX - 1)'th block within arena A. */
static struct block *arena_to_block(struct arena *a, size_t idx) 
{
  assert(a != NULL);
  assert(a->magic == ARENA_MAGIC);
  assert(idx < a->desc->blocks_per_arena); 
  return (struct block *) ((uint8_t *) a
                           + sizeof *a
                           + idx * a->desc->block_size);
}
