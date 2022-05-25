/*
 * CS 208 Lab 4: Malloc Lab
 *
 * By Sam Johnson-Lacoss and Lev Shuster (johnsonlacosss shusterl)
 *
 * Simple allocator based on explicit free lists, first fit search,
 * and boundary tag coalescing. 
 * Every time the heap is exstended or an item is freed, the allocator attempts to coalsce
 *
 * Each block has header and footer of the form:
 *
 *      63                  4  3  2  1  0
 *      ------------------------------------
 *     | s  s  s  s  ... s  s  0  0  0  a/f |
 *      ------------------------------------
 * 
 * 
 * Each empty block in addition to the above header and footer has a pointer the
 * previous and next empty block
 *  -------------------------------------------------------------------------
 * | hdr(16:f) | ptr to nxt free(8) | ptr to prev free(8) | ... | ftr(16:f) |
 *  -------------------------------------------------------------------------
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
 *
 * The pad contains all 0s when there are no empty blocks
 * THe pad contains the address of the first empty block otherwise
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

// Returns the pointer to the number that functions as the pointer to the bp of the previous value
#define PTR_NEXT(bp)  ((size_t *) bp) 
// Returns the pointer to the number that functions as the pointer to the bp of the next value
#define PTR_PREV(bp)  ((size_t *) PADD(bp, WSIZE)) 

#define LL_PREV(bp) (*PTR_PREV(bp)) // Returns the pointer to the previous bp
#define LL_NEXT(bp) (*PTR_NEXT(bp)) // Returns the pointer to the next bp



/* Global variables */
static void *heap_start = NULL; // Pointer to first block

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
    .teamname = "johnsonlacosssshusterl",   /* ID1+ID2 or ID1 */
    .name1 = "Sam Johnson-Lacoss",          /* full name of first member */
    .id1 = "johnsonlacosss",                /* login ID of first member */
    .name2 = "Lev Shuster",                 /* full name of second member (if any) */
    .id2 = "shusterl",                      /* login ID of second member */
};

/* 
 * mm_init -- This function creates the initial heap (padding, prologue, and epilogue) then expands the heap
 * the function takes no arguments
 * the function returns 0 if sucsessful and -1 if it was unable to exstend the heap
 * This function takes no arguments nor preconditions
 */
int mm_init(void) {
    /* create the initial empty heap */
    if ((heap_start = mem_sbrk(4 * WSIZE)) == NULL)
        return -1;

    /* linked list start is placed at the padding of the heap; it initially points to 0 signaling 
     * that the linked list is empty(empty) 
     */
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


/*
 * mm_malloc -- allocates a 16-byte aligned chunk of memory that correcponds to the size argument
 * this function takes a 'size' argument, which is the size of the block to allocate
 * This function returns a void*, which is the pointer to the payload of the now allocated chunk of memory
 * mm_malloc does not have preconditions/postconditions besides relying on the place function
 */
void *mm_malloc(size_t size) {
    size_t asize;      /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char *bp;          /* address of the block that will be returned */

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
 * mm_free -- free the block whose payload is pointed to by the given argument (bp)
 * the function takes in a pointer to the block that will be freed
 * This fuction doesn't return anything
 * This function assumes the payload was provided my mm_malloc and has not already been freed 
 * PRECONDITION: The payload was returned by an earlier call to mm_malloc
 * PRECONDITION: The payload has not been previously freed since its most recent return
 * from malloc 
 */
void mm_free(void *bp) {
    /* Calculate what the header/footer would be unallocated:
     * because of alignment, the size of any block will end in a zero
     * so setting the header to be the size of the block also marks the block
     * as unallocated 
     */
    size_t newhead = GET_SIZE(HDRP(bp));

    //Update the header and footer
    PUT(HDRP(bp), newhead);
    PUT(FTRP(bp), newhead);

    ll_add(bp);     //add newly freeded block to the linked list
    coalesce(bp);   //check to see if the newly freed block can be coalesced
}




/* The remaining routines are internal helper routines */

/*
 * place -- Place block of asize bytes at start of free block bp
 *          and if there is any excess memory not needed for asize, 
 *          split by setting the header and footer to unallocated size
 * Takes a pointer to a free block and the size of block to place inside it
 * Returns nothing
 * PRECONDITION: Assumes header of bp is set correctly to be the size of the free block
 * PRECONDITION: Assumes bp points to a free block
 */
static void place(void *bp, size_t asize) { 
    // 'asize' is the payload + head/foot + padding
    // Unalloc_size is the amount of memory not needed for the given payload and its packaging
    size_t unalloc_size = GET_SIZE(HDRP(bp)) - asize;

    // if there is a remainder that is too small to turn into its own block, make the 
    // newly allocated block contain the additional memory
    if (((int)unalloc_size - 32) < 0) { 
        size_t size = GET_SIZE(HDRP(bp));

        // updated the existing hearder to now indicate that it is allocated
        PUT(HDRP(bp), PACK(size, 1)); 
        PUT(FTRP(bp), PACK(size, 1)); 
    } else { // if there is no remainder OR a remainder that is large enough to be split into its own block
        // Update header to be an allocated block of the minimume viable size for the payload
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        // Create a new free block of unalloc_size bytes 
        size_t* next = (size_t*) NEXT_BLKP(bp);
        PUT(HDRP(next), unalloc_size);
        PUT(FTRP(next), unalloc_size);
        ll_add(next); // add newly free block to the linked list
    }

    ll_remove(bp); // remove bp from the linked list
}


/* ll_add -- add a free block to the linked list
* ARGUMENT: void* bp - pointer to the free block's payload
* Returns nothing
* PRECONDITION: this function assumes that the block *is* free but is not already in the linked list
*/
static void ll_add(void* bp){
    size_t* ll_start = (size_t*) PSUB(heap_start, DSIZE);
    
    //second is the pointer to the first free block in the linked list which will soon become the second
    size_t* second = (size_t*) LL_NEXT(ll_start); 

    if (second != 0) { // if the linked list is not empty:
        PUT(PTR_PREV(second), (size_t) bp); // set the pointer of the first item in linked list to be bp
    }

    // update the previous and next pointer of the newly added block of the linked list
    PUT(PTR_NEXT(bp), (size_t) second); 
    PUT(PTR_PREV(bp), (size_t) 0); // set bp previous to equal 0 (signaliing it is at the start of the linked list)

    PUT(ll_start, (size_t) bp); // set update the pointer to the first item in the linked list
}

/* ll_remove -- remove a non-free block from the linked list
* ARGUMENT: void* bp - pointer to the block's payload
* Returns nothing
* PRECONDITION: this function assumes that the block *is not* free and should thus be removed from the list
*/
static void ll_remove(void* bp) {
    size_t* ll_prev = (size_t*) LL_PREV((size_t) bp);       //find item before item to be removed
    size_t* ll_next = (size_t*) LL_NEXT((size_t) bp);       //find item after to be removed
    size_t* ll_start = (size_t*) PSUB(heap_start, DSIZE);   //find the start of the linked list
    
    
    /* take out block from linked list */
    if (!ll_prev && !ll_next) {                  // if the linked list is now empty:
        PUT(ll_start, 0);                        
    } else if (!ll_prev) {                       // if block to be reomved was at the beginning of the linked list:
        PUT(ll_start, (size_t) ll_next);         
        PUT((size_t*) PTR_PREV(ll_next), 0);     
    } else if (!ll_next) {                       // if it's at the end:
        PUT(PTR_NEXT(ll_prev), (size_t) 0); 
    } else {                                     // if none of the edge cases are needed 
        PUT(PTR_NEXT(ll_prev), (size_t) ll_next);// set next of preivous item to the location of next item
        PUT(PTR_PREV(ll_next), (size_t) ll_prev);// set previous of next item to the location of the previous item
    }
}

/*
 * coalesce -- Boundary tag coalescing.
 * Takes a pointer to a free block
 * Return ptr to coalesced block
 * PRECONDITIONS: bp must point to a block that is already free
 */
static void *coalesce(void *bp) {
    void* prev = PREV_BLKP(bp); // Left neibor of newly freed block (on the heap)
    void* next = NEXT_BLKP(bp); // Right neibor of newly freed block (on the heap)

    void* previous_header = HDRP(prev);     // pointer to the previous header
    void* next_header = HDRP(next);         // pointer to the next header

    size_t totalsize; // will be updated to reflect the total size of all the neiboring free blocks 

    if (!GET_ALLOC(previous_header) && !GET_ALLOC(next_header)) { // if both neibors are free
        /* remove middle and next from the linked list */
        ll_remove(bp);
        ll_remove(next);

        // size of: prev + middle + next
        totalsize = GET_SIZE(previous_header) + GET_SIZE(HDRP(bp)) + GET_SIZE(next_header); 

        //update header and footer
        PUT(previous_header, totalsize); 
        PUT(FTRP(prev), totalsize);

        return prev;

    } else if (!GET_ALLOC(previous_header)) { // if only the left is free
        /* remove middle newly freed block from the linked list */
        ll_remove(bp);

        totalsize = GET_SIZE(previous_header) + GET_SIZE(HDRP(bp)); // size of: prev + middle

        //update header and footer
        PUT(previous_header, totalsize);
        PUT(FTRP(prev), totalsize);

        return prev;

    } else if (!GET_ALLOC(next_header)) { // if only the right is free
        /* remove right block from the linked list */
        ll_remove(next);

        totalsize = GET_SIZE(HDRP(bp)) + GET_SIZE(next_header); // size of: middle + next

        //update header and footer
        PUT(HDRP(bp), totalsize); 
        PUT(FTRP(bp), totalsize); 
        return bp;
    }

    // if neither neibor is free makes no change to the heap or linked list
    return bp;
}



/* MODIFIED FOR LINKED LIST
 * find_fit - Find a fit for a block with asize bytes
 * exspects the linked list to be up to date
 * returns a pointer to a porperly sized block or nothing if no fit was found
 * takes in a goal payload size
 */
static void *find_fit(size_t asize) {
    for (char *cur_block = (char*) LL_NEXT(PSUB(heap_start, DSIZE)); (size_t) cur_block != (size_t) 0; 
         cur_block = (char *) LL_NEXT(cur_block)) {
        // heap_start is set to the blk ptr of the prologue, so we do (heap_start-16) to 
        // get to the padding at the front

        if (asize <= GET_SIZE(HDRP(cur_block))) { 
            return cur_block;
        }
    }
    return NULL;  /* no fit found */
}

/*
 * extend_heap - Extend heap with free block and return its block pointer
 * takes in the number of words to exstend the heap by
 * exspects the linked list to be up to date
 * returns the pointer to the newly created block
 */
static void *extend_heap(size_t words) {
    char *bp;                       // will hold the pointer to the newly created block
    size_t size = words * WSIZE;    // how much to exspand the heap by

    /* Allocate an even number of words to maintain alignment */
    if (words % 2 == 1)
        size += WSIZE;
    if ((long)(bp = mem_sbrk(size)) < 0)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */
    
    ll_add(bp);             // add newly created block to the linked list 
    return coalesce(bp);    // check if the end of the old heap is empty and can be coalesced
}



/*
 * check_heap -- Performs basic heap consistency checks for an implicit free list allocator
 * and prints out all blocks in the heap in memory order.
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

/* check_ll -- Performs the same as check heap on on the explicit free list's blocks */
static bool check_ll(int line) {
    for (char *cur_block = (char*) LL_NEXT(PSUB(heap_start, DSIZE)); cur_block != 0; 
         cur_block = (char *) LL_NEXT(cur_block)) {
        if (!check_block(line, cur_block) && (GET_SIZE(cur_block) != 0)) {
            printf("(check_ll at line %d) Error: bad block --> ", line);
            print_block(cur_block);
            return false;
        }
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
 * print_heap -- Prints out the current state of the implicit free list
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