
#include "httpd.h"

#include <regex.h>
#include <dirent.h>

// XXX: Colocar data no cabeçalho de resposta

inline static int __cmp_cst(const char *cst, char *noncst, int casesen) {
	if (!casesen)
		return !strncmp(cst, noncst, strlen(cst));
	return !strncasecmp(cst, noncst, strlen(cst));
}

int
httpd_init(HTTPDContext *ctx) {
	ctx->head.next = NULL;
	ctx->errorpage = NULL;
}

int
httpd_set(HTTPDContext *ctx, int port, int nclients) {
	assert(port > 0);
	assert(nclients > 0);
	
	ctx->port = port;
	ctx->nclients = nclients;

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
__httpd_extension_return_if_name(HTTPExtension *ext, void *extensionname) {
	assert(ext != NULL);
	assert(extensionname != NULL);

	return !strcasecmp((char*) extensionname, ext->extension);
}

void
httpd_rfc822_time(char *buff, int size, time_t *te) {
	struct tm *t;
	time_t timev;

	assert(buff != NULL);
	assert(size >= 0);

	if (te == NULL) {
		timev = time(NULL);
		t = localtime(&timev);
	} else
		t = localtime(te);

	strftime(buff, size - 1, "%a, %d %b %Y %H:%M:%S GMT%z", t);
}	

static int
__httpd_generate_dir_list(char *dir, char *buff, char *filen) {
#define WR_CODE(MSG, ...) do { \
	char *_msg = MSG; \
	sprintf(buff, MSG,##__VA_ARGS__); \
	fwrite(buff, 1, strlen(buff), fp); } while(0)

	FILE *fp;
	DIR *fdir;
	struct dirent *ent;
	struct stat st;
	char datestring[256];
	struct tm *tm;

	fdir = opendir(dir);
	if (fdir == NULL)
		return FALSE;

	srand(time(NULL));
	sprintf(filen, "%s/index_%d_%d.html", dir, time(NULL), rand());

	fp = fopen(filen, "w+");
	WR_CODE("<html><body>");

	while (ent = readdir(fdir)) {
		if (!strstr(filen, ent->d_name)) {
			sprintf(buff, "%s/%s", ent->d_name);
			stat(buff, &st);
			WR_CODE("<a href=\"%s\">%s</a> ", ent->d_name, ent->d_name);
			tm = localtime(&st.st_mtime);
			strftime(datestring, sizeof(datestring), nl_langinfo(D_T_FMT), tm);
			WR_CODE(" %s ", datestring);
			WR_CODE("<br/>");
		}
	}

	WR_CODE("</body></html>");
	fclose(fp);
	closedir(fdir);

	return TRUE;
}

static void
__httpd_execute(HTTPClient *client, char *file, HTTPMessage *msg) {
#define SEND_MSG(MSG, ...) do { \
	char *_msg = MSG; \
	sprintf(tmpbuff, MSG,##__VA_ARGS__); \
	write(client->_connfd, tmpbuff, strlen(tmpbuff)); } while(0)

	char tmpbuff[300],
	     readbuff[1024],
	     concat[1024];
	char *querystring;
	struct stat st;
	int lenconcat;
	FILE *fp;
	int nread;
	char *pt;
	HTTPExtension *ext;
	int code_response;
	const char *code_msg;
	int page_not_found = FALSE;
	const char *page_not_found_msg = "Page Not Found";
	int temp_file = FALSE;
	int compressed = FALSE, compressed_len = 0;

	// XXX: free querystring, file
	querystring = rindex(file, '?');
	if (querystring != NULL) {
		char *fn;
		fn = querystring;
		querystring = strdup(querystring+1);
		file = strndup(file, fn-file);

		fprintf(stderr, "qs = %s\n file = %s\n", querystring, file);
	}

	lenconcat=(strlen(client->_server->directory)+1);
	memcpy(concat, client->_server->directory, lenconcat);
	strncat(concat, file, sizeof(concat) - lenconcat);

	code_response = 200;
	code_msg = "OK";

	if (stat(concat, &st)) {
		file = client->_server->errorpage;	
		if (file) {
			lenconcat = strlen(client->_server->directory);
			memcpy(concat, client->_server->directory, lenconcat+1);
			concat[lenconcat] = '/';
			concat[lenconcat + 1] = 0;
			strncat(concat, client->_server->errorpage, sizeof(concat) - lenconcat);
		}
		code_response = 404;
		code_msg = "Not Found";
	}

	if (S_ISDIR(st.st_mode)) {
		fprintf(stderr, "directory %s\n", concat);
		if (__httpd_generate_dir_list(concat, readbuff, tmpbuff)) {
			memcpy(concat, tmpbuff, strlen(tmpbuff)+1);
			fprintf(stderr, "file = %s\n", concat);
			file = rindex(concat, '/');
			temp_file = TRUE;
		} else {
			code_response = 404;
			code_msg = "Not Found";
		}
	}
		

	SEND_MSG("HTTP/1.1 %d %s\r\n", code_response, code_msg);

	if (file) {
		pt = rindex(file, '.');
		ext = httpd_extension_return_if(&client->_server->head, __httpd_extension_return_if_name, 
		  (void*) pt);
	} else {
		ext = NULL;
	}


	if (!ext) {
		SEND_MSG("Content-Type: text/plain\n");
	} else{
		SEND_MSG("Content-Type: %s\n", ext->content_type);
	}

	httpd_rfc822_time(readbuff, sizeof readbuff, NULL);
	SEND_MSG("Date: %s\n", readbuff);

	if (file) {
		httpd_rfc822_time(readbuff, sizeof readbuff, &st.st_mtime);
		SEND_MSG("Last-Modified: %s\n", readbuff);
	}

	SEND_MSG("Server: MinHTTPD\n");

	if (!msg->close_connection) {
		SEND_MSG("Connection: Keep-Alive\n");
		SEND_MSG("Keep-Alive: timeout=%d, max=%d\n", client->_server->keepalivetimeout, 
		  client->_server->nclients);
	} else {
		SEND_MSG("Connection: close\n");
	}

	SEND_MSG("Accept-Ranges: bytes\n");

	if (file) {

		if (msg->accept_encoding & GZIP_ENCONDING) {
			gzFile outfile;
			FILE *fin;
			char *tmpfn;

			fprintf(stderr, "Compressing gzip\n");

			//XXX: Mudar por mkstemp	
			tmpfn = tmpnam(NULL);
			outfile = gzopen(tmpfn, "wb");
			compressed = TRUE;
			fin = fopen(concat, "rb");
			while (!feof(fin)) {
				nread = fread(readbuff, 1, sizeof readbuff, fin);
				compressed_len += gzwrite(outfile, readbuff, nread);
			}
			fclose(fin);
			gzclose(outfile);

			fp = fopen(tmpfn, "rb");

			SEND_MSG("Content-Encoding: gzip\n");
		} else {
			fp = fopen(concat, "rb");
		}

		if (fp) {
			rewind(fp);
			fseek(fp, 0L, SEEK_END);
			SEND_MSG("Content-Length: %d\n", ftell(fp));
			rewind(fp);

			SEND_MSG("\r\n");
			
			while (!feof(fp)) {
				nread = fread(readbuff, 1, sizeof readbuff, fp);
				write(client->_connfd, readbuff, nread);
			}
			fclose(fp);
		} else {
			perror("fopen: ");
			
			page_not_found = TRUE;
		}
	} else {
			page_not_found = TRUE;
	}

	if (page_not_found) {
		SEND_MSG("Content-Length: %d\n", strlen(page_not_found_msg));
		SEND_MSG("\r\n");
		SEND_MSG("%s", page_not_found_msg);
	}

	if (temp_file) {
		unlink(concat);
	}

	// CGI support disabled

	fprintf(stderr, "ok %d\n", __LINE__);

}

static void
httpd_get(HTTPClient *client, char *rl, HTTPMessage *msg) {
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
		} else if (!strncmp(pt, "Connection", 10)) {
			GETVAR(vconnection, "Connection");
		}

		pt = strtok(NULL, "\n");
		pos++;
	}

	printf("dir = %s\n", client->_server->directory);

	// XXX: Some variables are not used
	if (vhost) free(vhost);
	if (vuseragent) free(vuseragent);
	if (version) free(version);
	if (vacceptlang) free(vacceptlang);
	if (vaccept) free(vaccept);
	if (vacceptcharset) free(vacceptcharset);

	if (vacceptenc)	{
		pt = vacceptenc;
		while (*pt) {
			while (*pt == ' ') pt++;
			if (__cmp_cst("deflate", pt, TRUE)) {
				msg->accept_encoding |= DEFLATE_ENCODING;
				fprintf(stderr, "deflate supported\n");
			} else if (__cmp_cst("gzip", pt, TRUE)) {
				msg->accept_encoding |= GZIP_ENCONDING;
				fprintf(stderr, "gzip supported\n");
			} else {
				msg->accept_encoding |= INVALID_ENCODING;
				fprintf(stderr, "Format not supported found\n");
			}
			while (*pt && *pt != ',') pt++;
			if (*pt == ',') pt++;
		}

		free(vacceptenc);
	}

	if (vconnection) {
		msg->close_connection = __cmp_cst("close", vconnection, TRUE);
		free(vconnection);
	}

	if (filen) {
		__httpd_execute(client, filen, msg);
		free(filen);
	}
}

static void
http_message_init(HTTPMessage *msg) {
	msg->command          = UNDEFINED_CMD;
	msg->close_connection = FALSE;
	msg->accept_encoding = 0;
}

static void
http_message_clean(HTTPMessage *msg) {
	return;
}

int
httpd_file_exists(const char *path) {
	struct stat st;

	return !stat(path, &st);
}

void
httpd_setdirectory(HTTPDContext *ctx, const char *dir) {
	assert(ctx != NULL);
	assert(dir != NULL);
	
	if (!httpd_file_exists(dir)) {
		fprintf(stderr, "Erro: %s não existe\n", dir);
		exit(-1);
	}

	ctx->directory = strdup(dir);
}

void
httpd_setkeepalivetimeout(HTTPDContext *ctx, int keepalivetimeout) {
	assert(keepalivetimeout >= 0);
	assert(ctx != NULL);

	ctx->keepalivetimeout = keepalivetimeout;
}

void
httpd_seterrorpage(HTTPDContext *ctx, const char *errorpage) {
	assert(errorpage != NULL);
	assert(ctx != NULL);

	ctx->errorpage = strdup(errorpage);
}

static void
httpd_response_client(HTTPClient* client) {
	int n;
	char readline[HTTPD_MAXLINE + 1];
	int closecon = FALSE;
	HTTPMessage msg;

	while (!closecon && (n = read(client->_connfd, readline, HTTPD_MAXLINE)) > 0) {
		readline[n] = 0;
		http_message_init(&msg);

		printf("cmd %s\n", readline);

		if (__cmp_cst("GET", readline, FALSE)) {
			msg.command = GET_CMD;
		} else if (__cmp_cst("POST", readline, FALSE)) {
			msg.command = POST_CMD;
		}

		if (msg.command == UNDEFINED_CMD) {
			msg.close_connection = TRUE;
		} else {
			httpd_get(client, readline, &msg);
		}

		closecon = msg.close_connection;

		http_message_clean(&msg);

		if (!closecon) {
			fd_set rfds;
			struct timeval tv;
			int retval;

			FD_ZERO(&rfds);
			FD_SET(client->_connfd, &rfds);
			
			tv.tv_sec = client->_server->keepalivetimeout;
			tv.tv_usec = 0;

			fprintf(stderr, "Waiting %d seconds...\n", tv.tv_sec);

			retval = select(client->_connfd + 1, &rfds, NULL, NULL, &tv);
			if (retval == -1) {
				perror("select");
				closecon = TRUE;
			} else if (retval == 0) {
				closecon = TRUE;
			} else {
				fprintf(stderr, "Reading a new req\n");
				closecon = FALSE;
			}
		}
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
http_dump_extension(HTTPExtension *ext) {
	fprintf(stderr, "\n\n============\n");
	fprintf(stderr, "extension = (%p)\n", ext->extension, ext->extension);
	fprintf(stderr, "content_type = (%p)\n", ext->content_type, ext->content_type);
	fprintf(stderr, "interpreter = (%p)\n", ext->interpreter, ext->interpreter);
	fprintf(stderr, "type = %d\n", ext->type);
	fprintf(stderr, "next = %p\n", ext->next);
	fprintf(stderr, "============\n\n");
}

void
http_add(HTTPDContext *ctx, char *extensionname, char *content_type, char *interpreter, int type) {

	HTTPExtension *h = &ctx->head,
	              *newext;
	char *start, *end, *ext;
	int len;

	while (h->next != NULL)
		h = h->next;
	
	start = end = extensionname;
	while (*start) {
		end = start;
		while (*end && *end != ',') end++;
		len = end-start;
		ext = (char*) malloc(len+2);
		memcpy(ext, start, len);
		ext[len] = 0;
		start = end + (*end == ',');
		//
		fprintf(stderr, "adding %s (%d) (%p)\n", ext, len, ext);
		//
		newext = (HTTPExtension*) malloc(sizeof(HTTPExtension));
		newext->extension = ext;
		fprintf(stderr, "ext = %p\n", newext->extension);
		newext->content_type = strdup(content_type);
		if (interpreter)
			newext->interpreter = strdup(interpreter);
		else
			newext->interpreter = NULL;
		newext->type = type;
		newext->next = NULL;

		fprintf(stderr, "last = %p\n", newext);

		h->next = newext;
		h = newext;
	}

	fprintf(stderr, "head = %p\n", ctx->head.next);
}

void
httpd_foreach_extension(HTTPExtension *head, void (*func) (HTTPExtension *ext)) {
	HTTPExtension *ext;
	
	assert(head != NULL);
	assert(func != NULL);

	ext = head->next;
	while (ext) {
		func(ext);
		ext = ext->next;
	}
}

HTTPExtension*
httpd_extension_return_if(HTTPExtension *head, int (*func)(HTTPExtension *ext, void *data), void *data) {
	HTTPExtension *ext, *ret;

	assert(head != NULL);
	assert(func != NULL);

	ext = head->next;
	ret = NULL;
	while (ext) {
		if (func(ext, data)) {
			ret = ext;
			ext = NULL;
		} else
			ext = ext->next;
	}

	return ret;
}
