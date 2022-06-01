/**
 * @file proxy.c
 * @author your name(s) (you@domain.com)
 * 
 * REPLACE THIS vvv
 * Describe your proxy server here, noting any known bugs or other points of interest
 */

#include "csapp.h"
#include <stdbool.h>
#include <stdio.h>

#include <unistd.h> //remove for final submission just for waiting


/* MAXLINE is 1024 bytes */

char *DEFAULT_PORT = "80";



pthread_mutex_t mutex;

/* suggested entry and cache structs for Part II */
typedef struct cache_entry {
    char *url;
    char *header;
    char *content;
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
    // pthread_mutex_lock(&mutex);
    // sem_init(&mutex, 0, 1); //figure out what this line does
    printf("init created\n");
    cache = (cache_t*) malloc(sizeof(cache_t));
    cache->total_size = 0;
    cache->head = NULL;
    printf("cache value is %p", cache);
    // pthread_mutex_unlock(&mutex);


}

/* deallocate the entire cache (all the entries and the cache global variable) */
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
 * return a pointer to the matching entry or NULL if no matching entry is found
*/
cache_entry_t* cache_lookup(char *url) {
    cache_entry_t *cur = cache->head;
    while(cur) {
        printf("comparing %s to %s", cur->url, url);
        if (strcmp(cur->url, url) == 0) {//FOR DUBUGGING : cur-> and * may not be what is needed here
            return cur;
        }
        cur = cur->next;
    }
    printf("no match in cache\n");
    return NULL;
}

/* insert a new entry at the head of the cache */
void cache_insert(char *url, char* header, char *item, size_t size) {
    /* I'm a little fuzzy on the mechanics of '->' -Sam */
    cache_entry_t *newitem = (cache_entry_t*) malloc(sizeof(cache_entry_t));
    // set url
    newitem->url = malloc(strlen(url)+1);
    strncpy(newitem->url, url, strlen(url)+1);
    // insert header
    newitem->header = malloc(strlen(header)+1);
    strncpy(newitem->header, header, strlen(header)+1);
    // set content
    newitem->content = malloc(strlen(item)+1);
    strncpy(newitem->content, item, size);
    // memcpy(newitem->content, item, sizeof(item));
    // set size
    newitem->size = size;
    printf("\n\nthe size is set to %ld\n\n", size);
    
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
    cache->total_size++; // one item had been successfully inserted
    pthread_mutex_unlock(&mutex);
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
    printf("the cached stuff is:\n%s\n\n%s", entry->header, entry->content);
    // printf("\n\nsize of entry content: %ld\n", entry->size);
    // printf("header is %s\n", entry->header);
    Rio_writen(connfd, entry->header, strlen(entry->header)); // strlen(entry->header)+1
    printf("\nfinished displaying header\n");
    Rio_writen(connfd, entry->content, entry->size);
    return true;
}


void parseRequest(char *buf, char* method, char* url, char* version, char* url_trim, char* resource, char *port, char*hostname){
// parse URL for hostname, port, and filename, then open a socket on that port and hostname
    if (3 > sscanf(buf, "%s %s %s", method, url, version)) {
        printf("ERROR: bad scan\n");
        return;
    }
    // It should determine whether the client has sent a valid HTTP request; 
    // TODO check to make sure url has at least one . (either IP or top level domain)
    if(!sscanf(url, "http://%s", url_trim)){//makes sure usses http protical not https turn into test later
        printf("ERROR: bad URL read (doesn't use http)\nBad URL - %s\n", url);
        return;
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

    /* Read request line and headers */
    Rio_readinitb(&rio, connfd); //initalised the connection to read the request

    // Once a connection is established, your proxy should read the entirety of the request from the client and parse the request. 
    if (!Rio_readlineb(&rio, buf, MAXLINE)){//turn into test
        printf("ERROR: Unable to make a connection using the given fd\n");
        return;
    }   

    parseRequest(buf, method, url, version, url_trim, resource, port, hostname);

    /* We have, as of this moment, parsed the url. 
     * From here, we just need to see if we can find it in the cache
    */
    // pthread_mutex_lock(&mutex);
    if (can_respond_with_cache(url, connfd)) {
        printf("we can respond with cache\n");
        // Close(connfd);   // close the file; we're done with it
        return;
    }
    // pthread_mutex_unlock(&mutex);
    printf("not cached\n");

    /* Use the provided Open_clientfd function for this. 
     * It takes two strings arguments, the hostname and the port, and returns a file descriptor. */
    int fd_server = Open_clientfd(hostname, port);
    send_request(fd_server, resource, buf, hostname, port);


    rio_t rio_server;
    // send request
    
    Rio_readinitb(&rio_server, fd_server); //initalised the connection to read the request
    
    // Once a connection is established, your proxy should read the entirety of the request from the client 
    // and parse the request. 
    size_t n;
    // Read header
    n = Rio_readlineb(&rio_server, buf, MAXLINE); // first line: header
    char header[MAXLINE];

    // sprintf(buf, "%s 200 OK\r\n", version); //can delte strncpy line and have this line point to header not buf
    strncpy(header, buf, strlen(buf)+1);

    Rio_writen(connfd, buf, n);

    // Read server type and append it to the headerr line
    n = Rio_readlineb(&rio_server, buf, MAXLINE); // second line: size
    strcat(header, buf); // TOD may need to add \r\n
    Rio_writen(connfd, buf, n);

    // Read content size
    n = Rio_readlineb(&rio_server, buf, MAXLINE); // second line: size
    char size[MAXLINE];
    strncpy(size, buf, strlen(buf)+1);
    strcat(header, buf);
    Rio_writen(connfd, buf, n);

    // read content type
    n = Rio_readlineb(&rio_server, buf, MAXLINE);
    char type[MAXLINE];
    strncpy(type, buf, 19);
    type[18] = '\0'; // IMPORTANT!

    strcat(header, buf);
    Rio_writen(connfd, buf, n);

    // \r\n
    n = Rio_readlineb(&rio_server, buf, MAXLINE); // third line: empty
    Rio_writen(connfd, buf, n);
    strcat(header, buf);

    if(!sscanf(size, "Content-length: %s", size)){//makes sure usses http protical not https turn into test later
        printf("ERROR: unable to read size\n");
        return;
    }
    char *ptr;
    size_t bytes_of_content= strtol(size, &ptr, 10);
        
    
    // Read and subsequently write to server:
        char response[bytes_of_content];
    Rio_readnb(&rio_server, response, bytes_of_content);
    Rio_writen(connfd, response, bytes_of_content);

    cache_insert(url, header, response, bytes_of_content); 
    // close the files; we're done with them
    Close(fd_server);
    printf("_________finished handle_request_________\n\n");

}



// int main(int argc, char **argv) {
//     int listenfd, connfd;
//     char hostname[MAXLINE], port[MAXLINE];
//     socklen_t clientlen;
//     struct sockaddr_storage clientaddr;
//     cache_init();


//     /* Check command line args */
//     if (argc != 2) {
//         fprintf(stderr, "usage: %s <port>\n", argv[0]);
//         exit(1);
//     }
    
//     listenfd = Open_listenfd(argv[1]);
//     while (1) {
//         clientlen = sizeof(clientaddr);
//         connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
//         Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
//                     port, MAXLINE, 0);
//         printf("Accepted connection from (%s, %s)\n", hostname, port);

//         // For Part III, replace this with code that creates and detaches a thread
//         // or otherwise handles the request concurrently
//         handle_request(connfd);
//         close(connfd);
//     }
//     cache_free();
//     return 0;
// }




void* threadable_main(void *void_connfd){
    int connfd = (int) void_connfd;
    //sleep(1);
    FILE *fptr;
    fptr = fopen("connfd_after_pthread_call.txt","w");
    fclose(fptr);

    Pthread_detach(pthread_self());
    handle_request(connfd);
    
    Close(connfd);
    return NULL;
}

// concurency attmept 2:
//todo figure out why one is not identical in normal score (get to 4/40)
//todo determin if cashe needs to be on the heap to better share resources
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

    pthread_t threadID;
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);

        // handle_request(connfd);
        Pthread_create(&threadID, NULL, threadable_main, (void *)connfd);
    }

    cache_free();
    return 0;
}
