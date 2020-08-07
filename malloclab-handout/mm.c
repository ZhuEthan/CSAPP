/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  Blocks are never coalesced or reused.  The size of
 * a block is found at the first aligned word before the block (we need
 * it for realloc).
 *
 * This code is correct and blazingly fast, but very bad usage-wise since
 * it never frees anything.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

#define NEXT_FITx

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


//#define checkheap(lineno) mm_checkheap(lineno)
#define checkheap(lineno)


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define SIZE_PTR(p)  ((size_t*)(((char*)(p)) - SIZE_T_SIZE))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

// Given pointer to 
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// Given block ptr bp, compute address of next and pre blocks
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

static char* heap_listp = 0;

#ifdef NEXT_FIT
static char* rover;
#endif

static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
//static void checkheap(int verbose);
static void checkblock(void *bp, int lineno);

/*
 * mm_init - Called when a new trace starts.
 */
int mm_init(void)
{
	if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1) {
		return -1;
	}
	
	PUT(heap_listp, PACK(0, 1)); // why? 
	PUT(heap_listp+WSIZE, PACK(DSIZE, 1)); // prologue header
	PUT(heap_listp+2*WSIZE, PACK(DSIZE, 1)); // prologue footer
	PUT(heap_listp+3*WSIZE, PACK(0, 1)); // epilogue header

	heap_listp += 2*WSIZE;

	if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
		return -1;
	}
	return 0;
}

// Return pointer referring to new free heap starting point;
static void *extend_heap(size_t words) {
	void* bp;
	size_t size = ((words & 1) ? (words+1) : words) * WSIZE;

	if ((bp = mem_sbrk(size)) == (void*)-1) {
		return NULL;
	}
	
	size_t header = PACK(size, 0);
	PUT(HDRP(bp), header);
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

	return coalesce(bp);
}

static void place(void *bp, size_t asize) {
	size_t o_size = GET_SIZE(HDRP(bp));

	if (o_size-asize >= 2*DSIZE) {
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		PUT(HDRP(NEXT_BLKP(bp)), PACK(o_size-asize, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(o_size-asize, 0));
	} else {
		PUT(HDRP(bp), PACK(o_size, 1));
		PUT(FTRP(bp), PACK(o_size, 1));
	}
}

static void* find_fit(size_t asize) {
	void* cur;

	for (cur = heap_listp; GET_SIZE(HDRP(cur)) != 0; cur = NEXT_BLKP(cur)) {
		size_t hsize = GET_SIZE(HDRP(cur));
		size_t alloc = GET_ALLOC(HDRP(cur));
		if (hsize >= asize && !alloc) {
			return cur;
		}
	}
	
	return NULL;
}


/*
 * malloc - Allocate a block by incrementing the brk pointer.
 *      Always allocate a block whose size is a multiple of the alignment.
 */
void *malloc(size_t size)
{
  /*int newsize = ALIGN(size + SIZE_T_SIZE);
  unsigned char *p = mem_sbrk(newsize);
  //dbg_printf("malloc %u => %p\n", size, p);

  if ((long)p < 0)
    return NULL;
  else {
    p += SIZE_T_SIZE;
    *SIZE_PTR(p) = size;
    return p;
  }*/

	if (heap_listp == 0) {
		mm_init();
	}

	size_t asize;
	void* ptr;
	
	asize = ((size + DSIZE + (DSIZE-1)) / DSIZE) * DSIZE;
	if ((ptr = find_fit(asize)) == NULL) {
		if ((ptr = extend_heap(MAX(asize / WSIZE, CHUNKSIZE / WSIZE))) == NULL) {
			return NULL;
		}
	}
	place(ptr, asize);

	checkheap(__LINE__);
	
	return ptr;
}

static void *coalesce(void* bp) {
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

	size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
	size_t cur_size = GET_SIZE(HDRP(bp));
	size_t prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));

	if (prev_alloc && next_alloc) {
		return bp;
	} else if (prev_alloc && !next_alloc) {
		PUT(HDRP(bp), PACK(cur_size + next_size, 0));
		PUT(FTRP(bp), PACK(cur_size + next_size, 0));
		return bp;
	} else if (!prev_alloc && next_alloc) {
		PUT(HDRP(PREV_BLKP(bp)), PACK(cur_size + prev_size, 0));
		PUT(FTRP(PREV_BLKP(bp)), PACK(cur_size + prev_size, 0));
		return PREV_BLKP(bp);
	} else {
		PUT(HDRP(PREV_BLKP(bp)), PACK(cur_size + prev_size + next_size, 0));
		PUT(FTRP(PREV_BLKP(bp)), PACK(cur_size + prev_size + next_size, 0));
		return PREV_BLKP(bp);
	}
}

/*
 * free - We don't know how to free a block.  So we ignore this call.
 *      Computers have big memories; surely it won't be a problem.
 */
void free(void *ptr) {
	if (ptr == 0)
		return;
	
	/*Get gcc to be quiet */
	size_t size = GET_SIZE(HDRP(ptr));

	if (heap_listp == 0) {
		mm_init();
	}

	PUT(HDRP(ptr), PACK(size, 0));
	PUT(FTRP(ptr), PACK(size, 0));
	coalesce(ptr);
	checkheap(__LINE__);
}

/*
 * realloc - Change the size of the block by mallocing a new block,
 *      copying its data, and freeing the old block.  I'm too lazy
 *      to do better.
 */
void *realloc(void *oldptr, size_t size) {
	size_t oldsize;
	void* newptr;

	if (size == 0) {
		free(oldptr);
		return NULL;
	}
	
	if (oldptr == NULL) {
		return malloc(size);
	}

	newptr = malloc(size);
	if (newptr == NULL) {
		return 0;
	}

	oldsize = GET_SIZE(HDRP(oldptr));
	if (size < oldsize) {
		oldsize = size;
	}
	memcpy(newptr, oldptr, oldsize);

	free(oldptr);

	return newptr;
}

/*
 * calloc - Allocate the block and set it to zero.
 */
void *calloc (size_t nmemb, size_t size)
{
  	size_t bytes = nmemb * size;
  	void *newptr;

  	newptr = malloc(bytes);
	size_t asize = GET_SIZE(HDRP(newptr));

  	memset(newptr, 0, asize);

  	return newptr;
}

void printblock(void *bp) {
	char* header = HDRP(bp);
	size_t size = GET_SIZE(header);
	char* footer = FTRP(bp);

	printf("location: %p | hsize:%d, hallocated:%d |", bp, GET_SIZE(header), GET_ALLOC(header));
	printf(" data:");
	for (unsigned int i = 0; i < (size-8) / WSIZE; i++) {
		printf(" %x |", GET(bp));
		bp = (unsigned int*)bp + 1;
	}
	printf(" | fsize:%d, fallocated:%d\n", GET_SIZE(footer), GET_ALLOC(footer));
}

void checkblock(void* ptr, int lineno) {
	if (GET_SIZE(HDRP(ptr)) != GET_SIZE(FTRP(ptr))) {
		printblock(ptr);
		printf("size is not matched [%d]\n", lineno);
		exit(1);
	}

	checkheap(__LINE__);

	if (GET_SIZE(HDRP(ptr)) < 8)  {
		printblock(ptr);
		printf("size too small [%d]\n", lineno);
		exit(1);
	}
	if ((unsigned long)ptr & 0x7) {
		printblock(ptr);
		printf("alignment is not correct for header [%d]\n", lineno);
		exit(1);
	}
	if ((unsigned long)FTRP(ptr) & 0x7) {
		printblock(ptr);
		printf("alignemtn is not correct for footer address %lx in line [%d]\n", (unsigned long)(FTRP(ptr)+WSIZE), lineno);
		exit(1);
	}
	if (GET_ALLOC(HDRP(ptr)) != GET_ALLOC(FTRP(ptr))) {
		printblock(ptr);
		printf("alloc is not matched for address %lx in line [%d]\n", (unsigned long)ptr, lineno);
		exit(1);
	}
}

/*
 * mm_checkheap - There are no bugs in my code, so I don't need to check,
 *      so nah!
 */
void mm_checkheap(int lineno) {
	/*Get gcc to be quiet. */
	//verbose = verbose;
	// check epilogue and prolague blocks: 
	if (GET(HDRP(heap_listp)) != PACK(DSIZE, 1) || GET(FTRP(heap_listp)) != PACK(DSIZE, 1) ) {
		printf("%d\n", GET(heap_listp + WSIZE));
		printf("prologue error [%d]\n", lineno);
		exit(1);
	}
	
	// check block's address alignment
	int prev_alloc = 0;
	void* prev_ptr = NULL;

	for (void* ptr = heap_listp; GET_SIZE(HDRP(ptr)) != 0; ptr = NEXT_BLKP(ptr)) {
		checkblock(ptr, lineno);

		if (prev_alloc == 0 && GET_ALLOC(HDRP(ptr)) == 0) {
			printblock(ptr);
			printf("two consecutive blocks [%d]\n", lineno);
			exit(1);
		}
		prev_alloc = GET_ALLOC(HDRP(ptr));
		if (prev_ptr != NULL) {
			if (PREV_BLKP(ptr) != prev_ptr) {
				printblock(ptr);
				printf("next and prev pointers are not matched [%d]", lineno);
				exit(1);
			}
		}

		if (ptr < mem_heap_lo() || ptr > mem_heap_hi()) {
			printblock(ptr);
			printf("list pointers points beyond heap limits [%d]\n", lineno);
			exit(1);
		}
		if (ptr == prev_ptr) {
			printblock(ptr);
			printf("pointer doesn't move [%d]\n", lineno);
			exit(1);
		}
		prev_ptr = ptr;
	}

 	
}
