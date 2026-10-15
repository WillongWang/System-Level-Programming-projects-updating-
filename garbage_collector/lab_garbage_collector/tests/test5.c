#include "memlib.h"
#include "malloc.h"

extern stats_t *stats;

/* valid pointers in the global data area, coalescing required */

#define SIZE1 1600
#define SIZE2 3200
#define NITEMS 350

typedef struct test5_list_s {
	int value;
	int *data;
	struct test5_list_s *next;
} test5_list_t;

test5_list_t *roots[5];

void make_list(int which, int *mem)
{
  int i,j,cur_size;

  if(which < 4) cur_size = SIZE1;
  else cur_size = SIZE2;

  for(i=0; i<NITEMS; i++) {
    test5_list_t *new_item = (test5_list_t *)gc_malloc(sizeof(test5_list_t));
    new_item->data = (int *)gc_malloc(cur_size*sizeof(int));
    new_item->value = i;
    for(j=0; j<cur_size; j+=100) {
	new_item->data[j] = i*j+which;
    }
    new_item->next = roots[which];
    roots[which] = new_item;
  }
}

void check_lists()
{
  int i, j, k, cur_size;

  for(k=0; k < 5; k++) {
    test5_list_t *cur_item = roots[k];
    if(cur_item == NULL)
      continue;

    i = NITEMS;
    if(k < 4) cur_size = SIZE1;
    else cur_size = SIZE2;

    while(cur_item != NULL) {
      i--;
      for(j=0; j<cur_size; j+=100) {
        if(cur_item->data[j] != i*j+k) {
          fprintf(stderr,"Correctness test failed, data corruption:\n");
          fprintf(stderr,"\tlist %d, data[%d], expected %d, got %d\n",
	          k, j, i*j+k, cur_item->data[j]);
          exit(1);
        }
      }
      cur_item = cur_item->next;
    }
    if(i != 0) {
      fprintf(stderr,"Correctness test failed, too few items in list.\n");
      exit(1);
    }
  }
}

void test5() {
  int mem = 0, max_mem=0;
  int i;

  fprintf(stderr,"Running test 5 ...\n");  
  for(i = 0; i < 5; i++) {
	roots[i] = NULL;
  }
 
  make_list(0,&mem);
  mem += NITEMS*(SIZE1*sizeof(int)+sizeof(test5_list_t));

  make_list(1,&mem);
  mem += NITEMS*(SIZE1*sizeof(int)+sizeof(test5_list_t));

  make_list(2,&mem);
  mem += NITEMS*(SIZE1*sizeof(int)+sizeof(test5_list_t));

  make_list(3,&mem);
  mem += NITEMS*(SIZE1*sizeof(int)+sizeof(test5_list_t));

  check_lists();

  /* make some room for the next list */
  max_mem = mem;
  roots[0] = NULL;
  roots[3] = NULL;
  mem -= NITEMS*2*(SIZE1*sizeof(int)+sizeof(test5_list_t));
  make_list(4, &mem);
  mem += NITEMS*(SIZE2*sizeof(int) + sizeof(test5_list_t));
  if(mem > max_mem) max_mem = mem;
   
  check_lists();
 
  stats->max_total_size = max_mem;
}
