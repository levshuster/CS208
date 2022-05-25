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

/* Given block ptr bp, return the allocation status of the block preceding (in memory order) */
#define PREV_ALLOC(bp) (GET_ALLOC(HDRP(PREV_BLKP(bp))));

/* macros for explicit linked lists */
#define PTR_NEXT_PTR(bp)  ((size_t *) bp) // Returns the pointer to the number that functions as the pointer to the bp of the previous value
#define PTR_PREV_PTR(bp)  ((size_t *) PADD(bp, WSIZE)) // Returns the pointer to the number that functions as the pointer to the bp of the next value

#define PREV_PTR(bp)  (*PTR_PREV_PTR(bp)) // Returns the pointer to the previous bp
#define NEXT_PTR(bp)  (*PTR_NEXT_PTR(bp)) // Returns the pointer to the next bp


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
static void linked_list_coalesce_helper(void *bp);
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

    assert(check_heap(__LINE__));
    
    return 0;
}

/* UPDATED FOR EXPLICIT LINKED LIST
 * mm_malloc -- <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 */
void *mm_malloc(size_t size) {
    assert(check_heap(__LINE__));
    printf("started malloc \n");    
    print_linked_list();
    size_t asize;      /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size <= 0){ 
        assert(check_heap(__LINE__));
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
        // printf("FIND FIT SUCCESS\n");
        place(bp, asize);
        assert(check_heap(__LINE__));
        return bp;
    }
    /* No fit found. Get more memory and place the block */
    extendsize = max(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL){
        assert(check_heap(__LINE__));
        return NULL;
    }

    place(bp, asize);
    
    assert(check_heap(__LINE__));
    
    printf("finished malloc \n");    
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
    assert(check_heap(__LINE__));

    printf("started free\n");
    // Calculate what the header/footer would be unallocated:
    size_t newhead = GET_SIZE(HDRP(bp)); // size_t is meant to functinoally be an unsigned long
    
    newhead = PACK(newhead, 0); // masking: ... & (~0xf)
    
    // Make the last bit of the header and footer 0.
    *HDRP(bp) = newhead;
    *FTRP(bp) = newhead;
    assert(check_heap(__LINE__));
    ll_add(bp);
    assert(check_heap(__LINE__));
    printf("finsihed free\n");
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
    printf("started place\n");
    assert(check_heap(__LINE__));       
    printf("started place\n");
    print_linked_list();
    size_t unalloc_size = PSUB(GET_SIZE(HDRP(bp)), (char*) asize);
    printf("unalloc size: %ld\n", unalloc_size);
    size_t header_value = PACK(asize, 1); //double check that this should be size_t
    assert(check_heap(__LINE__));
    
    /* setting prev and next linked list items */
    size_t* prev = (size_t*) PREV_PTR(bp);
    size_t* next = (size_t*) NEXT_PTR(bp); 
    assert(check_heap(__LINE__)); 
    
    /* Setting Header/Footer values for newly-allocated block */
    assert(check_heap(__LINE__));
    printf("header value: %ld\n", header_value);
    PUT(HDRP(bp), header_value); // Update header
    PUT(FTRP(bp), header_value); // Update footer
    // assert(check_heap(__LINE__));

    /* If there is remaining unallocated memory, split it off */
    if ((unalloc_size > 0)) {
        size_t* unalloc_blk = (size_t*) NEXT_BLKP(bp);
        PUT(HDRP(unalloc_blk), (size_t) unalloc_size); // Update header        
        PUT(FTRP(unalloc_blk), (size_t) unalloc_size); // Update footer

        /* making the block point outwards */
        PUT(PTR_PREV_PTR(unalloc_blk), (size_t) prev); // prev pointer
        PUT(PTR_NEXT_PTR(unalloc_blk), (size_t) next); // next pointer
        
        /* make the neighbors point inwards to this block */
        if (prev) { // if previous is not NULL update previous value
            PUT(PTR_NEXT_PTR(prev), (size_t) unalloc_blk); // set the 'next' pointer of the previous block
        } else { // if the previous item *is* NULL
            PUT(PSUB(heap_start, 16), (size_t) unalloc_blk); // heap start pointer now jumps forward to next item in ll 
            }
        if (next) { // if next can be set (non-NULL)
            PUT(PTR_PREV_PTR(next), (size_t) unalloc_blk); // set the 'prev' pointer of the next block
        }
    } 
    /* Otherwise, don't split */
    else {
        assert(check_heap(__LINE__));
        if (!next) { // bp is at the end of the LL
            PUT(PTR_NEXT_PTR(prev), (size_t) 0); // prev jumps to NULL
        } 
        if (!prev) { // bp is at the start of the LL
            PUT(PTR_PREV_PTR(next), (size_t) 0); // next jumps backward to NULL
 
        } else if (next != NULL) { // bp is at the middle of the LL
            PUT(PTR_PREV_PTR(next), (size_t)prev); // next jumps backward to prev
            PUT(PTR_NEXT_PTR(prev), (size_t)next); // prev jumps forward to next
        } 
    }
    printf("finished place\n");
    assert(check_heap(__LINE__));
}



static void linked_list_coalesce_helper(void *bp) {
    assert(check_heap(__LINE__));
    void* next = NEXT_BLKP(bp); // next block, physically
    void* prev = PREV_BLKP(bp); // previous block, physically
    void* previous_header = HDRP(prev); // Make a pointer to the previous header
    void* next_header = HDRP(next); // Make a pointer to the next header
    assert(check_heap(__LINE__));    
    if ( (GET(previous_header) && !GET_ALLOC(previous_header)) && (GET(next_header) && !GET_ALLOC(next_header)) ) { /* if: (previous exists and unalloc) and (next exists and unalloc) */
        printf("NOT WANTED FOR FIRST: hit the if previous exists and unalloc and next exists and is unalloc");
        assert(check_heap(__LINE__));
        void* ll_next = (void*) NEXT_PTR(next); // next linked list item from 'next'
        void* ll_prev = (void*) PREV_PTR(next); // previous linked list item from 'next'
        /* if-statements to catch edge cases */
        if (!ll_prev) { // if 'next' is at the start of ll
            PUT(PTR_PREV_PTR(ll_next), 0); // next item in ll now jumps backward to NULL
            PUT((size_t*)PTR_NEXT_PTR((size_t*) PSUB(heap_start, 16)), (size_t) ll_next); // heap start pointer now jumps forward to next item in ll
            assert(check_heap(__LINE__));

        } else if (!ll_next) { // if 'next' is at the end of ll
            PUT((size_t*) PTR_PREV_PTR(ll_prev), 0); // set the prev item in the linked list to be the last item
        } else { // if 'next' is in the middle of the linked list
            PUT((size_t*) PTR_NEXT_PTR(ll_prev), (size_t) NEXT_PTR(ll_next)); // ll_prev now jumps forward directly to ll_next 
            PUT((size_t*) PTR_PREV_PTR(ll_next), (size_t) PREV_PTR(ll_next)); // ll_next now jumps backward directly to ll_prev
        }
        print_linked_list();
    } else if (GET(next_header) && !GET_ALLOC(next_header)) { /* if: (next exists and is unalloc) */
    printf("NOT WANTED FOR FIRST: next exists and unalloc\n");
        size_t* ll_next = (size_t*) NEXT_PTR(next); // next linked list item
        size_t* ll_prev = (size_t*) PREV_PTR(next); // prev linked list item

        if (!ll_prev) { // if 'next' is at the beginning
            /* setting bp's values */
            PUT((size_t*) PTR_PREV_PTR(bp), 0); // bp now jumps backward to NULL
            PUT((size_t*) PTR_NEXT_PTR(bp), (size_t) ll_next); // bp now jumps forward to next ll item
            /* setting ll_next's value */
            PUT((size_t*) PTR_PREV_PTR(ll_next), (size_t) bp); // next ll item now jumps backward to bp
            print_linked_list();


        } else if (!ll_next) { // if 'next' is at the end
            /* setting bp's values */
            PUT((size_t*) PTR_NEXT_PTR(bp), 0); // bp now jumps forward to NULL
            PUT((size_t*) PTR_PREV_PTR(bp), (size_t) ll_prev); // bp now jumps backward to the previous linked-list item
            /* setting ll_prev's value */
            PUT((size_t*) PTR_NEXT_PTR(ll_prev), (size_t) bp); // previous linked-list item now jumps to bp
            print_linked_list();

        } else { // if 'next' is in the middle
            /* setting bp's values */
            PUT((size_t*) PTR_PREV_PTR(bp), (size_t) ll_prev); // bp now jumps backward to prev ll item
            PUT((size_t*) PTR_NEXT_PTR(bp), (size_t) ll_next); // bp now jumps forward to next ll item
            /* setting ll_prev and ll_next values */
            PUT((size_t*) PTR_NEXT_PTR(ll_prev), (size_t) bp); // previous ll item now jumps forward to bp
            PUT((size_t*) PTR_PREV_PTR(ll_next), (size_t) bp); // next ll item now jumps backward to bp
            print_linked_list();

        }      
    } else { /* add only current block to the linked list */
        printf(" :) WANTED: hit if this will be the first item in the lined list");

        size_t* start = (size_t*) PSUB(heap_start, DSIZE); // value that points to first ll item
        size_t* second_item = (size_t*) NEXT_PTR(start); // get the old first item; will be 0 if emptty
        /* link bp and second item in the ll */
        print_linked_list();

        if (!GET(start)) { // bp will be placed at the start of an EMPTY linked list
            printf("hit the if padding doesn't point to anything\n");
            assert(check_heap(__LINE__));

            printf("linked list is empty when we coalesce\n");
            PUT(PTR_NEXT_PTR(PSUB(heap_start, DSIZE)), (size_t) bp); // heap start now jumps to bp
            PUT(PTR_NEXT_PTR(bp), 0);
            assert(check_heap(__LINE__));
        } else { // bp will be placed at the start of a NON-EMPTY linked list

            printf("linked list is not empty when we coalesce, we got a %ld\n", GET(start));
            assert(check_heap(__LINE__));
            PUT((size_t*) PTR_NEXT_PTR(bp), (size_t) second_item); // set bp's next item to be the second item
            PUT((size_t*) PTR_PREV_PTR(second_item), (size_t) bp); // set the second item's previous value to bp
            /* link bp and first item pointer */
            assert(check_heap(__LINE__));

            PUT(PTR_PREV_PTR(bp), (size_t) 0); // set bp's previous value to NULL
            PUT((size_t*) start, (size_t) bp); // put bp as start's value 
        } 
    print_linked_list();
    assert(check_heap(__LINE__));
    }
}

/* ll_add -- add a free block to the linked list
* ARGUMENT: void* bp - pointer to the free block's payload
* Returns nothing
* PRECONDITION: this function assumes that the block *is* free and should thus be allowed into the list
*/
static void ll_add(void* bp){
    assert(check_heap(__LINE__));
    size_t* padding = (size_t*) PSUB(heap_start, DSIZE);
    size_t* second = (size_t*) NEXT_PTR(padding);
    if (second != 0) {
        /* putting the newly freed block into the linked list */
        PUT(PTR_PREV_PTR(second), (size_t) bp); // set previous_ll of first item in linked list to be bp
    }
    assert(check_heap(__LINE__));

    PUT(PTR_NEXT_PTR(bp), (size_t) second); // set bp ll_next to current first item
    PUT(PTR_PREV_PTR(bp), (size_t) 0); // set bp previous to equal 0 cast as something
    PUT(padding, (size_t) bp); // set current first item in linked list to be the next value of bp
    assert(check_heap(__LINE__));
}

/* ll_remove -- remove a non-free block from the linked list
* ARGUMENT: void* bp - pointer to the block's payload
* Returns nothing
* PRECONDITION: this function assumes that the block *is not* free and should thus be omitted from the list
*/
static void ll_remove(void* bp) {
    assert(check_heap(__LINE__));
    size_t* ll_prev = (size_t*) PREV_PTR((size_t) bp); //find item before item to be removed
    size_t* ll_next = (size_t*) NEXT_PTR((size_t) bp); //find item after to be removed

    /* take it out! */
    if (!ll_prev && !ll_next) { // if it's empty:
        PUT((size_t*) PSUB(heap_start, DSIZE), (size_t) 0); // start means empty now
    } else if (!ll_prev) { // if it's at the beginning:
        PUT((size_t*) PSUB(heap_start, DSIZE), (size_t) ll_next); // start now jumps to next block in ll
        PUT((size_t*) PTR_PREV_PTR(ll_next), 0); // ll next now jumps backward to 0
    } else if (!ll_next) { // if it's at the end:
        PUT(PTR_NEXT_PTR(ll_prev), (size_t) 0);//set next of preivous item to the location of next item
    } else {
        PUT(PTR_NEXT_PTR(ll_prev), (size_t) ll_next);//set next of preivous item to the location of next item
        PUT(PTR_PREV_PTR(ll_next), (size_t) ll_prev);// set previous of next item to the location of the previous item
    }
    assert(check_heap(__LINE__));
}

/*
 * coalesce -- Boundary tag coalescing.
 * Takes a pointer to a free block
 * Return ptr to coalesced block
 * PRECONDITIONS:
 * * bp must point to a block that is already free
 */
static void *coalesce(void *bp) {
    assert(check_heap(__LINE__));
    printf("started coalesce\n");
    print_linked_list();
    // coalesce the linked list without effecting the blocks:

    size_t* retval = bp;
    //CHECKING PREVIOUS:
    assert(check_heap(__LINE__));
    void* previous_header = HDRP(PREV_BLKP(bp)); // Make a pointer to the previous header
    print_heap();
    print_linked_list();
    if (!GET_ALLOC(previous_header)) { // if previous is unallocated:
        /* Removing the previous and current from linked list */
        ll_remove(bp);
        retval = (size_t*) PREV_BLKP(bp); // set the retval to the previous block pointer
        size_t totalsize = (size_t) PADD( (size_t) GET_SIZE(previous_header), (size_t) GET_SIZE(HDRP(bp)));
            
        PUT(HDRP(retval), totalsize); // Update header
        PUT(FTRP(retval), totalsize); // Update footer
            assert(check_heap(__LINE__));
    }
    print_linked_list();
    // CHECKING NEXT:
    void* next_header = HDRP(NEXT_BLKP(bp)); // Make a pointer to the next header
    if (!GET_ALLOC(next_header)) { // if next is unallocated:
        /* remove ONLY the next item */
        ll_remove((size_t*) NEXT_PTR(retval));

        print_linked_list();        
        size_t totalsize = GET_SIZE(next_header) + GET_SIZE(HDRP(bp));
        PUT(HDRP(retval), totalsize);
        PUT(FTRP(retval), totalsize);
    }
    /* PRINTING LL */
    printf("finished coalescce \n");
    print_linked_list();
    assert(check_heap(__LINE__));
    return retval;
}



/* MODIFIED FOR LINKED LIST
 * find_fit - Find a fit for a block with asize bytes
 */
static void *find_fit(size_t asize) {
    assert(check_heap(__LINE__));
    for (char *cur_block = (char*) NEXT_PTR(PSUB(heap_start, DSIZE)); (size_t) cur_block != (size_t) 0; cur_block = (char *) NEXT_PTR(cur_block)) {
        // heap_start is set to the blk ptr of the prologue, so we do (heap_start-16) to get to the padding at the front
        if (asize <= GET_SIZE(HDRP(cur_block))) { // Since this is an explicit linked list containing ONLY free items, checking for freedom would be redundant
            assert(check_heap(__LINE__));
            return cur_block;
        }
    }
    assert(check_heap(__LINE__));
    return NULL;  /* no fit found */
}

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words) {
    assert(check_heap(__LINE__));
    printf("\n\n\nextend_heap by %ld words\n", words);
    print_linked_list();
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
    print_block(bp);
    /* Coalesce if the previous block was free */
    assert(check_heap(__LINE__));

    /* add newly created block to the linked list */
    assert(check_heap(__LINE__));
    ll_add(bp);
    assert(check_heap(__LINE__));
    printf("finised exstend heap\n\n\n");
    return coalesce(bp); // checck if the end of the old heap is empty and can be coalesced
}

/* check_ll -- Performs basic checks for the explicit free list's blocks */
static bool check_ll(int line) {
    //print_linked_list();
    for (char *cur_block = (char*) NEXT_PTR(PSUB(heap_start, DSIZE)); cur_block != 0; cur_block = (char *) NEXT_PTR(cur_block)) {
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

    return check_ll(line);
}

static void print_linked_list() {
    printf("printing linked list:\n");
    for (char *cur_block = (char*) NEXT_PTR(PSUB(heap_start, DSIZE)); PTR_NEXT_PTR(cur_block) != 0; cur_block = (char *) NEXT_PTR(cur_block)) {
        // heap_start is set to the blk ptr of the prologue, so we do (heap_start-16) to get to the padding at the front
        print_block(cur_block);
    }
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