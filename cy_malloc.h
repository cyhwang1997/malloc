#ifndef CY_MALLOC_H
#define CY_MALLOC_H


void init_memory_allocator(uint32_t start_addr, uint32_t end_addr, uint32_t requested_size);
void *cy_malloc(size_t n);
void cy_free(void *p);
void *palloc_get_page(size_t page_cnt);
void palloc_free_page(void *pages, size_t page_cnt);

#endif
