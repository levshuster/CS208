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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "queue.h"

/*
  Create empty queue.
  Return NULL if could not allocate space.
*/
queue_t *q_new()
{
    queue_t *q =  malloc(sizeof(queue_t));
    // TODO check if malloc returned NULL (this means space could not be allocated)
    if(q ==  NULL){
      return NULL;
    {}
    q->head = NULL;
    return q;
}

/* Free all storage used by queue */
void q_free(queue_t *q)
{
    // What if q is NULL?
    if(q == NULL){
      return;
    }
    // TODO free the queue nodes
    /* You'll want to loop through the list nodes until the next pointer is NULL,
     * starting at the head, freeing each node and its string. 
     * Account for an empty list (head is NULL). */
    q_free(q->tail);
    // Freeing queue structure itself
    free(q);
}

/*
  Attempt to insert element at head of queue.
  Return true if successful.
  Return false if q is NULL or could not allocate space.
  Argument s points to the string to be stored.
  The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_head(queue_t *q, char *s)
{
    list_ele_t *newh;
    // TODO check if q is NULL

    newh = malloc(sizeof(list_ele_t)); // allocates space on a the heap for the new node
    // TODO check if malloc returned NULL

    // TODO use malloc to allocate space for the value and copy s to value
    // See the C Tutor example linked from page 4 of the writeup!
    // If this second malloc call returns NULL, you need to free newh before returning
    
    newh->next = q->head;
    q->head = newh;
    // if the list was empty, the tail might also need updating
    return true;
}


/*
  Attempt to insert element at tail of queue.
  Return true if successful.
  Return false if q is NULL or could not allocate space.
  Argument s points to the string to be stored.
  The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_tail(queue_t *q, char *s)
{
    // TODO implement in similar fashion to q_insert_head
    // Add a field to queue_t so we can access the tail efficiently
    // if the list was empty, the head might also need updating
    return false;
}

/*
  Attempt to remove element from head of queue.
  Return true if successful.
  Return false if queue is NULL or empty.
  If sp is non-NULL and an element is removed, copy the removed string to *sp
  (up to a maximum of bufsize-1 characters, plus a null terminator.)
  The space used by the list element and the string should be freed.
*/
bool q_remove_head(queue_t *q, char *sp, size_t bufsize)
{
    // TODO check if q is NULL or empty
    // TODO if sp is not NULL, copy value at the head to sp
    // Use strncpy (http://www.cplusplus.com/reference/cstring/strncpy/)
    // bufsize is the number of characters already allocated for sp
    // Copy over bufsize - 1 characters and manually write a null terminator ('\0')
    // to the last index of sp

    // update q->head to remove the current head from the queue
    q->head = q->head->next;
    // if the last list element was removed, the tail might need updating
    return true;
}

/*
  Return number of elements in queue.
  Return 0 if q is NULL or empty
 */
int q_size(queue_t *q)
{
    // TODO implement this function
    // You should add a field to queue_t to keep track of the size
    return 0;
}

/*
  Reverse elements in queue
  No effect if q is NULL or empty
  This function should not allocate or free any list elements
  (e.g., by calling q_insert_head, q_insert_tail, or q_remove_head).
  It should rearrange the existing ones.
 */
void q_reverse(queue_t *q)
{
    /* You need to write the code for this function */
}
