/**
 * @file proxy.c
 * @author Sam Johnson-Lacoss <johnsonlacoss@carleton.edu>
 * @author Lev Shuster <shusterl@carleton.edu>
 * 
 * 
 * Zero known bugs
 * Caching and multithreading implemented
 * multithreaded via Pthread_create and Pthread_detach, one only 
 * task identifyied as critical is the incersion into the shared cache
 * a new thread is started whenever there is a new cleint who reaches out to 
 * the listening port of the proxy
 *
 * caching works by breaking the response into a header which contains all the header
 * line and a content. The content is treated as unbitrary bytes so that cache works
 * with both text and binary (like exicutables and pictures)
 * cache adds all responses, and cache is always checked before a new request is made
 * 
 *
 * parsing and send request are handled by helpers
 *
 */

#include "csapp.h"
#include <stdbool.h>
#include <stdio.h>


/* Constands */
char *DEFAULT_PORT = "80";
size_t MAX_ENTRY_SIZE = 400000;
/* MAXLINE is 1024 bytes */



/* Struct used to hold all pertinent info to be cached  */
typedef struct cache_entry {
    char *url;
    char *header;
    char *content; //may contain characters OR arbitary data (binary data)
    struct cache_entry *next;
    size_t size;  //units is bytes
} cache_entry_t; 

/* struct acting as the head to the linked list holding cached information */
typedef struct {
    cache_entry_t *head;
    size_t total_size;
} cache_t;


/* Global Variables */
pthread_mutex_t mutex; //used to signal when threads need to pause to prevent data races
cache_t *cache; //global variable to point to the in-memory cache

/* print out the contents of the cache */
void cache_print() {
    cache_entry_t *cur = cache->head;
    printf("current cache: (%zd)\n", cache->total_size);
    while(cur) {
        printf("%s (%zd)\n", cur->url, cur->size);
        cur = cur->next;
    }
}

/* initialize the global cache variable (allocate memory and initialize fields) 
 * No arguments, no return, no critical sections, called at the start of main
*/
void cache_init() {
    printf("init created\n");
    cache = (cache_t*) malloc(sizeof(cache_t));
    cache->total_size = 0;
    cache->head = NULL;
    printf("cache value is %p", cache);
}

/* deallocate the entire cache (all the entries and the cache global variable) 
 * No arguments, no return, no critical sections, called at the end of main
 */
void cache_free() {
    cache_entry_t *cur = cache->head;
    cache_entry_t *next;
    while(cur) {
        next = cur->next;
        free(cur->header);
        free(cur->content);
        free(cur->url);
        free(cur);
        cur = next;
    }
    free(cache);
}

/* search cache for an entry with a matching url 
 * ARGUMENT: char* url - string that contains the url of the item to look up
 * RETURN: 
         * a pointer to the matching entry if found
         * NULL if no matching entry is found
*/
cache_entry_t* cache_lookup(char *url) {
    cache_entry_t *cur = cache->head;
    while(cur) {
        if (strcmp(cur->url, url) == 0) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

/* insert a new entry at the head of the cache
 * ARGUMENTS:
            * char* url - the full url of the itel to add. This includes the port.
            * char* header - the plaintext header of the requested file (content size, type, etc.)
            * char* item - pointer to the memory location of the content of the file to be cached
            * size_t size - size of the file (not including the header's size)
 * CRITICAL SECTIONS: mutex lock used during modification of the global variable 'cache'
 */
void cache_insert(char *url, char* header, char *item, size_t size) {
    cache_entry_t *newitem = (cache_entry_t*) malloc(sizeof(cache_entry_t));

    // set url
    newitem->url = malloc(strlen(url)+1);
    strncpy(newitem->url, url, strlen(url)+1);

    // insert header
    newitem->header = malloc(strlen(header)+1);
    strncpy(newitem->header, header, strlen(header)+1);

    // set content
    newitem->content = malloc(size);
    memcpy(newitem->content, item, size);

    // set size
    newitem->size = size;

    // adding content from different threads to the same linked list may 
    // causes a data race or other issue thus we set a mutex lock 
    pthread_mutex_lock(&mutex);

    /* If it will be the first item, set its next item to NULL; otherwise insert normally */
    if (cache->total_size == 0) {
        cache->head = newitem;
        newitem->next = NULL;
    } else {
        cache_entry_t *old_first_item = cache->head;
        cache->head = newitem;
        newitem->next = old_first_item;
    }

    cache->total_size++; 

    pthread_mutex_unlock(&mutex);
}

/* ONLY CALLED BY handle_request()
 * search the cache for the requested item.
 * If it finds the item in cache, it writes the requested file to the client
 * if not, it returns to the original caller, handle_request()
 * ARGUMENTS:
            * char* request_url - url of the requested item
            * int connfd - the file descriptor of the client
 * RETURN: 
         * true - if we handled the request with the cache
         * false - if we could not. This returns us to a non-cached version of handle_request()
*/
bool can_respond_with_cache(char *request_url, int connfd){
    /* if we can't find an item, return false */
    cache_entry_t* entry = (cache_entry_t*) cache_lookup(request_url);
    if (!entry) {
        return false;
    }
    /* Otherwise, write to the connfd and return true */
    // printf("\n\nsize of entry content: %ld\n", entry->size);
    // printf("header is %s\n", entry->header);
    Rio_writen(connfd, entry->header, strlen(entry->header)); // strlen(entry->header)+1
    Rio_writen(connfd, entry->content, entry->size);
    return true;
}

/* CALLED ONLY BY handle_request()
 * parses the request sent to handle_request()
 * ARGUMENTS: 
            * PREFACE: all of these are to be set during the function. At the start they are only pointers to empty data fields
            * char* buf        the buffer for string movement
            * char* method     the method of the HTTP request ("GET")
            * char* url        the full url of the request (port/resource included)
            * char* url_trim   the url without "http://", then just the host name
            * char* version    the HTTP version it was requested in (still only handled as 1.0)
            * char* resource   the name of the requested file
            * char* port       the port upon which the request was made
            * char* hostname   same as url_trim at the end, host name
*/
void parse_request(char *buf, char* method, char* url, char* version, char* url_trim, char* resource, char *port, char*hostname){
// parse URL for hostname, port, and filename, then open a socket on that port and hostname
    if (3 != sscanf(buf, "%s %s %s", method, url, version)) {
        printf("ERROR: bad scan\n");
        return;
    }
    // It should determine whether the client has sent a valid HTTP request; 
    // TODO check to make sure url has at least one . (either IP or top level domain)
    if(!sscanf(url, "http://%s", url_trim)){ //makes sure usses http protical not https turn into test later
        strncpy(url_trim, url, strlen(url)+1);
    }  

    // set resource
    char* temp = strchr(url_trim, '/');
    if (temp) {
        strncpy(resource, temp+1, 100);
        *temp = '\0';
    } else {
        printf("\nno resource specified so returned index.html\n\n");
        strncpy(resource, "index.html", strlen("index.html")+1); //TODO : double checck this +1!!!!
    }
    // set port
    temp = strchr(url_trim, ':');
    if (temp) {
        strncpy(port, temp+1, 100);
        *temp = '\0';
    } else {
        strncpy(port, DEFAULT_PORT, strlen(DEFAULT_PORT)+1);
    }
    // set hostname
    strncpy(hostname, url_trim, strlen(url_trim)+1); // for readability

    printf("host: %s  |  port: %s  |  resource: %s\n", hostname, port, resource);
}

void send_request(int fd_server, char *resource, char *buf, char*hostname, char *port){
    sprintf (buf, "GET /%s HTTP/1.0\r\n", resource); 
    Rio_writen(fd_server, buf, strlen(buf));
    sprintf(buf, "Host: %s:%s\r\n", hostname, port); 
    Rio_writen(fd_server, buf, strlen(buf));
    sprintf(buf, "\r\n"); //Accept: */*\r\n
    Rio_writen(fd_server, buf, strlen(buf));
    printf("sent request\n");
}

/* Handles requests sent by the client.
 * This function parses the request, then it tries to use its cache to resolve it.
 * If it cannot, it forwards the request to the server and then forwards the response back to the client.
 * Then, it adds the item to the cache.
 * ARGUMENT: int connfd - the client's file descriptor
 */
void handle_request(int connfd) { //connfd is the connection file descriptor

    // read and parse request
    // the initial part of the doit function in tiny/tiny.c
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE], url_trim[MAXLINE]; 
    char hostname[MAXLINE], port[MAXLINE], resource[MAXLINE];

    /* Read request line and headers */
    rio_t rio;
    Rio_readinitb(&rio, connfd); //initalised the connection to read the request

    // If we cannot read from the client, we cannot proceed. 
    if (!Rio_readlineb(&rio, buf, MAXLINE)){
        printf("ERROR: Unable to make a connection using the given fd\n");
        return;
    }   

    parse_request(buf, method, url, version, url_trim, resource, port, hostname);

    // We have parsed the url that would match the cache. 
    // From here, we just need to see if we can find it in the cache.
    if (can_respond_with_cache(url, connfd)) {
        printf("we can respond with cache\n");
        return;
    }
    printf("not cached\n");

    // Forward the request to the server
    int fd_server;
    if ((fd_server = Open_clientfd(hostname, port)) == 0) {
        printf("ERROR: Invalid Port\n");
        return;
    }
    

    
    send_request(fd_server, resource, buf, hostname, port);

    // initialize a read connection with the server
    rio_t rio_server;
    Rio_readinitb(&rio_server, fd_server); 

    size_t n; // This variable tells us how long our Rio_writen() will write

    // first line: header
    n = Rio_readlineb(&rio_server, buf, MAXLINE); 
    char header[MAXLINE];
    strncpy(header, buf, strlen(buf)+1);
    Rio_writen(connfd, buf, n);

    // second line: server type
    n = Rio_readlineb(&rio_server, buf, MAXLINE); // second line: size
    strcat(header, buf); // TOD may need to add \r\n
    Rio_writen(connfd, buf, n);

    // third line: size
    n = Rio_readlineb(&rio_server, buf, MAXLINE);
    char size[MAXLINE];
    strncpy(size, buf, strlen(buf)+1);
    strcat(header, buf);
    Rio_writen(connfd, buf, n);

    // fourth line: content type
    n = Rio_readlineb(&rio_server, buf, MAXLINE);
    char type[MAXLINE];
    strncpy(type, buf, 19);
    type[18] = '\0'; // IMPORTANT: this sets it up so we can check our file type easily
    strcat(header, buf);
    Rio_writen(connfd, buf, n);

    // fifth line: \r\n
    n = Rio_readlineb(&rio_server, buf, MAXLINE); 
    Rio_writen(connfd, buf, n);
    strcat(header, buf);

    if(!sscanf(size, "Content-length: %s", size)){ //makes sure usses http protical not https turn into test later
        printf("ERROR: unable to read size\n");
        return;
    }

    // parsing size of the content into a size_t
    char *size_ptr;
    size_t bytes_of_content= strtol(size, &size_ptr, 10);

    // This check prevents our proxy from crashing in addition to the tiny server upon a request that is too large.
    // This allows us to keep running even if the server crashed.
    if(bytes_of_content > MAX_ENTRY_SIZE){
        printf("requested package too large for tiny server, please request a smaller file\n");
        return;
    }

    // Read and subsequently write to server:
    char response[bytes_of_content];
    Rio_readnb(&rio_server, response, bytes_of_content);
    Rio_writen(connfd, response, bytes_of_content);

    cache_insert(url, header, response, bytes_of_content); 

    // close the server connection, we're done with it.
    Close(fd_server);
}


void* threadable_main(void *void_connfd){
    int connfd = (int) void_connfd;

    Pthread_detach(pthread_self());
    handle_request(connfd);
    
    Close(connfd);
    return NULL;
}

// concurency attmept 2:
int main(int argc, char **argv) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    
    //initalise a cache for all the threads to share
    cache_init();

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    pthread_t threadID;
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        
        
        Pthread_create(&threadID, NULL, threadable_main, (void *)connfd);
    }

    cache_free();
    return 0;
}
