#include <stdio.h>
#include "csapp.h"
#include "doit.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */


int main(int argc,char **argv)
{
    int listenfd,connfd;
    socklen_t clientlen;
    char *port;
    char hostname[MAXLINE];
    struct sockaddr_storage clientaddr;
   
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }
    port=argv[1];
    listenfd=Open_listenfd(port);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
            Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                        port, MAXLINE, 0);
            printf("Accepted connection from (%s, %s)\n", hostname, port);
       
        doit(connfd);
        Close(connfd);
    }
    return 0;
}

