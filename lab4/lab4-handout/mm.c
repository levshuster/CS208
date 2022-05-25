/*
 * CS 208 Lab 4: Malloc Lab
 *
 * By Sam Johnson-Lacoss and Lev Shuster (johnsonlacosss shusterl)
 *
 * Simple allocator based on implicit free lists, first fit search,
 * and boundary tag coalescing.
 *
 * Each block has header and footer of the form:
 *
 *      63                  4  3  2  1  0
 *      ------------------------------------
 *     | s  s  s  s  ... s  s  0  0  0  a/f |
 *      ------------------------------------
 *
 * where s are the meaningful size bits and a/f is 1
 * if and only if the block is allocated. The list has the following form:
 *
 * begin                                                             end
 * heap                                                             heap
 *  -----------------------------------------------------------------
 * |  pad   | hdr(16:a) | ftr(16:a) | zero or more usr blks | hdr(0:a) |
 *  -----------------------------------------------------------------
 *          |       prologue        |                       | epilogue |
 *          |         block         |                       | block    |
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
 */


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include "mm.h"
#include "memlib.h"

/* Basic constants and macros */
#define WSIZE               8       /* word size (bytes) */
#define DSIZE               16      /* doubleword size (bytes) */
#define CHUNKSIZE           (1<<12) /* initial heap size (bytes) */
#define OVERHEAD            16      /* overhead of header and footer (bytes) */
#define MIN_B_SIZE          32      /* min block size */


/* NOTE: feel free to replace these macros with helper functions and/or
 * add new ones that will be useful for you. Just make sure you think
 * carefully about why these work the way they do
 */

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(size_t *)(p))
#define PUT(p, val)  (*(size_t *)(p) = (val))

/* Perform unscaled pointer arithmetic */
#define PADD(p, val) ((char *)(p) + (val))
#define PSUB(p, val) ((char *)(p) - (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0xf)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       (PSUB(bp, WSIZE))
#define FTRP(bp)       (PADD(bp, GET_SIZE(HDRP(bp)) - DSIZE))

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  (PADD(bp, GET_SIZE(HDRP(bp))))
#define PREV_BLKP(bp)  (PSUB(bp, GET_SIZE((PSUB(bp, DSIZE)))))


/* macros for explicit linked lists */
#define PTR_NEXT(bp)  ((size_t *) bp) // Returns the pointer to the number that functions as the pointer to the bp of the previous value
#define PTR_PREV(bp)  ((size_t *) PADD(bp, WSIZE)) // Returns the pointer to the number that functions as the pointer to the bp of the next value

#define LL_PREV(bp) (*PTR_PREV(bp)) // Returns the pointer to the previous bp
#define LL_NEXT(bp) (*PTR_NEXT(bp)) // Returns the pointer to the next bp

/* Global variables */

// Pointer to first block
static void *heap_start = NULL;

/* Function prototypes for internal helper routines */
static bool check_heap(int lineno);
static void print_heap();
static void print_linked_list();
static void print_block(void *bp);
static void ll_add(void* bp);
static void ll_remove(void* bp);
static bool check_block(int lineno, void *bp);
static void *extend_heap(size_t size);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static size_t max(size_t x, size_t y);

/*
 * For Grading Purposes: Information on the two authors
 * We can be reached at shusterl@carleton.edu
 */
team_t johnsonlacosssshusterl = {
    .teamname = "johnsonlacosssshusterl", /* ID1+ID2 or ID1 */
    .name1 = "Sam Johnson-Lacoss",   /* full name of first member */
    .id1 = "johnsonlacosss",      /* login ID of first member */
    .name2 = "Lev Shuster",    /* full name of second member (if any) */
    .id2 = "shusterl",      /* login ID of second member */
};

/* 
 * mm_init -- This function creates the initial heap (padding, prologue, and epilogue) then expands the heap
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 */
int mm_init(void) {
    /* create the initial empty heap */
    if ((heap_start = mem_sbrk(4 * WSIZE)) == NULL)
        return -1;

    /* linked list start is placed at the padding of the heap; it then points to 0 (empty) */
    PUT(heap_start, (size_t) 0);


    PUT(PADD(heap_start, WSIZE), PACK(OVERHEAD, 1));    /* prologue header */ 
    PUT(PADD(heap_start, DSIZE), PACK(OVERHEAD, 1));    /* prologue footer */
    PUT(PADD(heap_start, WSIZE + DSIZE), PACK(0, 1));   /* epilogue header */

    heap_start = PADD(heap_start, DSIZE); /* start the heap at the (size 0) payload of the prologue block */
    
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

/* UPDATED FOR EXPLICIT LINKED LIST
 * mm_malloc -- <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 */
void *mm_malloc(size_t size) {
    size_t asize;      /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size <= 0){ 
        return NULL;
    }
    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE) {
        asize = DSIZE + OVERHEAD;
    } else {
        /* Add overhead and then round up to nearest multiple of double-word alignment */
        asize = DSIZE * ((size + (OVERHEAD) + (DSIZE - 1)) / DSIZE);
    }

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    /* No fit found. Get more memory and place the block */
    extendsize = max(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL){
        return NULL;
    }

    place(bp, asize);
    return bp;
}

/*
 * mm_free -- free the block whose payload is pointed to by ptr
 * the function takes in a pointer to the block that will be freed
 * VOID - Nothing. This function assumes the payload was previously returned / unwanted
 * PRECONDITION: The payload was returned by an earlier call to mm_malloc
 * PRECONDITION: The payload has not been previously freed since its most recent return
 * from malloc 
 */
void mm_free(void *bp) {
    // Calculate what the header/footer would be unallocated:
    size_t newhead = GET_SIZE(HDRP(bp)); // size_t is meant to functinoally be an unsigned long
    // Make the last bit of the header and footer 0.
    PUT(HDRP(bp), newhead);
    PUT(FTRP(bp), newhead);
    ll_add(bp);
    coalesce(bp);
}

/* The remaining routines are internal helper routines */


/* UPDATED FOR LINKED LIST
 * place -- Place block of asize bytes at start of free block bp and remove block from the explicit free linked list
 *          and <How are you handling splitting?>
 * Takes a pointer to a free block and the size of block to place inside it
 * Returns nothing
 * <Are there any preconditions or postconditions?>
 */
static void place(void *bp, size_t asize) { // 'asize' is just the size of the payload + head/foot + padding O 
    size_t unalloc_size = GET_SIZE(HDRP(bp)) - asize;

    if (((int)unalloc_size - 32) < 0) { // if there is a remainder that doesn't split right
        size_t size = GET_SIZE(HDRP(bp));
        PUT(HDRP(bp), PACK(size, 1)); // PUT size&1 at the header
        PUT(FTRP(bp), PACK(size, 1)); // PUT size&1 at the footer
    } else { // if there is [0] OR [a remainder that splits right] 
         // then there must be a splittable block
        // Put asize at the header and footer
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        // Create a block of unalloc_size bytes
        size_t* next = (size_t*) NEXT_BLKP(bp);
        PUT(HDRP(next), unalloc_size);
        PUT(FTRP(next), unalloc_size);
        ll_add(next);
        }
    ll_remove(bp); // remove bp from the linked list
}


/* ll_add -- add a free block to the linked list
* ARGUMENT: void* bp - pointer to the free block's payload
* Returns nothing
* PRECONDITION: this function assumes that the block *is* free and should thus be allowed into the list
*/
static void ll_add(void* bp){
    size_t* ll_start = (size_t*) PSUB(heap_start, DSIZE);
    size_t* second = (size_t*) LL_NEXT(ll_start);
    if (second != 0) { // if the linked list is not empty:
        /* putting the newly freed block into the linked list */
        PUT(PTR_PREV(second), (size_t) bp); // set previous_ll of first item in linked list to be bp
    }

    PUT(PTR_NEXT(bp), (size_t) second); // set bp ll_next to current first item
    PUT(PTR_PREV(bp), (size_t) 0); // set bp previous to equal 0 cast as something
    PUT(ll_start, (size_t) bp); // set current first item in linked list to be the next value of bp
}

/* ll_remove -- remove a non-free block from the linked list
* ARGUMENT: void* bp - pointer to the block's payload
* Returns nothing
* PRECONDITION: this function assumes that the block *is not* free and should thus be omitted from the list
*/
static void ll_remove(void* bp) {
    size_t* ll_prev = (size_t*) LL_PREV((size_t) bp); //find item before item to be removed
    size_t* ll_next = (size_t*) LL_NEXT((size_t) bp); //find item after to be removed
    size_t* ll_start = (size_t*) PSUB(heap_start, DSIZE);
    /* take it out! */
    if (!ll_prev && !ll_next) { // if the linked list is empty:
        PUT(ll_start, 0); // start means empty now
    } else if (!ll_prev) { // if it's at the beginning:
        PUT(ll_start, (size_t) ll_next); // start now jumps to next block in ll
        PUT((size_t*) PTR_PREV(ll_next), 0); // next item in linked list now jumps backward to 0
    } else if (!ll_next) { // if it's at the end:
        PUT(PTR_NEXT(ll_prev), (size_t) 0);//set next of preivous item to the location of next item
    } else {
        PUT(PTR_NEXT(ll_prev), (size_t) ll_next);//set next of preivous item to the location of next item
        PUT(PTR_PREV(ll_next), (size_t) ll_prev);// set previous of next item to the location of the previous item
    }
}

/*
 * coalesce -- Boundary tag coalescing.
 * Takes a pointer to a free block
 * Return ptr to coalesced block
 * PRECONDITIONS:
 * * bp must point to a block that is already free
 */
static void *coalesce(void *bp) {
    void* prev = PREV_BLKP(bp);
    void* next = NEXT_BLKP(bp);
    void* previous_header = HDRP(prev); // Make a pointer to the previous header
    void* next_header = HDRP(next); // Make a pointer to the next header
    size_t totalsize; // total size, used later

    if (!GET_ALLOC(previous_header) && !GET_ALLOC(next_header)) { // both free
        /* remove middle and next, as we only point to leftmost in ll */
        ll_remove(bp);
        ll_remove(next);
        totalsize = GET_SIZE(previous_header) + GET_SIZE(HDRP(bp)) + GET_SIZE(next_header); // size of: prev + middle + next
        //update header and footer
        PUT(previous_header, totalsize); // Update header
        PUT(FTRP(prev), totalsize); // Update footer
        return prev;

    } else if (!GET_ALLOC(previous_header)) { // left free
        /* we point to only the leftmost, so remove middle */
        ll_remove(bp);

        //update header and footer
        totalsize = GET_SIZE(previous_header) + GET_SIZE(HDRP(bp)); // size of: prev + middle
        PUT(previous_header, totalsize); // Update header
        PUT(FTRP(prev), totalsize); // Update footer
        return prev;

    } else if (!GET_ALLOC(next_header)) { // right free
        /* point to only leftmost, so remove right */
        ll_remove(next);

        //update header and footer
        totalsize = GET_SIZE(HDRP(bp)) + GET_SIZE(next_header); // size of: middle + next
        PUT(HDRP(bp), totalsize); // Update footer
        PUT(FTRP(bp), totalsize); // Update header
        return bp;
    }
    return bp;
}



/* MODIFIED FOR LINKED LIST
 * find_fit - Find a fit for a block with asize bytes
 */
static void *find_fit(size_t asize) {
    for (char *cur_block = (char*) LL_NEXT(PSUB(heap_start, DSIZE)); (size_t) cur_block != (size_t) 0; cur_block = (char *) LL_NEXT(cur_block)) {
        // heap_start is set to the blk ptr of the prologue, so we do (heap_start-16) to get to the padding at the front
        if (asize <= GET_SIZE(HDRP(cur_block))) { // Since this is an explicit linked list containing ONLY free items, checking for freedom would be redundant
            // assert(check_heap(__LINE__));
            return cur_block;
        }
    }
    return NULL;  /* no fit found */
}

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = words * WSIZE;
    if (words % 2 == 1)
        size += WSIZE;
    if ((long)(bp = mem_sbrk(size)) < 0)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */
    
    /* add newly created block to the linked list */
    ll_add(bp);
    return coalesce(bp); // check if the end of the old heap is empty and can be coalesced
}

/* check_ll -- Performs basic checks for the explicit free list's blocks */
static bool check_ll(int line) {
    for (char *cur_block = (char*) LL_NEXT(PSUB(heap_start, DSIZE)); cur_block != 0; cur_block = (char *) LL_NEXT(cur_block)) {
        if (!check_block(line, cur_block) && (GET_SIZE(cur_block) != 0)) {
            printf("(check_ll at line %d) Error: bad block --> ", line);
            print_block(cur_block);
            return false;
        }
    } 
    return true;
}

/*
 * check_heap -- Performs basic heap consistency checks for an implicit free list allocator
 * and // prints out all blocks in the heap in memory order.
 * Checks include proper prologue and epilogue, alignment, and matching header and footer
 * Takes a line number (to give the output an identifying tag).
 */
static bool check_heap(int line) {
    char *bp;

    if ((GET_SIZE(HDRP(heap_start)) != DSIZE) || !GET_ALLOC(HDRP(heap_start))) {
        printf("(check_heap at line %d) Error: bad prologue header\n", line);
        return false;
    }

    for (bp = heap_start; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!check_block(line, bp)) {
            printf("in check_heap()\n");
            return false;
        }
    }

    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))) {
        printf("(check_heap at line %d) Error: bad epilogue header\n", line);
        return false;
    }

    return true;
}


/*
 * check_block -- Checks a block for alignment and matching header and footer
 */
static bool check_block(int line, void *bp) {
    if ((size_t)bp % DSIZE) {
        printf("(check_heap at line %d) Error: %p is not double-word aligned\n", line, bp);
        return false;
    }
    if (GET(HDRP(bp)) != GET(FTRP(bp))) {
        printf("(check_heap at line %d) Error: header does not match footer\n", line);
        printf("header: %ld  |  footer: %ld\n", GET(HDRP(bp)), GET(FTRP(bp)));
        return false;
    }
    return true;
}

/*
 * // print_heap -- Prints out the current state of the implicit free list
 */
static void print_heap() {
    char *bp;

    printf("Heap (%p):\n", heap_start);

    for (bp = heap_start; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        print_block(bp);
    }

    print_block(bp);
}

/*
 * print_block -- Prints out the current state of a block
 */
static void print_block(void *bp) {
    size_t hsize, halloc, fsize, falloc;

    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));

    if (hsize == 0) {
        printf("%p: End of free list\n", bp);
        return;
    }

    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp,
       hsize, (halloc ? 'a' : 'f'),
       fsize, (falloc ? 'a' : 'f'));
}

/*
 * max: returns x if x > y, and y otherwise.
 */
static size_t max(size_t x, size_t y) {
    return (x > y) ? x : y;
}