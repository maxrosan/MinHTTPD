
#include "httpd.h"

#include <regex.h>

int
httpd_init(HTTPDContext *ctx, int port, int nclients, char *dir) {
	assert(port > 0);
	assert(nclients > 0);
	
	ctx->port = port;
	ctx->nclients = nclients;
	ctx->directory = strdup(dir);

	return 0;
}

static int
__http_match(const char *patt, char *value) {
	regex_t reg;
	int res = 0;

	assert(regcomp(&reg, patt, REG_EXTENDED) == 0);
	switch (regexec(&reg, value, 0, NULL, 0)) {
		case 0:
			res = 0;
			break;
		case REG_NOMATCH:
			res = -1;
			break;
		default:
			printf("Algum erro aconteceu\n");
			res = -1;
			break;
	}
	regfree(&reg);

	return res;
}

static int
__httpd_has_intepreter(HTTPClient *client, char *file) {
	return -1;
}

static void
__httpd_execute(HTTPClient *client, char *file) {
#define GO(MSG) do { \
	char *_msg = MSG; \
	write(client->_connfd, _msg, strlen(_msg)); } while(0)

	printf("f = %s %s %d\n", file, client->_server->directory, __httpd_has_intepreter(client, file));

	if (__httpd_has_intepreter(client, file) == -1) {
		char *concat;
		struct stat *buff;
	
		concat = strcat(client->_server->directory, file);
		fprintf(stderr, "=> %d open %s\n", client->_connfd, concat);
		//write(client->_connfd, "WW\r\n", 4);
		
		if (!stat(concat, buff)) {
			char *pt;

			pt = rindex(file, '.');

			if (pt && !strcasecmp(".html", pt)) {
				GO("HTTP/1.1 200 OK\r\n");
				GO("Content-Type: text/plain\r\n");
				GO("Content-Length: 8\r\n");
				GO("\r\n");
				GO("12345678\r\n");
				GO("\r\n");
			}

		} else {
			fprintf(stderr, "failed");
		}
	}

}

static void
__httpd_get(HTTPClient *client, char *rl) {
#define GETVAR(VAR, CST) do { \
	char *var = CST; \
	while (*pt != ':') pt++; \
	pt++; while (*pt && *pt == ' ') pt++; \
	if (!*pt) break; \
	end = pt; \
	while (*end) end++; \
	VAR = strndup(pt, end-pt); } while(0)

	char *pt;
	int pos = 0;
	char *end;
	int n;
	char *vhost = NULL,
	 *vuseragent = NULL,
	 *vaccept = NULL,
	 *vacceptlang = NULL,
	 *vacceptenc = NULL,
	 *vacceptcharset = NULL,
	 *vconnection = NULL,
	 *filen = NULL,
	 *version = NULL;

	pt = strtok(rl, "\n");
	while (pt != NULL) {

		if (!strncmp(pt, "GET", 3)) {
			if (__http_match("GET (.*) HTTP\\/1\\.([01])", pt)) {
				fprintf(stderr, "Invalid GET command");
				filen = "/";
				version = "HTTP/1.1";
			} else {
				pt = pt + 3; // GET + ' '

				while (*pt == ' ') pt++;
				end = pt;
				while(*end != ' ') end++;
				filen = strndup(pt, end-pt);
				while (*end == ' ') end++;
				version = strndup(end, 8);
			}

		} else if (!strncmp(pt, "Host", 4)) {
			GETVAR(vhost, "Host");
		} else if (!strncmp(pt, "User-Agent", 10)) {
			GETVAR(vuseragent, "User-Agent");
		} else if (!strncmp(pt, "Accept-Language", 15)) {
			GETVAR(vacceptlang, "Accept-Language");
		} else if (!strncmp(pt, "Accept-Encoding", 15)) {
			GETVAR(vacceptenc, "Accept-Encoding");
		} else if (!strncmp(pt, "Accept-Charset", 14)) {
			GETVAR(vacceptcharset, "Accept-Charset");
		} else if (!strncmp(pt, "Accept", 6)) {
			GETVAR(vaccept, "Accept");
		}

		pt = strtok(NULL, "\n");
		pos++;
	}

	printf("dir = %s\n", client->_server->directory);

	if (filen) {
		__httpd_execute(client, filen);
	}

	if (vhost) free(vhost);
	if (vuseragent) free(vuseragent);
	if (filen) free(filen);
	if (version) free(version);
}

static void
httpd_response_client(HTTPClient* client) {
	int n;
	char readline[HTTPD_MAXLINE + 1];

	while ((n = read(client->_connfd, readline, HTTPD_MAXLINE)) > 0) {
		readline[n] = 0;

		printf("cmd %s\n", readline);

		if (!strncmp("GET", readline, 3)) {
			__httpd_get(client, readline);
			//break;
		}
	}

	exit(0);
}

static int
httpd_wait_clients(HTTPDContext *ctx) {
	int connfd;
	int ret;
	pid_t p;

	ret = 0;

	while (1) {
		connfd = accept(ctx->_listenfd, (struct sockaddr*) NULL, NULL);
		if (connfd == -1) {
			ret = -1;
			break;
		}
		
		p = fork();

		if (!p) { // child
			HTTPClient client;

			close(ctx->_listenfd);
			client._connfd = connfd;
			client._server = ctx;
			httpd_response_client(&client);
		}
		close(connfd);
	}

	return ret;
}

int
httpd_run(HTTPDContext *ctx) {
	assert(ctx != NULL);

	ctx->_listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (ctx->_listenfd == -1)
		return -1;

	bzero(&ctx->_servaddr, sizeof(ctx->_servaddr));
	ctx->_servaddr.sin_family      = AF_INET;
	ctx->_servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	ctx->_servaddr.sin_port        = htons(ctx->port);

	if (bind(ctx->_listenfd, (struct sockaddr *)&ctx->_servaddr, sizeof(ctx->_servaddr)) == -1) {
		perror("bind: ");
		goto err_bind;
	}

	if (listen(ctx->_listenfd, ctx->nclients) == -1) {
		goto err_bind;
	}
	
	httpd_wait_clients(ctx);

	return 0;
err_bind:
	close(ctx->_listenfd);
	return -1;
}
