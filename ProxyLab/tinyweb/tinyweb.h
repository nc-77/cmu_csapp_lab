#ifndef __TINYWEB__
#define __TINYWEB__

void doit(int fd);
void clienterror(int fd,char *cause,char *errnum,char *shortmsg,char *longmsg);
void read_requestHeaders(rio_t *rp);
int parse_uri(char *uri,char *filename,char *cgiargs);
void serve_static(int fd,char *filename,int filesize);
void serve_dynamic(int fd,char *filename,char *cgiargs);
void get_filetype(char *filename,char *filetype);
#endif