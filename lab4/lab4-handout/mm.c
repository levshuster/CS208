/*
 * CS 208 Lab 4: Malloc Lab
 *
 * <Please put your name and userid here>
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

#include <math.h> //checck if we are aloud to import other functions

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
// #define MIN_Payload_SIZE    16      /* minimum number of bits that the smallest payload can hold */


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
#define PREV_ALLOC(bp) (GET_ALLOC(HDRP(PREV_BLKP(bp)))); // TODO test to make sure works as exspected
/* Given the two pointers of two neighboring unalloced blocks, merge them*/
#define COALESCE(h1, h2) (PUT(h1, GET(h1) + GET(h2))) //double check that first arg should be h1

/* Global variables */

// Pointer to first block
static void *heap_start = NULL;

/* Function prototypes for internal helper routines */

static bool check_heap(int lineno);
static void print_heap();
static void print_block(void *bp);
static bool check_block(int lineno, void *bp);
static void *extend_heap(size_t size);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static size_t max(size_t x, size_t y);

/*
 * mm_init -- <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 */
int mm_init(void) {
    /* create the initial empty heap */
    if ((heap_start = mem_sbrk(4 * WSIZE)) == NULL)
        return -1;

    PUT(heap_start, 0);                        /* alignment padding */
    PUT(PADD(heap_start, WSIZE), PACK(OVERHEAD, 1));  /* prologue header */
    PUT(PADD(heap_start, DSIZE), PACK(OVERHEAD, 1));  /* prologue footer */
    PUT(PADD(heap_start, WSIZE + DSIZE), PACK(0, 1));   /* epilogue header */

    heap_start = PADD(heap_start, DSIZE); /* start the heap at the (size 0) payload of the prologue block */

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

/*
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
    if (size <= 0)
        return NULL;

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
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;

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
    printf("\n\n\n starting to free \n");
    // Calculate what the header/footer would be unallocated:
    size_t newhead = GET_SIZE(HDRP(bp)); // size_t is meant to functinoally be an unsigned long
    newhead = PACK(newhead, 0); // masking: ... & (~0xf)   //TODO turn into bitwise operation or find out about decrement operator (--)
    // Make the last bit of the header 0.
    *HDRP(bp) = newhead; // TODO test to make sure this updates header correctly
    // Make the last bit of the footer 0.
    *FTRP(bp) = newhead; // variable reuse!
    printf("heap at the end of free\n");
    printf("the allocation state of this block is: %i \n\n", (int) GET_ALLOC(HDRP(bp)));
    // print_heap();
    // check_heap(__LINE__);

}

/* The remaining routines are internal helper routines */


/*
 * place -- Place block of asize bytes at start of free block bp
 *          and <How are you handling splitting?>
 * Takes a pointer to a free block and the size of block to place inside it
 * Returns nothing
 * <Are there any preconditions or postconditions?>
 */
static void place(void *bp, size_t asize) { // 'asize' is just the size of the payload + head/foot + padding O
    // calc block size (total w/ byte-alignment)
    printf("\n\ntrying to place\n\n");
    //      done by dividing asize by DSIZE then rounding up and multiplying by 16
    size_t unalloc_size = GET_SIZE(HDRP(bp)) - asize; // size of unallocated remainder - used for splitting

    /* TESTING */
    // printf("Current total unallocated: %i \n", (int) GET_SIZE(HDRP(bp)));
    // printf("Allocating %ld bytes \n", asize);
    // printf("allocated %ld bytes, %ld bytes remain \n", asize, unalloc_size);

    // //      option 2 : size_t size = asize + (asize % 16) + 16;

    // // set header/footer to be size++ (total size + allocation bit)
    size_t header_value = PACK(asize, 1); //double check that this should be size_t// masking: ... & ((~0)-1)   
    //TODO turn into bitwise operation or find out about decrement operator (--)    
        //  header_value = size + 1;

    // // Setting Header/Footer values for 
    PUT(HDRP(bp), header_value); // Update header
    // //      find location of footer by decrementing size by 8
    PUT(FTRP(bp), header_value);
    // PUT(PADD(bp, (size_t) PSUB(asize, WSIZE)), header_value); // Update footer, translation : set the value @ (bp + (size-8)) to header_value

    // // SPLITTING:
    //old code: size_t next_head = (size_t) PADD(HDRP(bp), asize);
    size_t next_head = (size_t) HDRP((size_t) NEXT_BLKP(bp));
    PUT(next_head, unalloc_size); // Update header

    // PUT(PADD(next_head, (size_t) PSUB(unalloc_size, WSIZE)), unalloc_size); // Update footer
    PUT(FTRP(PADD(next_head, 8)), unalloc_size); // FTRP of fake payload pointer, updates footer
    // printf("Bytes split: %ld \n", GET_SIZE(next_head));

    // setting header/footer

    // orignal code from lab assigment 
    // currently does no splitting, just allocates the entire free block
    // PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
    // PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
    //printf("\n]n heap at the end of place");
    //print_heap();
    
}

/*
 * coalesce -- Boundary tag coalescing.
 * Takes a pointer to a free block
 * Return ptr to coalesced block
 * <Are there any preconditions or postconditions?>
 */
static void *coalesce(void *bp) {
    printf("\n\n\n starting to coalesce\n");

    size_t retval = (size_t) bp;
    // Make a pointer to the previous header
    size_t previous_header = (size_t) HDRP(PREV_BLKP(bp));
    printf("the previous header is %ld\n", previous_header);
    printf("no segfault when setting prev header, the prev header allocation state is %i (ref: %ld) \n", ((int) GET_ALLOC(previous_header)), ((size_t) (((size_t) GET_ALLOC(previous_header)) << 63)));
    // if previous is unallocated:
    if (!GET_ALLOC(previous_header)) {
        retval = (size_t) PREV_BLKP(bp); // set the retval to the previous block pointer
        // printf("coalescing previous\n");

        size_t totalsize = GET_SIZE(previous_header) + GET_SIZE(HDRP(bp));
        printf("no segfault when setting total size\n");
        PUT(HDRP(retval), totalsize);
        printf("no segfault when putting header (prev)\n");
        PUT(FTRP(retval), totalsize);
        printf("no segfault when putting footer (prev)\n\n");

        // COALESCE(previous_header, HDRP(bp)); // coalesce the two
        // // put at the footer's pointer (FTRP) the value (GET) at the header's pointer (HDRP)
        // PUT((size_t) FTRP((size_t) retval), (size_t) GET((size_t)HDRP((size_t)retval)));
        // // printf("previous worked\n");
    } 
    // Make a pointer to the next header
    size_t next_header = (size_t) HDRP(NEXT_BLKP(bp));
    printf("the next header is %ld\n", next_header);
    printf("no segfault when setting next header, the next header allocation state is %ld (ref: %ld) \n", GET_ALLOC(next_header), ((size_t) (((size_t) GET_ALLOC(previous_header)) << 63)));
    // if next is unallocated:
    if (!GET_ALLOC(next_header)) {
        // we don't need to change the retval because no matter what
        // it'll always be pointing to the first unalloced block in the order
        // printf("coalescing next\n");

        size_t totalsize = GET_SIZE(next_header) + GET_SIZE(HDRP(bp));
        printf("no segfault when getting total size\n");
        PUT(HDRP(retval), totalsize);
        printf("no segfault when putting header (next)\n");
        PUT(FTRP(retval), totalsize);
        printf("no segfault when putting footer (next)\n\n");

        // COALESCE(HDRP(retval), next_header); // coalesce the two
        // //put at the footer's pointer (FTRP) the value (GET) at the header's pointer (HDRP)
        // PUT((size_t) FTRP((size_t) retval), (size_t) GET((size_t) HDRP((size_t)retval)));
        // // printf("next worked\n");
    }

    // printf("heap at the end of coalesce\n");
    print_heap();
    // check_heap(__LINE__);
    printf("\n\nmake it to the end of a coalese call\n\n");
    return (void*) retval;
}


/*
 * find_fit - Find a fit for a block with asize bytes
 */
static void *find_fit(size_t asize) {
    /* search from the start of the free list to the end */
    for (char *cur_block = heap_start; GET_SIZE(HDRP(cur_block)) > 0; cur_block = NEXT_BLKP(cur_block)) {
        if (!GET_ALLOC(HDRP(cur_block)) && (asize <= GET_SIZE(HDRP(cur_block))))
            return cur_block;
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
    printf("extending heap to %zu bytes\n", mem_heapsize());
    if ((long)(bp = mem_sbrk(size)) < 0)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* new epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
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
