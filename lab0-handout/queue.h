/* 
 * Starter file for CS 208 Lab 0: Welcome to C
 * Adapted from lab developed at CMU by R. E. Bryant, 2017-2018
 */

/*
 * This program implements a queue supporting both FIFO and LIFO
 * operations.
 *
 * It uses a singly-linked list to represent the set of queue elements
 */

#include <stdbool.h>

/************** Data structure declarations ****************/

/* Linked list element (You shouldn't need to change this) */
typedef struct node {
    /* Pointer to array holding string.
       This array needs to be explicitly allocated and freed */
    char *value;
    struct node *next;
} Node;

/* Queue structure */
typedef struct {
    Node *head;  /* Linked list of elements */
    Node *tail;
    long size;
} Queue;

/************** Operations on queue ************************/

/*
  Create empty queue.
  Return NULL if could not allocate space.
*/
Queue *q_new();

/*
  Free ALL storage used by queue.
  No effect if q is NULL
*/
void q_free(Queue *q);

/*
  Attempt to insert element at head of queue.
  Return true if successful.
  Return false if q is NULL or could not allocate space.
  Argument s points to the string to be stored.
  The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_head(Queue *q, char *s);

/*
  Attempt to insert element at tail of queue.
  Return true if successful.
  Return false if q is NULL or could not allocate space.
  Argument s points to the string to be stored.
  The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_tail(Queue *q, char *s);

/*
  Attempt to remove element from head of queue.
  Return true if successful.
  Return false if queue is NULL or empty.
  If sp is non-NULL and an element is removed, copy the removed string to *sp
  (up to a maximum of bufsize-1 characters, plus a null terminator.)
  The space used by the list element and the string should be freed.
*/
bool q_remove_head(Queue *q, char *sp, long bufsize);

/*
  Return number of elements in queue.
  Return 0 if q is NULL or empty
 */
int q_size(Queue *q);

/*
  Reverse elements in queue
  No effect if q is NULL or empty
  This function should not allocate or free any list elements
  (e.g., by calling q_insert_head, q_insert_tail, or q_remove_head).
  It should rearrange the existing ones.
 */
void q_reverse(Queue *q);
