#ifndef HTTPD_H
#define HTTPD_H

#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/stat.h>

typedef struct {
	int _connfd;
	struct _HTTPDContext *_server;
} HTTPClient;

typedef struct _HTTPDContext {
	int port;
	int nclients;
	char *directory;
	//
	int                _listenfd;
	struct sockaddr_in _servaddr;
} HTTPDContext;

#define HTTPD_MAXDATASIZE 100
#define HTTPD_MAXLINE 4096

int httpd_init(HTTPDContext *ctx, int port, int nclients, char *dir);
int httpd_run(HTTPDContext *ctx);

#endif
