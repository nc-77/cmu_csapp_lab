#include "csapp.h"
#include "tinyweb.h"


int main(int argc,char **argv)
{
    int listenfd,connfd;
    char *port;
    char client_hostname[MAXLINE],client_port[MAXLINE];
    struct sockaddr_storage clientaddr;// 通用socketaddr
    socklen_t clientlen;
    port=argv[1];

    if(argc!=2){
        fprintf(stderr,"usage: %s <port>\n",argv[0]);
        exit(1);
    }
    listenfd=open_listenfd(port);
    while(1){
        clientlen=sizeof(struct sockaddr_storage);
        connfd=Accept(listenfd,(SA*)&clientaddr,&clientlen);
        getnameinfo((SA*)&clientaddr,clientlen,client_hostname,MAXLINE,client_port,MAXLINE,0);
        printf("Accepted connection from (%s %s)\n",client_hostname,client_port);
        
        doit(connfd);
        Close(connfd);
    }
    return 0;
}