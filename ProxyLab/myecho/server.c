#include "csapp.h"

void echo(int connfd);

int main(int argc,char **argv)
{
    int listenfd,connfd;
    char client_hostname[MAXLINE],client_port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char *port=argv[1];
    
    if(argc!=2){
        fprintf(stderr,"usage: %s <port>\n",argv[0]);
        exit(1);
    }
    listenfd=open_listenfd(port);
    while(1){
        clientlen=sizeof(struct sockaddr_storage);
        connfd=Accept(listenfd,(SA *)&clientaddr,&clientlen);
        getnameinfo((SA*)&clientaddr,clientlen,client_hostname,MAXLINE,client_port,MAXLINE,0);
        printf("Connected to (%s %s)\n",client_hostname,client_port);
        echo(connfd);
        Close(connfd);
    }
    return 0;
}

void echo(int connfd)
{
    size_t n;
    char buf[MAXBUF];
    char msg[MAXBUF];
    rio_t rio;
    char *buf_head=buf;
    Rio_readinitb(&rio,connfd);
    while((n=Rio_readlineb(&rio,buf,MAXBUF))!=0){
        printf("server received %lu bytes\n",n);
        buf[strlen(buf)-1]=' ';
        sprintf(msg,"%s:this msg is from server\n",buf);
        Rio_writen(connfd,msg,strlen(msg));
    }
}