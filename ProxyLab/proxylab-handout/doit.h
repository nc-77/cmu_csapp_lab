#ifndef _DOIT_H
#define _DOIT_H
#include "csapp.h"
typedef struct Request
{
    char host[MAXLINE];
    char port[MAXLINE];
    char path[MAXLINE];
}Request;

void clienterror(int fd,char *cause,char *errnum,char *shortmsg,char *longmsg);
void read_requestHeaders(rio_t *rp);
void parse_uri(Request *req,char *uri);
void doit(int clientfd);

#endif