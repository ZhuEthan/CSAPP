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
#include <stdint.h>

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
#define CHUNKSIZE (1 << 9)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc, prev_alloc) ((size) | (alloc) | (prev_alloc << 1))

#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

#define GET_NEXT_POINTER(p) ((void*)((int64_t)(*(unsigned int*)(p) == 0 ? 0 : (*(unsigned int*)(p)+0x800000000))))
#define GET_PREV_POINTER(p) ((void*)((int64_t)(*(unsigned int*)((char*)p + WSIZE) == 0 ? 0 : (*(unsigned int*)((char*)p + WSIZE) + 0x800000000))))
#define PUT_NEXT_POINTER(p, add) (*(unsigned int*)(p) = (uintptr_t)(add == NULL ? 0 : add-0x800000000))
#define PUT_PREV_POINTER(p, add) (*(unsigned int*)((char*)p + WSIZE) = (uintptr_t)(add == NULL ? 0 : add-0x800000000))

// Given pointer to 
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_PREV_ALLOC(p) ((GET(p) & 0x2) >> 1)

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// Given block ptr bp, compute address of next and pre blocks
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

#define NEXT_FREE_BLKP(bp) (GET(bp))

#define SEG_LIMIT 24 

static char* heap_listp = 0;

static void** seg_list = 0;

#ifdef NEXT_FIT
static char* rover;
#endif

static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
static void print_free_list();
static void checkblock(void *bp, int lineno);

static inline int get_seglist_bucket(size_t size) {
	int words = size / WSIZE;
	int i = 0;
	while (words > 1) {
		words >>= 1;
		i += 1;
	}
	return i;
}

static inline void* get_seglist_root(int seglist_bucket) {
	unsigned int offset = *(unsigned int*)((char*)seg_list + WSIZE*seglist_bucket);
	return (void*)(offset == 0 ? 0 : offset + 0x800000000);
}

static inline void set_seglist_root(int id, void* ptr) {
	*(unsigned int*)((char*)seg_list + WSIZE*id) = (uintptr_t)(ptr == NULL ? 0 : ptr-0x800000000);
}


/*
 * mm_init - Called when a new trace starts.
 */
int mm_init(void) {
	heap_listp = 0;
	seg_list = 0;

	if ((seg_list = mem_sbrk(SEG_LIMIT * WSIZE)) == (void*)-1) {
		return -1;
	}
	memset(seg_list, 0, SEG_LIMIT*WSIZE);

	if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1) {
		return -1;
	}
	
	PUT(heap_listp, PACK(0, 1, 1)); // why? 
	PUT(heap_listp+WSIZE, PACK(DSIZE, 1, 1)); // prologue header
	PUT(heap_listp+2*WSIZE, PACK(DSIZE, 1, 1)); // prologue footer
	PUT(heap_listp+3*WSIZE, PACK(0, 1, 1)); // epilogue header

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
	
	size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
	if (prev_alloc) {
		PUT(HDRP(bp), PACK(size, 0, 1));
		PUT(FTRP(bp), PACK(size, 0, 1));
		PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1, 0));
	} else {
		PUT(HDRP(bp), PACK(size, 0, 0));
		PUT(FTRP(bp), PACK(size, 0, 0));
		PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1, 0));
	}
	return coalesce(bp);
}


static void remove_block(void* bp) {
/*
	1. both prev and next are existing. 
	2. prev is NULL and next is existing. 
	3. prev is NULL and next is null. 
	4. prev is existing and next is null. 
*/
	void* prev_node = GET_PREV_POINTER(bp);
	void* next_node = GET_NEXT_POINTER(bp);
	int seglist_bucket = get_seglist_bucket(GET_SIZE(HDRP(bp)));
	//void* seglist_root = get_seglist_root(seglist_bucket);

	if (prev_node == NULL && next_node == NULL) {
		set_seglist_root(seglist_bucket, NULL);
		//seglist_root = NULL;
	} else if (prev_node != NULL && next_node == NULL) {
		PUT_NEXT_POINTER(prev_node, NULL);
	} else if (prev_node == NULL && next_node != NULL) {
		set_seglist_root(seglist_bucket, next_node);
		PUT_PREV_POINTER(next_node, NULL);
	} else {
		PUT_PREV_POINTER(next_node, prev_node);
		PUT_NEXT_POINTER(prev_node, next_node);
	}
}


static void insert_head(void* bp, size_t size) {
/*
	1. seglist_root is NULL
	2. seglist_root points to some blocks. 
*/
	int seg_list_bucket = get_seglist_bucket(size);
	void* seglist_root = get_seglist_root(seg_list_bucket);
	if (seglist_root == NULL) {
		set_seglist_root(seg_list_bucket, bp);
		PUT_NEXT_POINTER(bp, NULL);
		PUT_PREV_POINTER(bp, NULL);
	} else {
		PUT_NEXT_POINTER(bp, seglist_root);
		PUT_PREV_POINTER(seglist_root, bp);
		PUT_PREV_POINTER(bp, NULL);
		set_seglist_root(seg_list_bucket, bp);
	}
}


static void *coalesce(void* bp) {
	size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

	size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
	size_t cur_size = GET_SIZE(HDRP(bp));

	size_t prev_size; 

	if (prev_alloc && next_alloc) {
		insert_head(bp, cur_size);
		checkheap(__LINE__);
		return bp;
	} else if (prev_alloc && !next_alloc) {
		remove_block(NEXT_BLKP(bp));
		size_t asize = cur_size + next_size;
		PUT(HDRP(bp), PACK(asize, 0, 1));
		PUT(FTRP(bp), PACK(asize, 0, 1));
		insert_head(bp, asize);
		checkheap(__LINE__);
		return bp;
	} else if (!prev_alloc && next_alloc) {
		void* prev_bp = PREV_BLKP(bp);
		remove_block(prev_bp);
		prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
		size_t asize = cur_size + prev_size;
		PUT(HDRP(PREV_BLKP(bp)), PACK(asize, 0, 1));
		PUT(FTRP(PREV_BLKP(bp)), PACK(asize, 0, 1));
		insert_head(prev_bp, asize);
		checkheap(__LINE__);
		return PREV_BLKP(bp);
	} else {
		void* prev_bp = PREV_BLKP(bp);
		void* next_bp = NEXT_BLKP(bp);
		remove_block(prev_bp);
		remove_block(next_bp);

		prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
		size_t asize = cur_size + prev_size + next_size;
		PUT(HDRP(PREV_BLKP(bp)), PACK(asize, 0, 1));
		PUT(FTRP(PREV_BLKP(bp)), PACK(asize, 0, 1));
		insert_head(prev_bp, asize);
		checkheap(__LINE__);
		return PREV_BLKP(bp);
	}

}


static void* find_fit(size_t asize) {
	for (int bucket = get_seglist_bucket(asize); bucket < SEG_LIMIT; bucket++) {
		void* root = get_seglist_root(bucket);
		for (void* cur = root; cur != NULL; cur = GET_NEXT_POINTER(cur)) {
			size_t hsize = GET_SIZE(HDRP(cur));
			if (hsize >= asize) {
				return cur;
			}
		}
	}
	
	return NULL;
}


/*
 * malloc - Allocate a block by incrementing the brk pointer.
 *      Always allocate a block whose size is a multiple of the alignment.
 */
void *malloc(size_t size) {
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
	
	size_t words = MAX(4, ((size + WSIZE + (WSIZE-1)) / WSIZE));
	asize = (words & 1 ? (words+1) : words) * WSIZE;

	if ((ptr = find_fit(asize)) == NULL) {
		if ((ptr = extend_heap(MAX(asize / WSIZE, CHUNKSIZE / WSIZE))) == NULL) {
			return NULL;
		}
	}
	place(ptr, asize);
	checkheap(__LINE__);

	return ptr;
}


static void place(void *bp, size_t asize) {
	size_t o_size = GET_SIZE(HDRP(bp));
	remove_block(bp);

	if (o_size-asize >= 4*WSIZE) {
		PUT(HDRP(bp), PACK(asize, 1, 1));
		size_t new_size = o_size-asize;
		PUT(HDRP(NEXT_BLKP(bp)), PACK(new_size, 0, 1));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(new_size, 0, 1));
		insert_head(NEXT_BLKP(bp), new_size);
	} else {
		size_t n_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
		size_t n_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(o_size, 1, 1));
		PUT(HDRP(NEXT_BLKP(bp)), PACK(n_size, n_alloc, 1));
		if (!n_alloc) {
			PUT(FTRP(NEXT_BLKP(bp)), PACK(n_size, n_alloc, 1));
		}
	}
}


/*
 * free - We don't know how to free a block.  So we ignore this call.
 *      Computers have big memories; surely it won't be a problem.
 */
void free(void *ptr) {
	if (ptr == 0)
		return;
	
	size_t size = GET_SIZE(HDRP(ptr));

	if (heap_listp == 0) {
		mm_init();
	}
	
	size_t prev_alloc = GET_PREV_ALLOC(HDRP(ptr));
	size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));

	PUT(HDRP(ptr), PACK(size, 0, prev_alloc));
	PUT(FTRP(ptr), PACK(size, 0, prev_alloc));

	PUT(HDRP(NEXT_BLKP(ptr)), PACK(next_size, next_alloc, 0));
	if (!next_alloc) {
		PUT(FTRP(NEXT_BLKP(ptr)), PACK(next_size, next_alloc, 0));
	}
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
	void* header = HDRP(bp);
	void* footer = FTRP(bp);

	printf("location: %p | hsize:%d, hallocated:%d, pallocated: %d |", bp, GET_SIZE(header), GET_ALLOC(header), GET_PREV_ALLOC(header));
	printf(" data:");
	if (!GET_ALLOC(header)) {
		printf("next: %p, prev: %p", GET_NEXT_POINTER(bp), GET_PREV_POINTER(bp));
	}
	/*size_t words = (size - (GET_ALLOC(header) ? 4 : 8)) / WSIZE;
	for (unsigned int i = 0; i < words; i++) {
		//printf(" %x |", GET(bp));
		bp = (unsigned int*)bp + 1;
	}*/
	if (!GET_ALLOC(header)) { 
		printf(" | fsize:%d, fallocated:%d, pallocated: %d", GET_SIZE(footer), GET_ALLOC(footer), GET_PREV_ALLOC(footer));
	}
	printf("\n");
}


void checkblock(void* ptr, int lineno) {
	if (!GET_ALLOC(HDRP(ptr)) && GET_SIZE(HDRP(ptr)) != GET_SIZE(FTRP(ptr))) {
		printblock(ptr);
		printf("size is not matched [%d]\n", lineno);
		exit(1);
	}

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
	if (!GET_ALLOC(HDRP(ptr)) && (unsigned long)FTRP(ptr) & 0x7) {
		printblock(ptr);
		printf("alignemtn is not correct for footer address %lx in line [%d]\n", (unsigned long)(FTRP(ptr)+WSIZE), lineno);
		exit(1);
	}
	if (!GET_ALLOC(HDRP(ptr)) && GET_ALLOC(HDRP(ptr)) != GET_ALLOC(FTRP(ptr))) {
		printblock(ptr);
		printf("alloc is not matched for address %lx in line [%d]\n", (unsigned long)ptr, lineno);
		exit(1);
	}
	if (!GET_ALLOC(HDRP(ptr))) {
		void* prev_ptr = GET_PREV_POINTER(ptr);
		void* next_ptr = GET_NEXT_POINTER(ptr);
		
		if (prev_ptr != NULL) {
			if (GET_NEXT_POINTER(prev_ptr) != ptr) {
				printf("prev and next are not matched:\n");
				printf("ptr %p | next: %p | prev: %p \n", ptr, next_ptr, prev_ptr);
				printf("ptr->prev:  %p | next: %p | prev: %p \n", prev_ptr, GET_NEXT_POINTER(prev_ptr), GET_PREV_POINTER(prev_ptr));
				exit(1);
			}
		}
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
	if (GET(HDRP(heap_listp)) != PACK(DSIZE, 1, 1) || GET(FTRP(heap_listp)) != PACK(DSIZE, 1, 1)) {
		printf("%d\n", GET(heap_listp + WSIZE));
		printf("prologue error [%d]\n", lineno);
		exit(1);
	}
	
	// check block's address alignment
	int prev_alloc = 1;
	void* prev_ptr = NULL;

	int free_blocks = 0;

	for (void* ptr = heap_listp; GET_SIZE(HDRP(ptr)) != 0; ptr = NEXT_BLKP(ptr)) {
		if (!GET_ALLOC(HDRP(ptr))) {
			free_blocks += 1;
		}

		checkblock(ptr, lineno);

		if (prev_alloc == 0 && GET_ALLOC(HDRP(ptr)) == 0) {
			printf("\nthe prev block: \n");
			printblock(PREV_BLKP(ptr));
			printf("\nthe current block\n");
			printblock(ptr);
			printf("two consecutive blocks [%d]\n", lineno);
			exit(1);
		}
		if (!prev_alloc && prev_ptr != NULL) {
			if (PREV_BLKP(ptr) != prev_ptr) {
				printblock(ptr);
				printf("prev [%p] pointers refers to wrong place [%p] at line [%d]", PREV_BLKP(ptr), prev_ptr, lineno);
				exit(1);
			}
		}

		if (ptr < mem_heap_lo() || ptr > mem_heap_hi()) {
			printblock(ptr);
			printf("list pointers points beyond heap limits [%p] for ptr [%p] at line [%d]\n", mem_heap_hi(), ptr, lineno);
			exit(1);
		}
		if (ptr == prev_ptr) {
			printblock(ptr);
			printf("pointer doesn't move [%d]\n", lineno);
			exit(1);
		}
		prev_ptr = ptr;
		prev_alloc = GET_ALLOC(HDRP(ptr));
	}


	if (free_blocks != 0) {
		printf("not allocated list\n");
		for (void* ptr = heap_listp; GET_SIZE(HDRP(ptr)) != 0; ptr = NEXT_BLKP(ptr)) {
			if (!GET_ALLOC(HDRP(ptr))) {
				printf("%p -> ", ptr);
			}
		}
		print_free_list();
		printf("\nfree blocks number is not matched %d from line %d\n", free_blocks, lineno);
		exit(1);
	}
 	
}

static void print_free_list() {
	printf("\nfree list");
	for (int i = 0; i < SEG_LIMIT; i++) {
		void* cur = *((void**)seg_list+i);
		if (cur != NULL) printf("\nindex %d : \n", i);
		for (; cur != NULL; cur = GET_NEXT_POINTER(cur)) {
			printf("%p -> ", cur);
		}
		if (cur != NULL) printf("\n");
	}
	printf("\n");
}
