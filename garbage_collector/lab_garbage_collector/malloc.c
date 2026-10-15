
/*
 *  CS213 - Lab assignment 3
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h> /* GetModuleHandle / NtCurrentTeb: global data area and stack base */

/* Win32: Use PE Const Instead in program environment */
// #include <elf.h>

#include "memlib.h"
#include "malloc.h"
#include "useful.h"

/* Pointer handed out by the most recent gc_malloc call (see mark_addr) */
static void *last_alloc = NULL;

team_t team = {
    /* Team name to be displayed on webpage */
    "",
    /* First member full name */
    "",
    /* First member email address */
    "",
    /* Second member full name (leave blank if none) */
    "",
    /* Second member email address (blank if none) */
    ""};

/*free list是双向链表结构，结点是free block
 * The free list has a ptr to its head at dseg_lo, and the tree of
 * allocated blocks has a ptr to its root stored at dseg_lo + sizeof(long).
 * gc_add_free just dumps blocks onto the free list
 *
 * gc_malloc currently searches the free list; if that fails
 * to get a large enough block, then it resorts to mem_sbrk.
 * You will need to decide when to call garbage_collect.
 *
 * Each used/allocated block have a header that consists of (只有) a Tree structure (地址是block的首地址), which
 * contains the size of the allocated block and ptrs to the headers of
 * other allocated blocks,和对应内存block
 */
/* the struct of the list is just like the struct of the Tree */
/*
  typedef struct tree_node Tree;
  struct tree_node {
    int size;
    Tree * left, * right;
  };the tree of allocated blocks是链表结构,用splay tree实现,结点即一个Tree
*/
typedef struct list_struct
{
  int size;
  struct list_struct *next, *prev;
} list_t; // free list是链表结构，结点是free block（即一个list_t加相应size的内存block），size不含header/tree

/* Initialize the heap by getting 1 page of memory;
 * The free list pointer and allocated tree pointers
 * occupy the first two words of this page. The rest of the page
 * is our first free block. Set the free list pointer to
 * point to it, and set the allocated tree pointer to NULL
 */
int gc_init(void)
{
  list_t *list;

  mem_sbrk(mem_pagesize()); // 4096Bytes  -- 4K 自定义一块[dseg_lo, dseg_hi]堆

  /* initialize freelist ptr ( dseg_lo + 8(2*sizeof(void*)) ); 32位的指针,long,int都是4byte */
  list = (list_t *)(dseg_lo + (sizeof(long) << 1));
  *(list_t **)dseg_lo = list;
  list->next = list->prev = list;
  /* got 1 page, used first 2 words for free list ptr, alloc'd tree ptr, */
  /* of the remaining bytes, need space for a Tree structure */
  /* the max space can allocate to caller should have a Tree struct */
  list->size = mem_pagesize() - (sizeof(ptr_t) << 1) - sizeof(Tree); // sizeof(Tree) == sizeof(list_t)

  /* initialize the allocated tree pointer to NULL since nothing alloc'd yet */
  *(Tree **)(dseg_lo + sizeof(long)) = NULL;
  last_alloc = NULL;
  return 0;
}

/* Remove a block from the list; "addr" is the address where
 * the pointer to the head of the list is stored; "blk"
 * is a pointer to the block to remove from the list.
 */
/*    "addr" is used as a pointer of pointer
 *	  list_t **addr maybe more easily to understand
 */
void gc_remove_free(char *addr, list_t *blk)
{
  if (blk == blk->next)
  {
    *(list_t **)addr = NULL;
    return;
  }

  if (*(list_t **)addr == blk)
  {
    *(list_t **)addr = blk->next;
    if (!blk->next->size)
      assert(0);
  }
  blk->next->prev = blk->prev;
  blk->prev->next = blk->next;
}

/* Add a block to the list; "addr" is the address where
 * the pointer to the head of the list is stored,
 * "blk" is a pointer to the block to add to the list.
 */
/*    "addr" is used as a pointer of pointer
 *	  list_t **addr maybe more easily to understand
 *	  we know if the parameter is a pointer, the original
 *	  pointer will not change, but if it is a pointer of pointer
 *	  it will work by change the "second level" pointer.
 *    blk just added to the first position,is that good?
 */
void gc_add_free(char *addr, list_t *blk)
{
  list_t *freelist, *f;

  /* coalesce: merge blk with every free block physically adjacent to it
   * (block = header + payload, so its end is (char *)b + sizeof(Tree) + b->size) */
  for (f = *(list_t **)addr; f != NULL;)
  {
    if ((char *)f + sizeof(Tree) + f->size == (char *)blk) /* f ends where blk starts */
    {
      gc_remove_free(addr, f);
      f->size += sizeof(Tree) + blk->size;
      blk = f;
      f = *(list_t **)addr; /* merged block may touch another one: rescan */
    }
    else if ((char *)blk + sizeof(Tree) + blk->size == (char *)f) /* blk ends where f starts */
    {
      gc_remove_free(addr, f);
      blk->size += sizeof(Tree) + f->size;
      f = *(list_t **)addr;
    }
    else if ((f = f->next) == *(list_t **)addr) /* next block; back at head = done */
      f = NULL;
  }

  freelist = *(list_t **)addr;

  if (freelist == NULL)
  {
    blk->next = blk->prev = blk;
    /* not "freelist = blk", freelist just a local copy */
    *(list_t **)addr = blk;
    return;
  }
  blk->next = freelist;
  blk->prev = freelist->prev;
  freelist->prev->next = blk;
  freelist->prev = blk;

  /* make this the first thing on the list */
  *(list_t **)addr = blk;
}

/************************************************************/
/* Functions and types that you will find useful for        */
/* garbage collection. Define any additional functions here */
/************************************************************/

typedef struct stack_t
{
  /* You can define the stack data type in any way that you like.
   * You are allowed to use the standard libc malloc/free
   * routines for the stack management, if you wish.
   * DO NOT use gc_malloc!
   */
  // int dummy; // SYNTAX PURPOSE, replace it by your own ones.
  Tree *ptr;
  struct stack_t *next;
} stack_t;

void push_stack(stack_t **stack_top, Tree *ptr)
{
  stack_t *temp = (stack_t *)malloc(sizeof(stack_t));
  temp->ptr = ptr;
  temp->next = *stack_top;
  *stack_top = temp;
}

Tree *pop_stack(stack_t **stack_top)
{
  stack_t *temp;
  Tree *ptr;
  if (*stack_top == NULL)
    return NULL;
  temp = (*stack_top)->next;
  ptr = (*stack_top)->ptr;
  free(*stack_top);
  *stack_top = temp;
  return ptr;
}

/* return a pointer to the address where the global data segment starts,
 * and the length of the segment in bytes (not words!).
 */
/* Win32: Used PE Constant Value instead in program environment */
/*void get_data_area(void **start, int *length)
{

    Elf32_Ehdr *test = (Elf32_Ehdr *)0x8048000;
    Elf32_Phdr *pgm = (Elf32_Phdr *)((void *)test + test->e_phoff);
    Elf32_Half nhdr = test->e_phnum;

    for( ; pgm < (pgm + nhdr); pgm++) {
  if((pgm->p_type == PT_LOAD) && !(pgm->p_flags & PF_X)) {
      /* Loadable program segment AND not executable, must be data * /
      *start = (void *)(pgm->p_vaddr);
      *length = (int)(pgm->p_memsz);
      break;
  }
    }
}*/

/* Traverse the tree of allocated blocks, collecting
 * pointers to unallocated blocks in the stack s
 */
void collect_unmarked(Tree *root, stack_t **s)
{
  /* The tree of allocated blocks can be thousands of nodes deep (blocks are inserted in
   * increasing address order), so walk it with an explicit stack instead of 原recursion,
   * which would overflow the program stack.
   */
  Tree **todo = NULL;
  int n = 0, cap = 0;
  Tree *node;

  if (root)
  {
    cap = 64;
    todo = (Tree **)malloc(cap * sizeof(Tree *));
    todo[n++] = root;
  }
  while (n > 0)
  {
    node = todo[--n];
    if (!(node->size & 0x1)) // unreachble push it for the sweep 见 line 301
      push_stack(s, node);
    node->size &= ~0x1;    /* clear for next time */
    if (n + 2 > cap)
    {
      cap *= 2;
      todo = (Tree **)realloc(todo, cap * sizeof(Tree *)); // 保存原todo
    }
    if (node->right)
      todo[n++] = node->right;
    if (node->left)
      todo[n++] = node->left;
  }
  free(todo);
}

extern void verify_complete(void);
extern void verify_garbage(void *addr);

/* Where 'a' word that looks like a heap pointer was found */
enum { FROM_ROOT, FROM_LOCALS, FROM_BLOCK }; /* 自定义堆包括header和数据区;'a' from 寄存器、全局变量、普通栈帧(非堆里);直接调用gc_malloc的函数局部变量;另一个已分配块的数据区
根据tests文件有规则(还要防止误杀如和pointer相等的整数):
FROM_ROOT和FROM_LOCALS的指针必须正好指向数据区开头；从另一个分配块中找到的指针可以指向目标块中间
*/
/* Mark the allocated block that contains 'a' (if there is one) and queue it
 * on "work", work 是一个“待扫描工作栈”，保存已经确定存活，但其数据区还没有扫描的内存块
 */
static void mark_addr(unsigned long a, stack_t **work, int from)
{
  Tree *node;
  int *w, n;

  if (a < (unsigned long)dseg_lo || a > (unsigned long)dseg_hi)
    return; /* does not point into the heap */
  node = contains((ptr_t)a, (Tree **)(dseg_lo + sizeof(long)));
  if (node == NULL || (node->size & 0x1))
    return; /* not inside an allocated block, or already marked */
  if (from != FROM_BLOCK && a != (unsigned long)(node + 1)) //(unsigned long)(node + 1)数据区起点
    return;
  if (from == FROM_LOCALS && (char *)(node + 1) != (char *)last_alloc)
  {
    /* A debug build leaves dead variables in the caller's frame (e.g. the pointer the
     * program freed last). A block that is still entirely zero (gc_malloc clears its
     * blocks) and is not the one returned last has never been used, so a pointer to it
     * found here is taken to be stale. 见截图
     */
    w = (int *)(node + 1);
    for (n = (node->size & ~0x1) / sizeof(int); n > 0 && *w == 0; n--)
      w++;
    if (n == 0)
      return;
  }
  node->size |= 0x1; //程序中默认按照8字节对齐,所有的size都是8的整数倍,一个node的size最后1个byte的最后2个bit一定为00即用来存放我们的mark,1表示在用
  push_stack(work, node);
}

/* Treat every word in [lo, hi) as a potential heap pointer */
static void mark_range(int *lo, int *hi, stack_t **work, int from)
{
  unsigned long a;
  unsigned long heap_lo = (unsigned long)dseg_lo, heap_hi = (unsigned long)dseg_hi;

  for (; lo < hi; lo++)
  {
    a = (unsigned long)*lo;
    if (a >= heap_lo && a <= heap_hi)
      mark_addr(a, work, from);
  }
}

void garbage_collect(int *regs, int pgm_stack) //gc_malloc没内存分了就收垃圾,在自定义堆里
{
  Tree *alloc_tree;
  Tree *node;
  stack_t *work = NULL;  /* marked blocks whose contents still have to be scanned */
  stack_t *stack = NULL; /* unmarked (garbage) blocks */
  stack_t **stack_top = &stack;
  unsigned long stack_base, top, next, a;
  unsigned long heap_lo = (unsigned long)dseg_lo, heap_hi = (unsigned long)dseg_hi;
  int temp_ptr, from;
  int i;
  int *p;
  char *base;
  IMAGE_NT_HEADERS *nt;
  IMAGE_SECTION_HEADER *sec;

  stack_base = (unsigned long)((NT_TIB *)NtCurrentTeb())->StackBase; //?线程环境块（TEB）的开头就是NT_TIB，NtCurrentTeb() 返回当前线程这块结构的地址，转成 NT_TIB * 后读出 StackBase，x86 上栈从高地址往低地址长，StackBase 是这块栈的最高地址（线程刚启动时的栈顶），StackLimit 才是当前已经提交的最低地址
  top = (unsigned long)pgm_stack; // line 437,见useful.h最后一句,原top是gc_malloc的ebp值,ebp指向的内容是上一个调用函数的ebp
  from = FROM_LOCALS; /* the first segment holds the locals of gc_malloc's caller */
  /* (the old code began with top = *(int *)top,top往大遍历, and so never looked at that segment) */

  /* 1. Mark the blocks that the root set points to: the callee-save registers... */
  mark_range(regs, regs + 3, &work, FROM_ROOT);

/*原代码:
  while (**(int **)top != 0x00000000) // gc_malloc 的 ebp ──> (correctness 的 ebp) ... ──> main 的 ebp -> CRT 启动帧 -> 0
  {
    //基础:一个内存地址为一个字节,sizeof(int)和指针大小为4字节
    for (i = 0; top + 4 * i < *(int *)top; i++) // 区间包括调用gc_malloc的如correctness的ebp,装correctness返回地址和参数的地址,装调用correctness的函数的局部变量的地址,*top;原0xCCCCCCCC是VC6 Debug模式的填充值但不可靠
    {
*/
  while (top < stack_base && (next = *(unsigned long *)top) > top && next <= stack_base)
  {
    for (i = 0; top + 4 * i < next; i++)
    {
      temp_ptr = *(int *)(top + 4 * i); //找可能的堆指针
      if ((unsigned long)temp_ptr >= heap_lo && (unsigned long)temp_ptr <= heap_hi)
        mark_addr((unsigned long)temp_ptr, &work, from);
    }
    top = next;
    from = FROM_ROOT; /* every older frame is a plain root */
  }
  mark_range((int *)top, (int *)stack_base, &work, FROM_ROOT); /* whatever lies above the last frame */

/* ?ppt */
  base = (char *)GetModuleHandle(NULL); //返回当前 EXE 在内存中的起始地址
  nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
  sec = IMAGE_FIRST_SECTION(nt);
  for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++)
  {
    if (!(sec->Characteristics & IMAGE_SCN_MEM_WRITE) || (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE))
      continue;
    for (p = (int *)(base + sec->VirtualAddress);
         p < (int *)(base + sec->VirtualAddress + sec->Misc.VirtualSize); p++)
    {
      a = (unsigned long)*p;
      if (a < heap_lo || a > heap_hi)
        continue;
      if ((char *)p == (char *)&dseg_lo || (char *)p == (char *)&dseg_hi ||
          (char *)p == (char *)&dseg_size || (char *)p == (char *)&last_alloc) //dseg_size来自memlib.h
        continue;
      mark_addr(a, &work, FROM_ROOT);
    }
  }

  /* 2. Everything a marked block points to is reachable too: scan the contents of
   *    every marked block (this marks further blocks and queues them) until done.
   */
  while ((node = pop_stack(&work)) != NULL)
    mark_range((int *)(node + 1), (int *)((char *)(node + 1) + (node->size & ~0x1)), &work, FROM_BLOCK);

  /* 3. Collect pointers to all the unmarked (i.e. unreachable) blocks and clear the marks.
   *    contains() splays the tree, so re-read the root first.
   */
  alloc_tree = *(Tree **)(dseg_lo + sizeof(long));
  collect_unmarked(alloc_tree, stack_top);

  /* 4. Delete each unmarked block from the allocated tree and put it on the free list
   *    (gc_add_free coalesces it with its free neighbours).
   */
  while (*stack_top != NULL)
  {
    temp_ptr = (int)pop_stack(stack_top);
    verify_garbage((char *)temp_ptr + sizeof(Tree));
    *(Tree **)(dseg_lo + sizeof(long)) = delete ((ptr_t)temp_ptr, alloc_tree);
    alloc_tree = *(Tree **)(dseg_lo + sizeof(long));
    gc_add_free((char *)dseg_lo, (list_t *)temp_ptr);
  }
  verify_complete();
}

void *gc_malloc(size_t size)
{
  list_t *list, *newlist;
  char *addr, *test;
  long foo, mask;
  Tree *alloc_tree;
  int registers[3];
  int top, collected = 0;

  /* start by getting the callee-save registers and the top of the program
   * stack at this point. Doing this here means that we don't have to search
   * the stack frame for gc_malloc or anything that it calls when we collect
   * root pointers.
   */
  GET_CALLEE_REGS(registers);
  PGM_STACK_TOP(top);

  /* round up size to a multiple of 8 */
  size = (size ? ((size >> 3) + !!(size & 7)) << 3 : 8); // size >> 3即除以8,size & 7即除以8的余数

  /* search un-coalesced free list */
  addr = dseg_lo;
  /* list is a pointer to current 首element of the freelist*/
  list = *(list_t **)addr;
  if (list == NULL) /* empty free list: collect first, instead of growing the heap right away */
  {
    collected = 1;
    garbage_collect(registers, top);
    list = *(list_t **)addr;
  }
  if (list != NULL)
  {
    do
    {
      if ((unsigned)list->size >= size)
      {
        break;
      }
      list = list->next;
      if (list == *(list_t **)addr)
      {
        list = NULL;
      }
      if (list == NULL && !collected) /* no fit: collect (and coalesce) once, then search again */
      {
        collected = 1;
        garbage_collect(registers, top);
        list = *(list_t **)addr;
      }
    } while (list);
  }

  /* if we couldn't find a fit then try sbrk */
  if (list == NULL)
  {
    mask = mem_pagesize() - 1; // mask 0xFFF, ~mask = 0xFFFFF000
                               /* in case of there is no space for a Tree struct
                                *	size will reduce after calculation processed.
                                */
    size += sizeof(Tree);      // sizeof(Tree) == sizeof(list_t)
    /* ceil( size/pagesize ) * pagesize */
    foo = (size & ~mask) + ((!!(size & mask)) * mem_pagesize());
    size -= sizeof(Tree);
    list = (list_t *)(dseg_hi + 1);
    test = mem_sbrk(foo); // 若成功,mem_sbrk返回(dseg_hi+1)
    if (!test)
      return NULL;
    list->size = foo - sizeof(Tree);
    gc_add_free(dseg_lo, list);
    list = *(list_t **)addr; /* list may have been merged into its neighbour; the merged block is the head */
  }

  /* found a fit */
  if ((unsigned)list->size < size + sizeof(list_t) + sizeof(long))
  { // 当前list所指块按size分配后若还能剩至少一完整块(至少需要sizeof(list_t)和一个可用字sizeof(long))
    /* give them the whole chunk (we need sizeof(list_t) to hold accounting
     * information, want at least one usable word if we're going to
     * keep track of this block!) */
    gc_remove_free(addr, list);

    /* put this block in the allocated tree */
    addr = dseg_lo + sizeof(long);
    alloc_tree = *(Tree **)addr;
    *(Tree **)addr = insert(alloc_tree, (Tree *)list); // list原本是list_t *，list和Tree的结构体格式一样，list中的元素直接对应(Tree *)list的相应元素
    return last_alloc = memset((char *)list + sizeof(Tree), 0, list->size); // 刚好跳过tree header
  }

  /* give them the beginning of the block */
  newlist = (list_t *)(((char *)list) + size + sizeof(Tree)); // sizeof(Tree) == sizeof(list_t)
  newlist->size = list->size - size - sizeof(Tree);
  list->size = size;

  if (list->next == list)
  {
    newlist->next = newlist->prev = newlist;
  }
  else
  {
    newlist->prev = list->prev;
    newlist->next = list->next;
    newlist->prev->next = newlist->next->prev = newlist;
  }
  if (*(list_t **)addr == list)
  {
    *(list_t **)addr = newlist;
  }

  /* put this block in the allocated tree */
  addr = dseg_lo + sizeof(long);
  alloc_tree = *(Tree **)addr;
  *(Tree **)addr = insert(alloc_tree, (Tree *)list);

  return last_alloc = memset((char *)list + sizeof(Tree), 0, list->size);
}
