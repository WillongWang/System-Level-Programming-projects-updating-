#include "memlib.h"
#include "malloc.h"

extern stats_t *stats;

typedef struct test2_list {
  int value;
  int filler[102400];
  struct test2_list *next;
  struct test2_list *prev;
} test2_list_t;

/* A doubly-linked list. This is a lot like test1, but there is some garbage
 * to find on each iteration. Careful not to get into an infinite loop!
 */
void test2() {
  int i = 0;
  int mem = 0;
  int max_mem = 0;
  test2_list_t *head;
  test2_list_t *new;

  fprintf(stderr,"Running test 2 ...\n");
  head = (test2_list_t *)gc_malloc(sizeof(test2_list_t));
  head->prev = head->next = head;

  while(1) {
    new = (test2_list_t *)gc_malloc(sizeof(test2_list_t));
    new = (test2_list_t *)gc_malloc(sizeof(test2_list_t));
    if(new==NULL) {
      break;
    }
    new->next = head;
    new->prev = head->prev;
    head->prev->next = new;
    new->value = i;
    head = new;
    i++;
    mem += sizeof(test2_list_t);
    if(mem > max_mem) max_mem = mem;
  }

  /* We have now filled all of the heap. Garbage collection must have 
   * been performed
   */

  /* should have successfully allocated at least 20 blocks */
  if(i < 20) {
    fprintf(stderr,"Correctness test failed, only able to allocate %d list items (>20 expected)\n",i);
    exit(1);
  }

  i--; /* no data corresponding to last increment */

  while(i > 0 && head) {
    if(head->value != i) {
      fprintf(stderr,"Correctness test failed. Data corruption.\n");
      exit(1);
    }
    head = head->next;
    i--;
  }

  if(i > 0) {
    fprintf(stderr,"Correctness test failed. List too short!\n");
    exit(1);
  }
  
  stats->max_total_size = max_mem;

}
