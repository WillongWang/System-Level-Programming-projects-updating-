#include "memlib.h"
#include "malloc.h"

extern stats_t *stats;

#define SIZE 100

void recurse(int level, int *mem)
{
	int *my_data; // 递归每个不一样
	int i;
	if(level < 400) {
		my_data = gc_malloc(SIZE*sizeof(int));
		*mem += SIZE*sizeof(int);
		for(i=0; i<SIZE;i++) {
			my_data[i] = level*i;
		}
		recurse(level+1, mem);
		for(i=0; i<SIZE;i++) {
			if(my_data[i] != level*i) {
				fprintf(stderr,"Correctness test failed, data corruption!\n");
				exit(1);
			}
		}
	} else {
		for(i=0; i<30000; i++) {
			my_data = gc_malloc(SIZE*sizeof(int));
		}
		*mem += 2*SIZE*sizeof(int); // 运行line 27,上一次的my_data所指空间还在,此时的my_data只是变了个地址
	}

	
}

void test4() {
/* Recursion, valid pointers throughout the stack */
  int mem = 0;
  fprintf(stderr,"Running test 4 ...\n");

  recurse(0, &mem);
  stats->max_total_size = mem;
}
