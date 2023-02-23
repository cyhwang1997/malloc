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
	void *base;								/* Base of pool. */
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
static struct desc requested_descs[100];	/* Descriptor for frequently requested size. */
static size_t requested_desc_cnt; /* Number of requested_size descriptors. */

static struct arena *block_to_arena (struct block *);
static struct block *arena_to_block (struct arena *, size_t idx);

static void init_pool(struct pool *p, void *base, size_t page_cnt);

/* The start address, end address of the memory pool is given.
   The size that will be frequently requested is given as requested_size.
   This will be treated by the requested_desc descriptor.

   Divides the given address area to pages with PGSIZE. 
	 With the calculated free pages, it initializes the memory pool.*/
void init_memory_allocator(uint32_t start_addr, uint32_t end_addr)
{
	/* Error handling. */
	if (start_addr >= end_addr) {
    printf("[ERROR] start_addr >= end_addr");
    return;
  }

	/* Calculates number of free pages and initializes pool. */
	size_t free_pages = (end_addr - start_addr) / PGSIZE;
	init_pool(&mem_pool, ((void *)start_addr), free_pages);


	/* Initializes malloc() descriptors. */
	size_t block_size;
	for (block_size = 16; block_size < PGSIZE / 2; block_size *= 2) {
	  struct desc *d = &descs[desc_cnt++];
	  assert(desc_cnt <= sizeof descs / sizeof *descs);
	  d->block_size = block_size;
	  d->blocks_per_arena = (PGSIZE - sizeof (struct arena)) / block_size;
	  list_init(&d->free_list);
	}
}

void cy_requested_size_initiator(size_t requested_size)
{
  struct desc *d;

  if (requested_size == 0) {
    printf("[ERROR] requested_size is zero.\n");
    return;
  }

  for (d = descs; d < descs + desc_cnt; d++) {
    if (d->block_size == requested_size) {
      printf("[ERROR] requested_size is already defined in descriptor.\n");
      return;
    }
  }

  if (requested_size >= PGSIZE/2) {
    printf("[ERROR] requested_size %d is equal or bigger than PGSIZE/2. PGSIZE: %d\n", requested_size, PGSIZE);
    return;
  }

  else if (requested_size < PGSIZE/2) {
    struct desc *d = &requested_descs[requested_desc_cnt++];
    d->block_size = (size_t) requested_size;
    d->blocks_per_arena = (PGSIZE - sizeof (struct arena)) / requested_size;
    list_init(&d->free_list);
  }
}

/* Obtains and returns a new block of at least n bytes. 
   Returns a null pointer if memory is not available. */
void *cy_malloc(size_t n) 
{
  struct desc *d;
  struct block *b;
  struct arena *a;
  bool requested_desc_allocated = false;
  /* A null pointer satisfies a request for 0 bytes. */
  if (n == 0)
    return NULL;
	
  /* If the size is in the requested_size descriptor.*/
  for (d = requested_descs; d < requested_descs + requested_desc_cnt; d++) {
    if (d->block_size == n) {
      requested_desc_allocated = true;
      break;
    }
  }

  if(requested_desc_allocated)
    printf("[CYDBG] Requested_desc is allocated for size %d\n", n);

  /* Find the smallest descriptor that satisfies a SIZE-byte
     request. */
 if (!requested_desc_allocated) {
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
    a = (struct arena*)palloc_get_page(page_cnt); 
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
	  a = (struct arena *)palloc_get_page(1);
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

/* Frees block p, which must have been previously allocated with malloc(). */
void cy_free(void *p)
{
  /* Error handling */
  if (p == NULL) {
    printf("[ERROR] Failed to free %p\n", p);
    return;
  }

  struct block *b = (struct block *)p;
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

/* Obtains and returns a group of page_cnt contiguous free pages.
   If too few pages are available, returns a null pointer. */
void *palloc_get_page(size_t page_cnt)
{
  struct pool *pool = &mem_pool;
  void *pages;
  size_t page_idx;

  if (page_cnt == 0)
  	return NULL;

  page_idx = bitmap_scan_and_flip(pool->used_map, 0, page_cnt, false);

  if (page_idx != BITMAP_ERROR) {
    pages = pool->base + (PGSIZE * page_idx);
  }
  else {
    pages = NULL;
  }

  return pages;
}

/* Frees the page_cnt pages starting at pages. */
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
  struct arena *a = (struct arena *)pg_round_down(b);

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
