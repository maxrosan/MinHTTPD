
#include "httpd.h"

#include <regex.h>
// XXX: Colocar data no cabeçalho de resposta

int
httpd_init(HTTPDContext *ctx) {
	ctx->head.next = NULL;
}

int
httpd_set(HTTPDContext *ctx, int port, int nclients, char *dir) {
	assert(port > 0);
	assert(nclients > 0);
	
	ctx->port = port;
	ctx->nclients = nclients;
	ctx->directory = strdup(dir);
	//ctx->head.next = NULL;

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
//#define GO(MSG) do { \
//	char *_msg = MSG; \
//	write(client->_connfd, _msg, strlen(_msg)); } while(0)

#define SEND_MSG(MSG, ...) do { \
	char *_msg = MSG; \
	sprintf(tmpbuff, MSG,##__VA_ARGS__); fprintf(stderr, tmpbuff); \
	write(client->_connfd, tmpbuff, strlen(tmpbuff)); } while(0)

	char tmpbuff[300],
	 readbuff[1024];

	fprintf(stderr, "f = %s %s %d\n", file, client->_server->directory, __httpd_has_intepreter(client, file));

	if (__httpd_has_intepreter(client, file) == -1) {
		char *concat;
		struct stat st;

		concat = strcat(client->_server->directory, file);
	
		if (!stat(concat, &st)) {

			char *pt;
			HTTPExtension *ext;
			FILE *fp;
			int nread;

			pt = rindex(file, '.');
#if 0
			if (pt && !strcasecmp(".html", pt)) {
				GO("HTTP/1.1 200 OK\r\n");
				GO("Content-Type: text/plain\r\n");
				GO("Content-Length: 8\r\n");
				GO("\r\n");
				GO("12345678\r\n");
				GO("\r\n");
			}
#endif
			SEND_MSG("HTTP/1.1 200 OK\r\n");

			ext = client->_server->head.next;
			fprintf(stderr, "first = %p\n", ext);
			while (ext) {
				fprintf(stderr, "%s ? %s\n", ext->extension, pt);
				if (!strcasecmp(ext->extension, pt)) {
					break;
				}
				ext = ext->next;
			}
			
			if (!ext) {
				SEND_MSG("Content-Type: text/plain\n");
			} else{
				SEND_MSG("Content-Type: %s\n", ext->content_type);
			}

			fprintf(stderr, "ext = %s\n", ext->extension);

			if (ext->type == FILET) {
				SEND_MSG("Content-Length: %d\n", st.st_size);
				SEND_MSG("\r\n");


				fp = fopen(concat, "rb");
				rewind(fp);
				while (!feof(fp)) {
					nread = fread(readbuff, 1, sizeof readbuff, fp);
					write(client->_connfd, readbuff, nread);
				}
				fclose(fp);

			} else { // CGIT
			}

		} else {
			fprintf(stderr, "%s not found\n", concat);
			SEND_MSG("HTTP/1.1 404 Not Found\r\n");
			SEND_MSG("\r\n");
		}
	}

	fprintf(stderr, "ok %d\n", __LINE__);

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
			break;
		}

		fprintf(stderr, "waiting\n");
	}

	fprintf(stderr, "OK\n");

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

			fprintf(stderr, "::: child = %p\n", ctx->head.next);

			close(ctx->_listenfd);
			client._connfd = connfd;
			client._server = ctx;
			httpd_response_client(&client);

		} else {
			fprintf(stderr, "::: parent = %p\n", ctx->head.next);
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

void
http_add(HTTPDContext *ctx, char *extension, char *content_type, char *interpreter, int type) {

	HTTPExtension *h = &ctx->head,
	              *newext;
	char *start, *end, *ext;
	int len;

	while (h->next != NULL)
		h = h->next;
	
	start = end = extension;
	while (*start) {
		end = start;
		while (*end && *end != ',') end++;
		len = end-start;
		ext = (char*) malloc(len+1);
		memcpy(ext, start, len);
		ext[len] = 0;
		start = end + (*end == ',');
		//
		fprintf(stderr, "adding %s\n", ext);
		//
		newext = (HTTPExtension*) malloc(sizeof(HTTPExtension));
		newext->extension = ext;
		newext->content_type = strdup(content_type);
		if (interpreter)
			newext->interpreter = strdup(interpreter);
		else
			newext->interpreter = NULL;
		newext->type = type;
		newext->next = NULL;

		h->next = newext;
		h = newext;
	}


	fprintf(stderr, "head = %p\n", ctx->head.next);
}
