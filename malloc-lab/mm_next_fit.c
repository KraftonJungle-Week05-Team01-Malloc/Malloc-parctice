/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "team05",
    /* First member's full name */
    "Taewook Jeong",
    /* First member's email address */
    "gotlou8317@gmail.com",
    /* Second member's full name (leave blank if none) */
    "Hoyeon Kwak",
    /* Second member's email address (leave blank if none) */
    "h0y301v@gmail.com" };

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constancs and macros */
#define WSIZE 4             /* Word and header/footer size (bytes) */
#define DSIZE 8             /* Double word size (bytes) */
#define CHUNKSIZE (1 << 12) /* Extend heap by this amount (bytes), 1<<12 == 4096(4KB) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))                  // read a word
#define PUT(p, value) (*(unsigned int *)(p) = (value)) // write a word

/* Read the size and allocated fields from address p(header or footer) */
#define GET_SIZE(p) (GET(p) & ~0x7) // read the size of block(~0x7: 111...111000으로, hdr나 ftr에 저장된 word와의 교집합은 block의 size가 된다)
#define GET_ALLOC(p) (GET(p) & 0x1) // read the allocated block(if p is alocated, 1 else 0)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((void *)(bp)-WSIZE)                        // pointer to header of block
#define FTRP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // pointer to footer of block

/* Given block ptr bp, compute address of next and previous blocks */
#define PREV_BLKP(bp) ((void *)(bp)-GET_SIZE((void *)(bp)-DSIZE))   // pointer to previous block
#define NEXT_BLKP(bp) ((void *)(bp)+GET_SIZE((void *)(bp)-WSIZE)) // pointer to next block

/* for heap initializations(mm_init function) */
static void *heap_listp;
static void *freed_last; // for next fit
static void *before; // for next fit
static void *extend_heap(size_t);
static void *coalesce(void *);
static void place(size_t *bp, size_t size);
static void *next_fit(size_t asize);

/*
 * mm_init - initialize the malloc package.
 */

 /*
 mm init: Before calling mm malloc mm realloc or mm free, the application program (i.e., the trace-driven driver program that you will use to evaluate your implementation)
 calls mm init to perform any necessary initializations, such as allocating the initial heap area.
 The return value should be -1 if there was a problem in performing the initialization, 0 otherwise.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    /* An initial free block(4 * WSIZE) */
    PUT(heap_listp, 0);                            // Alignment padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // Prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // Prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     // Epilogue header
    heap_listp += (2 * WSIZE);                     // prologue header bp(none), heap_listp -> specifically, prologue footer pointer

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) // bp: points to last byte of heap(before extended, old mem_brk)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         // Free block header
    PUT(FTRP(bp), PACK(size, 0));         // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // New epilogue header

    /* Coalesce if the previous block was fress */
    return coalesce(bp);
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

 /*
 The mm malloc routine returns a pointer to an allocated block payload of at least size bytes.
 The entire allocated block should lie within the heap region and should not overlap with any other allocated chunk.
 We will comparing your implementation to the version of malloc supplied in the standard C library (libc).
 Since the libc malloc always returns payload pointers that are aligned to 8 bytes,
 your malloc implementation should do likewise and always return 8-byte aligned pointers.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    void *bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead(hdr, ftr) and alignment reqs. */
    asize = ALIGN(size) + 2 * WSIZE;

    /* Search the free list for a fit by best-fit algorithm */
    if ((bp = next_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;

}

static void place(size_t *bp, size_t asize)
{

    size_t nsize = GET_SIZE(HDRP(bp)) - asize; // Size of splitted free block(remainder)
    PUT(HDRP(bp), PACK(asize, 1));             // Place header and footer of new allocated block
    PUT(FTRP(bp), PACK(asize, 1));

    if (0 < nsize) // Free remaining block by setting Align value to 0
    {
        PUT(HDRP(NEXT_BLKP(bp)), PACK(nsize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(nsize, 0));
    }
}

static void *next_fit(size_t asize) // -> and |->  
{
    void *bp = heap_listp + 2 * WSIZE;
    void *temp = before;
    if (before != NULL)
    {
        bp = before;
    }
    while (GET(HDRP(bp)) != 0x1) // while before meeting epilogue header
    {
        if (GET_ALLOC(HDRP(bp)) == 0 && asize <= GET_SIZE(HDRP(bp))) // 
        {
            before = bp;
            return bp;
        }
        bp = NEXT_BLKP(bp);
    }
    bp = heap_listp + 2 * WSIZE; // if there is no free block before epilogue header, meet this line and starts to find more
    while (bp < temp) //  from the beginning of the heap to temp(what was 'before' variable before), so that we search in whole cycle
    {
        if (GET_ALLOC(HDRP(bp)) == 0 && asize <= GET_SIZE(HDRP(bp)))
        {
            before = bp;
            return bp;
        }
        bp = NEXT_BLKP(bp);
    }
    before = NULL; // if there is no free block, make before NULL, because if we let extend_heap solve it and call next-fit function again, we have to starts from the beginning
    return NULL;
}



// static void *next_fit(size_t asize)  // -> and <-
// {
//     void *bp = heap_listp + 2 * WSIZE;
//     void *temp = before;
//     if (before != NULL)
//     {
//         bp = before;
//     }
//     while (GET(HDRP(bp)) != 0x1) // while before meeting epilogue header
//     {
//         if (GET_ALLOC(HDRP(bp)) == 0 && asize <= GET_SIZE(HDRP(bp))) // 
//         {
//             before = bp;
//             return bp;
//         }
//         bp = NEXT_BLKP(bp);
//     }
//     bp = temp; // if there is no free block before epilogue header, meet this line and starts to find more
//     while (heap_listp + 2 * WSIZE <= bp) //  
//     {
//         if (GET_ALLOC(HDRP(bp)) == 0 && asize <= GET_SIZE(HDRP(bp)))
//         {
//             before = bp;
//             return bp;
//         }
//         bp = PREV_BLKP(bp);
//     }
//     before = NULL; // if there is no free block, make before NULL, because if we let extend_heap solve it and call next-fit function again, we have to starts from the beginning
//     return NULL;
// }

/*
 * mm_free - Freeing a block does nothing.
 */

 /*
 The mm free routine frees the block pointed to by ptr. It returns nothing.
 This routine is only guaranteed to work when the passed pointer (ptr) was returned
 by an earlier call to mm malloc or mm realloc and has not yet been freed.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    freed_last = coalesce(bp); // memorize last freed block pointer for later use at Next-Fit


    if (freed_last < before && before <= FTRP(freed_last))
    {                          // when freed-and-coalesced block contains 'memorized before block' on Next-Fit function.
        before = freed_last;   // Don't need to compare with Header of the block, just compare with pointer. but it needs to be compared with Footer pointer
    }

}

static void *coalesce(void *bp)
{

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) /* Case 1 */
    {
        //do nothing
    }
    else if (prev_alloc && !next_alloc) /* Case 2 */
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) /* Case 3 */
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else /* Case 4 */
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));

        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *oldptr, size_t newsize)
{
    if (oldptr == NULL)
        return mm_malloc(newsize);

    if (newsize == 0)
    {
        mm_free(oldptr);
        return NULL;
    }

    newsize = ALIGN(newsize);                    // Size of payload(aligned)
    size_t oldsize_blk = GET_SIZE(HDRP(oldptr)); // Size of block(already-aligned)
    size_t oldsize = oldsize_blk - 2 * WSIZE;    // Size of payload(already-aligned)
    size_t newsize_blk = newsize + 2 * WSIZE;    // Size of block(already-aligned)

    if (newsize == oldsize) // Case 1
        return oldptr;
    else if (newsize < oldsize) // Case 2
    {
        place(oldptr, newsize_blk);  // since case 1 and 2 just shrinks block itself, we don't have to concider
        coalesce(NEXT_BLKP(oldptr)); // If next to the remaining block is free, two blocks should be coalesced.
        return oldptr;
    }
    else if (GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) == 0) // Case 3. newsize > oldsize
    {
        size_t nextsize_blk = GET_SIZE(HDRP(NEXT_BLKP(oldptr)));
        if (newsize_blk <= oldsize_blk + nextsize_blk)
        {
            // PUT(FTRP(oldptr), PACK(0, 0)); // is there (little) difference in secs?? -> No. 
            PUT(HDRP(oldptr), PACK(oldsize_blk + nextsize_blk, 0)); // For using new(old+next) block size at place function
            place(oldptr, newsize_blk);
            return oldptr;
        }
    }

    /* When unable to find free space at block itself or next block, using mm_malloc and mm_free function */
    void *newptr = mm_malloc(newsize);
    if (newptr == NULL) // if mmaloc gives info that there's no free space, return NULL
        return NULL;
    memcpy(newptr, oldptr, oldsize);
    mm_free(oldptr);
    return newptr;
}
