/*
 * echoclient.c - An echo client
 */
/* $begin echoclientmain */
#include "csapp.h"

int main(int argc, char **argv) 
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3) {
	fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
	exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);

    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        /*printf("1.buf:%s", buf);
   
        if (buf[0] == '\n') printf("newline\n");
        if (buf[0] == '\0') printf("terminate\n");*/
        Rio_writen(clientfd, buf, strlen(buf));
       // printf("2.buf:%s", buf);
        Rio_readlineb(&rio, buf, MAXLINE);
       // printf("3.buf:%s", buf);
        Fputs(buf, stdout);
       // printf("4.buf:%s", buf);
    }
    Close(clientfd); //line:netp:echoclient:close
    exit(0);
}
/* $end echoclientmain */
