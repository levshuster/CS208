/**
 * @file proxy.c
 * @author your name(s) (you@domain.com)
 * 
 * REPLACE THIS vvv
 * Describe your proxy server here, noting any known bugs or other points of interest
 */

#include "csapp.h"
/* MAXLINE is 1024 bytes */

char *DEFAULT_PORT = "80";

/* suggested entry and cache structs for Part II */
typedef struct cache_entry {
    char *url;
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
    
}

/* deallocate the entire cache (all the entries and the cache global variable) */
void cache_free() {

}

/* search cache for an entry with a matching url 
 * return a pointer to the matching entry or NULL if no matching entry is found
*/
cache_entry_t* cache_lookup(char *url) {
    return NULL;
}

/* insert a new entry at the head of the cache */
void cache_insert(char *url, char *item, size_t size) {
    
}

/* Implement this function for Part I
 * For Part III, you may need to change the parameter and return type of handle_request
 */
void handle_request(int connfd) { //connfd is the connection file descriptor
    printf("starting handle request\n");
    /* Once a connection is established, your proxy should read the entirety of the request from the client 
     * and parse the request. 
     * request the object the client specified. 
     * Finally, your proxy should read the server’s response and forward it to the client. */
    /* 
    Your code in handle_request should use the RIO functions to read the request from the client. 
    Once you have read and parsed the first line of the request from the client, you will need to open a socket to 
    the server. 
    */

    // read and parse request
    //the initial part of the doit function in tiny/tiny.c
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE], url_trim[MAXLINE], port[MAXLINE], resource[MAXLINE]; 
    char hostname[MAXLINE];
    rio_t rio;
    
    /* Read request line and headers */
    Rio_readinitb(&rio, connfd); //initalised the connection to read the request

    // Once a connection is established, your proxy should read the entirety of the request from the client 
    // and parse the request. 
    if (!Rio_readlineb(&rio, buf, MAXLINE)){//turn into test
        printf("ERROR: Unable to make a connection using the given fd\n");
        return;
    }   
    printf("buffer: %s\n", buf);

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
    printf("scanned url trim: %s\n", url_trim);

    // //set resource, left NULL if none exists
    // sscanf(url_trim, "/%s", resource);

    // // checking for port, if port not specified use port 80
    // if(!sscanf(url_trim, ":%s", port))
    //     port = "80";

    // SUPER RISKY:
    // set resource
    char *temp = strchr(url_trim, '/');
    if (temp) {
        strncpy(resource, temp, 100);
        *temp = '\0';
    } else {
	    strncpy(resource, "index.html", strlen("index.html")+1); //TODO : double checck this +1!!!!
    }
    // set port
    *temp = strchr(url_trim, ':');
    if (temp) {
        strncpy(port, temp, 100);
        *temp = '\0';
    } else {
        strncpy(port, DEFAULT_PORT);
    }
    // set hostname
    strncpy(hostname, url_trim, strlen(url_trim)+1); // for readability

    // if(3 > sscanf(url_trim, "%s:%s/%s", hostname, port, resource)) { // if one of these things is missing
    //     printf("missing %i items\n", 3 - check);
    //     //the something's up: trigger emergency cases
    //     if (2 > sscanf(url_trim, "%s:%s", hostname, port)) {
    //         printf("no port\n");
    //         strncpy(port, DEFAULT_PORT, MAXLINE); // then the port must be '80' because not specified
    //         if (2 > scanf(url_trim, "%s/%s", hostname, resource)) {
    //             printf("no resource, only hostname\n");
    //             strncpy(hostname, url_trim, MAXLINE); // then the hostname is full string and resource is left to be NULL
    //         }
    //     }
    // }
    printf("host: %s  |  port: %s  |  resource: %s\n", hostname, port, resource);
    //if so, it can then establish its own connection to the appropriate web server then 
    //request the object the client specified. 
    
    
    /* Use the provided Open_clientfd function for this. 
     * It takes two strings arguments, the hostname and the port, and returns a file descriptor. */
    int fd_server = Open_clientfd(hostname, port);
    rio_t rio_server;
    // what is prxy sending?
    // what is client getting back via curl?
    // run in tiny, one with proxy, one in curl

    // send request
    // TODO may need to remove first line because we handle the request line two commands earlier
    Rio_readinitb(&rio_server, fd_server); //initalised the connection to read the request
    printf("buf: %s\n", buf);
    Rio_writen(fd_server, buf, strlen(buf));// TODO ask if this should be buffer
    
    if (!Rio_readlineb(&rio_server, buf, MAXLINE)){//turn into test
        printf("ERROR: Unable to make a connection using the webserver\n");
        return;
    }   
    // what is proxy?

    // if(!Rio_writen(&rio_server, buf, MAXLINE)){
    //     printf("ERROR: bad sending request (write)\n");
    //     return
    // }


    // Finally, your proxy should read the server’s response and forward it to the client.
    // Close(fd_server);   // TODO : ask if mixed buffered and non buffered
    // Rio_readinitb(&rio_server, fd_server); //initalised the connection to read the request

    // Once a connection is established, your proxy should read the entirety of the request from the client 
    // and parse the request. 

    printf("got a response\nthe returned buffer is %s\n", buf);
    Rio_writen(connfd, buf, MAXLINE); //TODO write test for bad write

    Close(connfd);   // close the file; we're done with it
    // if(!Rio_writen(&rio, buf, MAXLINE)){
    //     printf("ERROR: bad write\n");
    //     return
    // }
    // send request to server as we read it from client (think echo)
    // a request consists of a GET line and zero or more headers
    // a blank line indicates the end of the request headers
    // remember: line endings/blank lines are "\r\n"
    

    // read the response from the server
    // remember: response headers, then a blank line, then the response body
    printf("finished handle_request\n");
}

int main(int argc, char **argv) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

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
}
