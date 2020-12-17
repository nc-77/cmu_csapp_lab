#include "csapp.h"


int main (int argc, char **argv) 
{
    char buf[MAXBUF];
    rio_t rio;
    if(argc!=3){
        fprintf(stderr,"usage: %s <host> <port>\n",argv[0]);
        exit(1);
    }
    char *hostname=argv[1];
    char *port=argv[2];
    int clientfd=open_clientfd(hostname,port);
    Rio_readinitb(&rio,clientfd);
    
    while(Fgets(buf,MAXBUF,stdin)!=NULL){
        /* 写入,相当于发送信息到服务器 */
        Rio_writen(clientfd,buf,strlen(buf));
        /* 读取,相当于从服务器接收信息 */
        Rio_readlineb(&rio,buf,MAXLINE);
        /* 将读取到的信息打印到stdout */
        Fputs(buf,stdout);
    }

    return 0;
}