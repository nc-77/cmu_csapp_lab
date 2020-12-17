#include "csapp.h"
#include "doit.h"
/* 
    处理http事务 
    忽略除GET外所有请求
    忽略任何请求报头
*/
/* Req headers 
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
*/
void doit(int listenfd)
{
    /* 
        通过读写listenfd 与客户端通信
        通过读写clientfd 与服务端通信
     */
    rio_t rio_listen,rio_client;
    int n;
    char buf[MAXLINE],method[MAXLINE],uri[MAXLINE],version[MAXLINE];
    char newreq[MAXLINE+20];
    Request req;
    int clientfd;

    /* proxy 接收客户端 Req */
    Rio_readinitb(&rio_listen,listenfd);
    Rio_readlineb(&rio_listen,buf,MAXLINE);
    printf("Request line:\n");
    printf("%s\n",buf);
    sscanf(buf,"%s %s %s",method,uri,version);
    
    if(strcasecmp(method,"GET")){
        /* 忽略除get外所有请求 */
        clienterror(listenfd,method,"501","Not implemented","Tiny does not implement this method");
        return;
    }
    /* 忽略所有请求报头 */
    read_requestHeaders(&rio_listen);
  
    /* 解析uri */
    parse_uri(&req,uri);
    
    /* 浏览器使用 */
    //sprintf(newreq, "GET http://%s:%s%s HTTP/1.0\r\n\r\n", req.host,req.port,req.path); 
    /* drive.sh 测试用 */
    sprintf(newreq, "GET %s HTTP/1.0\r\n\r\n", req.path);  
    /* proxy 向服务端发送 request */
    clientfd=Open_clientfd(req.host,req.port);
    Rio_writen(clientfd,newreq,MAXLINE);

    printf("proxy send request successfully\n");

    /* proxy 向客户端发送 response */
    Rio_readinitb(&rio_client,clientfd);

    while ((n = Rio_readlineb(&rio_client, buf, MAXLINE))) {//real server response to buf
        printf("proxy received %d bytes,then send\n",n);
        Rio_writen(listenfd, buf, n);  //real server response to real client
    }

    Close(clientfd);
}

void clienterror(int fd,char *cause,char *errnum,char *shortmsg,char *longmsg){
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));

}

void read_requestHeaders(rio_t *rp){
    char buf[MAXLINE];
    memset(buf,0,sizeof(buf));
    printf("request headers:\n");
    while(strcmp(buf,"\r\n")){
        Rio_readlineb(rp,buf,MAXLINE);
        printf("%s",buf);
    }
}

void parse_uri(Request *req,char *uri){
    //printf("parse_uri begin\n");
    
    if (strstr(uri, "http://") != uri) {
        fprintf(stderr, "Error: invalid uri!\n");
        exit(0);
    }
    uri += strlen("http://");
    char *c = strstr(uri, ":");
    if(!c){
        /* 无显示端口默认80 */
        strcpy(req->port,"80");
        c = strstr(uri, "/");
        *c = '\0';
        strcpy(req->host, uri);
        *c = '/';
        strcpy(req->path, c);
    }
    else{
        *c = '\0';
        strcpy(req->host, uri);
        uri = c + 1;
        c = strstr(uri, "/");
        *c = '\0';
        strcpy(req->port, uri);
        *c = '/';
        strcpy(req->path, c);
    }
    
    //printf("parse_uri end\n");
    //printf("host=%s,port=%s,path=%s\n",req->host,req->port,req->path);
}



