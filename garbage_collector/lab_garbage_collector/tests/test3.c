#include "memlib.h"
#include "malloc.h"

extern stats_t *stats;

typedef struct test3_list {
  int value;
  int filler[10240];
  int *val_ptr;
  struct test3_list *next;
} test3_list_t;

/* Pointers to middle of allocated block, not just the start */

void test3() {
  int i = 0;
  int mem = 0;
  int max_mem = 0;
  test3_list_t *head;
  test3_list_t *new;

  fprintf(stderr,"Running test 3 ...\n");
  head = (test3_list_t *)gc_malloc(sizeof(test3_list_t));
  mem = sizeof(test3_list_t);

  while(1) {
    new = (test3_list_t *)gc_malloc(sizeof(test3_list_t));
    if(new==NULL) {
      break;
    }
    new->next = head;
    new->value = i;
    head = new;
    
    new = (test3_list_t *)gc_malloc(sizeof(test3_list_t));
    if(new==NULL) {
      /* most recent addition to the list will have no val_ptr, so 
       * take it off the list
       */
      head->val_ptr = NULL;
      head = head->next; //垃圾
      mem += sizeof(test3_list_t);
      if(mem > max_mem) max_mem = mem;
      break;
    }
    head->val_ptr = &(new->filler[i]);
    *(head->val_ptr) = (1000+i)*2; 
    i++;
    mem += 2*sizeof(test3_list_t);
    if(mem > max_mem) max_mem = mem;
  }

  /* So far, nothing should be garbage collected */
  if(mem_usage() < max_mem) {
    fprintf(stderr,"Correctness test failed, valid blocks have been");
    fprintf(stderr,"collected. Checking data integrity... \n");
  }

  new = head; /* walk the list with `new`: `head` must stay at the front, the steps below start from it */
  while(i > 0) {
    if( (new->value != (i-1)) || (*(new->val_ptr) != (1000+i-1)*2)) {
      fprintf(stderr,"Correctness test failed. Data corruption.\n");
      exit(1);
    }
    new = new->next;
    i--;
  }

  /* now delete some nodes. Each node deleted makes two collectable */
  new = head;
  while(new && new->next) {
    new->next = new->next->next;
    new = new->next; //跳过丢掉两个
    i += 2;
    mem -= 2*sizeof(test3_list_t);
  }

  /* should now be able to allocate "i" nodes again */
  while(i > 1) {
    new = (test3_list_t *)gc_malloc(sizeof(test3_list_t));
    if(new == NULL) {   /* 回收器少回收了块：明确报错，不要空指针崩溃 */
      fprintf(stderr, "Correctness test failed. Could not re-allocate freed nodes.\n");
      exit(1);
    }
    new->next = head;
    head = new;
    i--;
  }

  stats->max_total_size = max_mem;

}
