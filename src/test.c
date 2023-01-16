#include <stdio.h>
#include <stdlib.h>
#include "cy_malloc.h"
#include "cy_vaddr.h"


int main (void) {
  printf("test begin\n");

  /*To get the heap area address, do malloc.*/
  uint32_t *a = malloc(PGSIZE*30);
  printf("[CYTEST] a: %p\n", a);
//  free(a);
  printf("[CYTEST] a: %p\n", a);

  /*With ~PGMASK, set the least significant 12 bits to 0.*/
  uint32_t *a_pg = (uint32_t *)((uintptr_t) a & ~PGMASK);
  a_pg += PGSIZE;

  /*Set the pool's size to have 20 pages.
    init_memory_allocator gets the address by uint format.*/
  uint32_t start_addr = (uint32_t) a_pg;
  uint32_t end_addr = start_addr + (uint32_t)PGSIZE*20;

  printf("[CYTEST] start_addr: %x, PGSZ: 0x%x\n", start_addr, PGSIZE);
  printf("[CYTEST] end_addr: %x\n", end_addr);

  printf("\n[CYTEST] --------init_memory_allocator--------\n");
  /*init_memory*/
  init_memory_allocator(start_addr, end_addr);
//  init_memory_allocator(start_addr, end_addr, 16384);

  printf("\n[CYTEST] --------cy_requested_size_initiator--------\n");
  /*[ERROR] Requested_size same as the size in descriptors*/
  cy_requested_size_initiator(32);
  /*[ERROR] Requested size same as the PGSIZE*/
  cy_requested_size_initiator(16384);
  /*Normal requested size*/
  cy_requested_size_initiator(20);

  printf("\n[CYTEST] --------cy_malloc--------\n");
  /*malloc*/
  /*Allocate memory smaller than 16B(the minimum size).*/
  //int *mem10 = cy_malloc(10);
  int *mem10 = cy_malloc(0x100c);
  if (mem10 != NULL)
    printf("[CYTEST] mem10 %p is allocated\n", mem10);
  else
    printf("[CYTEST] mem10 has a NULL pointer.\n");

  /*Allocate memory of 16B. Free list of 16B has been made by cy_malloc(10).*/
  //int *mem16 = cy_malloc(16);
  int *mem16 = cy_malloc(0x814);
  if (mem16 != NULL)
    printf("[CYTEST] mem16 %p is allocated\n", mem16);
  else
    printf("[CYTEST] mem16 has a NULL pointer.\n");

  /*Allocate memory of requested_size.*/  
  int *mem20 = cy_malloc(20);
  if (mem20 != NULL)
    printf("[CYTEST] mem20(requested_size) %p is allocated\n", mem20);
  else
    printf("[CYTEST] mem20 has a NULL pointer.\n");

  /*Allocate memory of different size.*/
  int *mem32 = cy_malloc(32);
  if (mem32 != NULL)
    printf("[CYTEST] mem32 %p is allocated\n", mem32);
  else
    printf("[CYTEST] mem32 has a NULL pointer.\n");

  /*Allocate memory of Big Block.*/
  int *mem5K = cy_malloc(5000);
  if (mem5K != NULL)
    printf("[CYTEST] mem5K %p is allocated\n", mem5K);
  else
    printf("[CYTEST] mem5K has a NULL pointer.\n");


  printf("\n[CYTEST] --------cy_free--------\n");
  /*free*/

  /*mem16 is still using a block in the descriptors for 16B. 
    After freeing mem10 and mem16, the arena(page) is freed.*/
  *mem10 = 4;
  printf("[CYTEST] (before free) mem10: %d\n", *mem10);
  cy_free(mem10);
  printf("[CYTEST] (after free)  mem10: %d\n", *mem10);

  *mem16 = 6;
  printf("[CYTEST] (before free) mem16: %d\n", *mem16);
  cy_free(mem16);
  printf("[CYTEST] (after free)  mem16: %d\n", *mem16);

  *mem20 = 10;
  printf("[CYTEST] (before free) mem20: %d\n", *mem20);
  cy_free(mem20);
  printf("[CYTEST] (after free) mem20: %d\n", *mem20);


  *mem32 = 15;
  printf("[CYTEST] (before free) mem32: %d\n", *mem32);
  cy_free(mem32);
  printf("[CYTEST] (after free) mem32: %d\n", *mem32);

  *mem5K = 5000;
  printf("[CYTEST] (before free) mem5K: %d\n", *mem5K);
  cy_free(mem5K);
  printf("[CYTEST] (after free) mem5K: %d\n", *mem5K);


/*  for (int i = 0; i < 100; i ++){
  	int *mem10 = cy_malloc(10);
  	if (mem10 != NULL)
    		printf("[CYTEST] mem10 %p is allocated\n", mem10);
  	else
    		printf("[CYTEST] mem10 has a NULL pointer.\n");
  	cy_free(mem10);
  }*/
}
