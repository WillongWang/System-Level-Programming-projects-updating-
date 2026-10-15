#include "memlib.h"
#include "malloc.h"

extern stats_t *stats;

typedef struct test1_list {
  int value;
  int filler[10240];
  struct test1_list *next;
} test1_list_t;

/* simple linked list test. Fills the heap with valid data and makes sure
 * nothing is garbage collected. Need to correctly find pointers within the
 * heap allocated space itself
 */
void test1() {
  int i = 0;
  int mem = 0;
  int max_mem = 0;
  test1_list_t *head;
  test1_list_t *new;

  fprintf(stderr,"Running test 1 ...\n");
  head = (test1_list_t *)gc_malloc(sizeof(test1_list_t));

  while(1) {
    new = (test1_list_t *)gc_malloc(sizeof(test1_list_t));
    if(new==NULL) {
      break;
    }
    new->next = head;
    new->value = i;
    head = new;
    i++;
    mem += sizeof(test1_list_t);
    if(mem > max_mem) max_mem = mem;
  }

  /* We have now filled all of the heap. Garbage collection must have 
   * been performed, but nothing should have been collected
   */


  while(i > 0 && head) {
    if(head->value != (i-1)) {
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
