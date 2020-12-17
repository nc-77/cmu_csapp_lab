#include "csapp.h"
#include "tinyweb.h"
/* 
    处理http事务 
    忽略除GET外所有请求
    忽略任何请求报头
*/
void doit(int fd)
{
    rio_t rio;
    struct stat statbuf;
    char buf[MAXLINE],method[MAXLINE],uri[MAXLINE],version[MAXLINE];
    char filename[MAXLINE],cgiargs[MAXLINE];
    int is_static;
    /* Read request line and headers */
    Rio_readinitb(&rio,fd);
    Rio_readlineb(&rio,buf,MAXLINE);
    printf("Request line:\n");
    printf("%s\n",buf);
    sscanf(buf,"%s %s %s",method,uri,version);
    
    if(strcasecmp(method,"GET")){
        /* 忽略除get外所有请求 */
        clienterror(fd,method,"501","Not implemented","Tiny does not implement this method");
        return;
    }
    /* 忽略所有请求报头 */
    read_requestHeaders(&rio);

    /* 解析uri */
    is_static=parse_uri(uri,filename,cgiargs);

    if(stat(filename,&statbuf)<0){
        clienterror(fd,filename,"404","Not found","Tiny couldn't find this file");
        return;
    }
    if(is_static){
        /* 正常文件且可以被读取 */
        if(!S_ISREG(statbuf.st_mode)||!(S_IRUSR&statbuf.st_mode)){
            clienterror(fd,filename,"403","Forbidden","Tiny couldn't read this file");
            return;
        }
        serve_static(fd,filename,statbuf.st_size);
    }
    else{
        if(!S_ISREG(statbuf.st_mode)||!(S_IRUSR&statbuf.st_mode)){
            clienterror(fd,filename,"403","Forbidden","Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd,filename,cgiargs);
    }
    
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

    printf("request headers:\n");
    while(strcmp(buf,"\r\n")){
        Rio_readlineb(rp,buf,MAXLINE);
        printf("%s",buf);
    }
}

int parse_uri(char *uri,char *filename,char *cgiargs){
    char *ptr;
    /* static content */
    if(!strstr(uri,"cgi-bin")){
        strcpy(cgiargs,"");
        strcpy(filename,".");
        strcat(filename,uri);
        /* 如果uri以/结尾，加上默认首页 */
        if(uri[strlen(uri)-1]=='/'){
            strcat(filename,"index.html");
        }
        return 1;
    }
    /* dynamic content */
    else{
        ptr=index(uri,'?');
        if(ptr){
            strcpy(cgiargs,ptr+1);
            *ptr='\0';
        }
        else
            strcpy(cgiargs,"");
        
        strcpy(filename,".");
        strcat(filename,uri);
        return 0;
    }
}

void serve_static(int fd,char *filename,int filesize){
    char filetype[MAXLINE],buf[MAXLINE];
    int srcfd;
    char *srcp;

    /* 发送 response headers */
    get_filetype(filename,filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");   
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf)); 

    /* 发送 response body */
    srcfd=Open(filename,O_RDONLY,0);
    /* 将 file 映射到 虚拟内存中 */
    srcp=Mmap(NULL,filesize,PROT_READ,MAP_PRIVATE,srcfd,0);
    Close(srcfd);
    Rio_writen(fd,srcp,filesize);
    Munmap(srcp,filesize);
}

void serve_dynamic(int fd,char *filename,char *cgiargs){
    char buf[MAXLINE];
    /* 发送 response headers */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");   
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    Rio_writen(fd,buf,strlen(buf));

    /* fork一个进程执行cgi */
    if(fork()==0){ /* child */
        setenv("QUERY_STRING",cgiargs,1);
        Dup2(fd,STDOUT_FILENO); // 将stdout重定向到fd
        Execve(filename,NULL,environ);
    }
    Wait(NULL);
}

void get_filetype(char *filename,char *filetype){

    if(strstr(filename,".html"))
        strcpy(filetype,"text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain"); 
}