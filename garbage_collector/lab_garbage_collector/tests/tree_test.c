/*
                An implementation of top-down splaying
                    D. Sleator <sleator@cs.cmu.edu>
                             March 1992

  "Splay trees", or "self-adjusting search trees" are a simple and
  efficient data structure for storing an ordered set.  The data
  structure consists of a binary tree, without parent pointers, and no
  additional fields.  It allows searching, insertion, deletion,
  deletemin, deletemax, splitting, joining, and many other operations,
  all with amortized logarithmic performance.  Since the trees adapt to
  the sequence of requests, their performance on real access patterns is
  typically even better.  Splay trees are described in a number of texts
  and papers [1,2,3,4,5].

  The code here is adapted from simple top-down splay, at the bottom of
  page 669 of [3].  It can be obtained via anonymous ftp from
  spade.pc.cs.cmu.edu in directory /usr/sleator/public.

  The chief modification here is that the splay operation works even if the
  item being splayed is not in the tree, and even if the tree root of the
  tree is NULL.  So the line:

                              t = splay(i, t);

  causes it to search for item with key i in the tree rooted at t.  If it's
  there, it is splayed to the root.  If it isn't there, then the node put
  at the root is the last one before NULL that would have been reached in a
  normal binary search for i.  (It's a neighbor of i in the tree.)  This
  allows many other operations to be easily implemented, as shown below.

  [1] "Fundamentals of data structures in C", Horowitz, Sahni,
       and Anderson-Freed, Computer Science Press, pp 542-547.
  [2] "Data Structures and Their Algorithms", Lewis and Denenberg,
       Harper Collins, 1991, pp 243-251.
  [3] "Self-adjusting Binary Search Trees" Sleator and Tarjan,
       JACM Volume 32, No 3, July 1985, pp 652-686.
  [4] "Data Structure and Algorithm Analysis", Mark Weiss,
       Benjamin Cummins, 1992, pp 119-130.
  [5] "Data Structures, Algorithms, and Performance", Derick Wood,
       Addison-Wesley, 1993, pp 367-375.
*/

#include <stdio.h>
#include <stdlib.h>
#include "malloc.h"
#include "memlib.h"


typedef struct ttree_node tTree;
struct ttree_node {
    tTree * left, * right;
    int size;
    int *data;
};

tTree * test_splay (int i, tTree * t) {
/* Simple top down splay, not requiring i to be in the tree t.  */
/* What it does is described above.                             */
    tTree N, *l, *r, *y;
    if (t == NULL) return t;
    N.left = N.right = NULL;
    l = r = &N;

    for (;;) {
        if (i < t->size) {
            if (t->left == NULL) break;
            if (i < t->left->size) {
                y = t->left;                           /* rotate right */
                t->left = y->right;
                y->right = t;
                t = y;
                if (t->left == NULL) break;
            }
            r->left = t;                               /* link right */
            r = t;
            t = t->left;
        } else if (i > t->size) {
            if (t->right == NULL) break;
            if (i > t->right->size) {
                y = t->right;                          /* rotate left */
                t->right = y->left;
                y->left = t;
                t = y;
                if (t->right == NULL) break;
            }
            l->right = t;                              /* link left */
            l = t;
            t = t->right;
        } else {
            break;
        }
    }
    l->right = t->left;                                /* assemble */
    r->left = t->right;
    t->left = N.right;
    t->right = N.left;
    return t;
}


tTree * test_insert(int i, tTree * t, int *mem, int *max_mem) {
/* Insert i into the tree t, unless it's already there.    */
/* Return a pointer to the resulting tree.                 */
    tTree * new;
    
    new = (tTree *) gc_malloc (sizeof (tTree));

    if (new == NULL) {
        printf("Ran out of space\n");
        exit(1);
    }
    
    *mem += sizeof(tTree);
    if(*mem > *max_mem) *max_mem = *mem;

    new->size = i;
    new->data = (int *)gc_malloc(i*sizeof(int));

    if(new->data == NULL) {
      printf("Ran out of space\n");
      exit(1);
    }

    *mem += i*sizeof(int);
    if(*mem > *max_mem) *max_mem = *mem;

    if (t == NULL) {
        new->left = new->right = NULL;
        return new;
    }
    t = test_splay(i,t);
    if (i < t->size) {
        new->left = t->left;
        new->right = t;
        t->left = NULL;
        return new;
    } else if (i > t->size) {
        new->right = t->right;
        new->left = t;
        t->right = NULL;
        return new;
    } else { /* We get here if it's already in the tree */
             /* Don't add it again                      */
      *mem -= i*sizeof(int) + sizeof(tTree);

      return t;
    }
}

void get_size(tTree *root, int *mem) 
{

  if(root != NULL) {
    *mem += root->size + sizeof(tTree);
    get_size(root->left, mem);
    get_size(root->right, mem);
  }
}


tTree * test_delete(int i, tTree * t) {
/* Deletes i from the tree if it's there.               */
/* Return a pointer to the resulting tree.              */
    tTree * x;

    if (t==NULL) return NULL;
    t = test_splay(i,t);
    if (i == t->size) {               /* found it */
        if (t->left == NULL) {
            x = t->right;
        } else {
            x = test_splay(i, t->left);
            x->right = t->right;
        }
        /**mem -= t->size*sizeof(int);*/
        return x;
    }
    else {

      if(i < t->size) {
	/*
	count(t->left, &c);
	*mem -= c*sizeof(int);
	*/
	t->left = NULL;
      } else {
	/*
	c = t->size;
	count(t->left, &c);
	*mem -= c*sizeof(int);
	*/
	t = t->right;
      }
    } 

    return t;                         /* It wasn't there */
}

#define MAX_SIZE 5000

extern stats_t *stats;

void tree_test() {
/* A sample use of these functions.  Start with the empty tree,         */
/* insert some stuff into it, and then delete it                        */
    tTree * root;
    int i, op;
    int cur_size;
    int mem = 0;
    int max_mem = 0;
    root = NULL;              /* the empty tree */

    
    srandom(270572);

    for (i = 0; i < 100; i++) {
        cur_size = ((float)random()/RAND_MAX) * MAX_SIZE;
	/*fprintf(stderr,"inserting %d, mem now %d\n",cur_size, mem);*/
        root = test_insert(cur_size, root, &mem, &max_mem);
	/*fprintf(stderr,"size = %d\n\n",nodes);*/
    }

    fprintf(stderr,"initial insertions completed\n");

    for (i = 0; i < 3000; i++) {
        op = random();
	cur_size = ((float)op/RAND_MAX)*MAX_SIZE;
	if((op & 0x11)==0x11) {
	  root = test_delete(cur_size, root);
	  mem = 0;
	  get_size(root, &mem);
	} else {
	  root = test_insert(cur_size, root, &mem, &max_mem);
	}
    }

    stats->max_total_size = max_mem;
}
