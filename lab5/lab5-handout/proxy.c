/**
 * @file proxy.c
 * @author your name(s) (you@domain.com)
 * 
 * REPLACE THIS vvv
 * Describe your proxy server here, noting any known bugs or other points of interest
 */

#include "csapp.h"
#include <stdbool.h>

/* MAXLINE is 1024 bytes */

char *DEFAULT_PORT = "80";

/* suggested entry and cache structs for Part II */
typedef struct cache_entry {
    char *url;
    char *header;
    char *item;
    struct cache_entry *next;
    size_t size;
} cache_entry_t; 

typedef struct {
    cache_entry_t *head;
    size_t total_size;
} cache_t;

// For Part II: a global variable to point to the in-memory cache
cache_t *cache;

/* print out the contents of the cache */
void cache_print() {
    cache_entry_t *cur = cache->head;
    printf("current cache: (%zd)\n", cache->total_size);
    while(cur) {
        printf("%s (%zd)\n", cur->url, cur->size);
        cur = cur->next;
    }
}

/* initialize the global cache variable (allocate memory and initialize fields) */
void cache_init() {
    printf("init created\n");
    cache = (cache_t*) malloc(sizeof(cache_t));
    cache->total_size = 0;
    cache->head = NULL;
    printf("cache value is %p", cache);

}

/* deallocate the entire cache (all the entries and the cache global variable) */
void cache_free() {
    cache_entry_t *cur = cache->head;
    cache_entry_t *next;
    while(cur) {
        next = cur->next;
        free(cur->item);
        free(cur->url);
        free(cur);
        cur = next;
    }
    free(cache);
}

/* search cache for an entry with a matching url 
 * return a pointer to the matching entry or NULL if no matching entry is found
*/
cache_entry_t* cache_lookup(char *url) {
    cache_entry_t *cur = cache->head;
    while(cur) {
        printf("compairing %s to %s", cur->url, url);
        if (strcmp(cur->url, url) == 0) {//FOR DUBUGGING : cur-> and * may not be what is needed here
            return cur;
        }
        cur = cur->next;
    }
    printf("cash clookup found no match\n");
    return NULL;
}

/* insert a new entry at the head of the cache */
void cache_insert(char *url, char *item, size_t size) {
    /* I'm a little fuzzy on the mechanics of '->' -Sam */
    cache_entry_t *newitem = (cache_entry_t*) malloc(sizeof(cache_entry_t));
    // set url
    newitem->url = malloc(strlen(url)+1);
    strncpy(newitem->url, url, strlen(url)+1);
    // printf("\n\n url is set to %s by cache_instert, but the given url is %s\n", newitem->url, url);
    // set item
    newitem->item = malloc(strlen(item)+1);
    strncpy(newitem->item, item, strlen(item)+1);
    // printf("item is set to %s by cache_insert\n", newitem->item);
    // set size
    newitem->size = size;

    /* If it will be the first item, set its next item to NULL; otherwise insert normally */
    if (cache->total_size == 0) {
        cache->head = newitem;
        newitem->next = NULL;
    } else {
        cache_entry_t *old_first_item = cache->head;
        cache->head = newitem;
        newitem->next = old_first_item;
    }
    cache->total_size++; // one item had been successfully inserted
}

bool can_respond_with_cache(char *request_url, int connfd){
    /* if we can't find an item, return false */
    cache_entry_t* entry = (cache_entry_t*) cache_lookup(request_url);
    if (!entry) {
        printf("no fit found\n");
        return false;
        }
    printf("fit found\n");
    /* Otherwise, write to the connfd and return true */
    Rio_writen(connfd, entry->item, entry->size);
    return true;
}


/* Implement this function for Part I
 * For Part III, you may need to change the parameter and return type of handle_request
 */
void handle_request(int connfd) { //connfd is the connection file descriptor
    /* Once a connection is established, your proxy should read the entirety of the request from the client 
     * and parse the request. 
     * request the object the client specified. 
     * Finally, your proxy should read the server’s response and forward it to the client. */

    // read and parse request
    //the initial part of the doit function in tiny/tiny.c
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE], url_trim[MAXLINE]; 
    char hostname[MAXLINE], port[MAXLINE], resource[MAXLINE];
    rio_t rio;
    
    printf("\n\n%ld\n\n", connfd);


    /* Read request line and headers */
    Rio_readinitb(&rio, connfd); //initalised the connection to read the request

    // Once a connection is established, your proxy should read the entirety of the request from the client and parse the request. 
    if (!Rio_readlineb(&rio, buf, MAXLINE)){//turn into test
        printf("ERROR: Unable to make a connection using the given fd\n");
        return;
    }   

    // parse URL for hostname, port, and filename, then open a socket on that port and hostname
    if (3 > sscanf(buf, "%s %s %s", method, url, version)) {
        printf("ERROR: bad scan\n");
        return;
    }
    // sscanf(buf, "%s %s %s", method, url, version);
    printf("scanned request line: %s | %s | %s\n", method, url, version);

    // It should determine whether the client has sent a valid HTTP request; 
    // TODO check to make sure url has at least one . (either IP or top level domain)
    if(!sscanf(url, "http://%s", url_trim)){//makes sure usses http protical not https turn into test later
        printf("ERROR: bad URL read (doesn't use http)\nBad URL - %s\n", url);
        return;
    }    

    /* We have, as of this moment, parsed the url. 
     * From here, we just need to see if we can find it in the cache
    */
    printf("url is %s\n", url);
    // if (can_respond_with_cache(url, connfd)) {
    //     printf("we can respond with cache\n");
    //     return;
    // }
    printf("not cached\n");

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


    /* Use the provided Open_clientfd function for this. 
     * It takes two strings arguments, the hostname and the port, and returns a file descriptor. */
    int fd_server = Open_clientfd(hostname, port);
    rio_t rio_server;
    // send request
    sprintf (buf, "GET /%s HTTP/1.0\r\n", resource); 
    Rio_writen(fd_server, buf, strlen(buf));
    sprintf(buf, "Host: %s:%s\r\n", hostname, port); 
    Rio_writen(fd_server, buf, strlen(buf));
    sprintf(buf, "\r\n"); //Accept: */*\r\n
    Rio_writen(fd_server, buf, strlen(buf));

    printf("\n\n211 %ld\n\n", connfd);

    printf("sent request\n");

    Rio_readinitb(&rio_server, fd_server); //initalised the connection to read the request
    
    // Once a connection is established, your proxy should read the entirety of the request from the client 
    // and parse the request. 
    printf("read response\n");
    size_t n;
    // Read header
    n = Rio_readlineb(&rio_server, buf, MAXLINE); // first line: header
    char header[MAXLINE];
    strncpy(header, buf, strlen(buf)+1);
    Rio_writen(connfd, header, n);
    printf("\nHEADER LINE | %s\n", header);
    // Read server type and append it to the headerr line
    n = Rio_readlineb(&rio_server, buf, MAXLINE); // second line: size
    strcat(header, buf);
    Rio_writen(connfd, buf, n);


    printf("\n\n232 %ld\n\n", connfd);


    // Read content size
    n = Rio_readlineb(&rio_server, buf, MAXLINE); // second line: size
    char size[MAXLINE];
    strncpy(size, buf, strlen(buf)+1);
    Rio_writen(connfd, size, n);
    printf("\nSIZE LINE | %s\n", size);

        printf("\n\n%ld\n\n", connfd);

    printf("\n\n245 %ld\n\n", connfd);
    if(!sscanf(size, "Content-length: %s", size)){//makes sure usses http protical not https turn into test later
        printf("ERROR: unable to read size\n");
        return;
    }  
    char *ptr;
    size_t bytes_of_content= strtol(size, &ptr, 10);
        
    printf("the final count is %ld\n", bytes_of_content);
    
    char response[bytes_of_content];
    response[0] = '\0';

        printf("\n\n258    %ld\n\n", connfd);

    int i = 0;
    int newcon = connfd;
    printf("newcon :: %i\n", newcon);
    while ((n = Rio_readlineb(&rio_server, buf, MAXLINE)) != 0) {
        // Rio_writen(connfd, buf, n);
        printf("iteration number %i (pre):: %ld\n", i, connfd);
        strcat(response, buf);
        printf("iteration number %i (post):: %ld\n", i, connfd);
        i++;
        // memcpy( (void*) response, (void*) buf, (size_t) n ); //do for final version

    }
    // n = Rio_readn(fd_server, buf, bytes_of_content);
    printf("buf::%s\n", buf);
    // memcpy( (void*) response, (void*) buf, (size_t) bytes_of_content );
    printf("response is::%s\n", response);

    // printf("RESPONSE : %s\n", response);
    printf("newcon :: %i\n", newcon);
    Rio_writen(newcon, response, bytes_of_content);//shoudl this be maxline?
    cache_insert(url, response, strlen(response)+1);//should response size be MAXLINE?

    // size_t response_size = strlen(response)+1;

    Close(fd_server);
    Close(connfd);   // close the file; we're done with it
    printf("finished handle_request\n\n");

}

int main(int argc, char **argv) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    cache_init();


    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        // For Part III, replace this with code that creates and detaches a thread
        // or otherwise handles the request concurrently
        handle_request(connfd);
    }
    cache_free();
}
