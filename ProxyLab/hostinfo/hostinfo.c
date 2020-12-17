#include "csapp.h"
int main(int argc,char **argv)
{   
    struct addrinfo hints;
    struct addrinfo *listp,*p;
    char buf[MAXLINE];
    if(argc!=2){
        fprintf(stderr,"usage: %s <domain name>\n",argv[0]);
        exit(0);
    }

    /* Get a list of addrinfo records */
    memset(&hints,0,sizeof(struct addrinfo));
    hints.ai_family=AF_INET; /* set ipv4 only */
    hints.ai_socktype=SOCK_STREAM; /* connections only */
    int s=getaddrinfo(argv[1],NULL,&hints,&listp);
    if(s!=0){
        /*getaddrinfo failed */
        fprintf(stderr,"getaddrinfo error: %s\n",gai_strerror(s));
        exit(1);
    }
    /* display each IP address */
    int flags=NI_NUMERICHOST;
    for(p=listp;p!=NULL;p=p->ai_next){
        getnameinfo(p->ai_addr,p->ai_addrlen,buf,MAXLINE,NULL,0,flags);
        printf("%s\n",buf);
    }
    freeaddrinfo(listp);
    return 0;
}