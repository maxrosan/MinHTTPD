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
#include <langinfo.h>
#include <pwd.h>
#include <grp.h>
#include <locale.h>
#include <fcntl.h>

#define TRUE 1
#define FALSE 0

typedef struct {
	int _connfd;
	struct _HTTPDContext *_server;
} HTTPClient;

enum {
	UNDEFINED_CMD,
	GET_CMD,
	POST_CMD,
};

typedef struct {
	char *filen;
	int command;
	int close_connection;
} HTTPMessage;

enum HTTP_extension_type {
	__EXTENSION_UNDEFINED,
	FILET,
};

typedef struct _HTTPExtension {
	char *extension;
	char *content_type;
	char *interpreter;
	int type;
	struct _HTTPExtension *next;
} HTTPExtension;

typedef struct _HTTPDContext {
	int port;
	int nclients;
	char *directory;
	int keepalivetimeout;
	char *errorpage;
	//
	int                _listenfd;
	struct sockaddr_in _servaddr;
	//
	HTTPExtension head;
} HTTPDContext;

#define HTTPD_MAXDATASIZE 100
#define HTTPD_MAXLINE 4096

int httpd_init(HTTPDContext *ctx);
int httpd_set(HTTPDContext *ctx, int port, int nclients);
void httpd_setdirectory(HTTPDContext *ctx, const char *dir);
void httpd_setkeepalivetimeout(HTTPDContext *ctx, int keepalivetimeout); 
void httpd_seterrorpage(HTTPDContext *ctx, const char *page);
int httpd_run(HTTPDContext *ctx);
void http_add(HTTPDContext *ctx, char *extensionname, char *content_type, char *interpreter, int type);

void http_dump_extension(HTTPExtension *ctx);
void httpd_foreach_extension(HTTPExtension *head, void (*func)(HTTPExtension *ext));
HTTPExtension* httpd_extension_return_if(HTTPExtension *head, int (*func)(HTTPExtension *ext, void*), void *data);
void httpd_rfc822_time(char *buff, int size);

#endif
