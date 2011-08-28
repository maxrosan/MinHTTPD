
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "httpd.h"
#include "iniparser.h"

struct {
	int port;
	int nclients;
	char *config_filename;
	char *directory;
	//
	HTTPDContext httpd;
} context;

static struct option options_server[] = {
	// Porta a ser usada pelo servidor
	{ .name    = "port",
	  .has_arg = 1,
	  .flag    = 0,
	  .val     = 'p' },
	//
	{ 0, 0, 0, 0 }
};

inline static void
init() {
	context.port = -1;
	context.nclients = 1;
	context.config_filename = "config.ini";
	context.directory = "./";
}

inline static int
parse_args(int argc, char **argv) {
	int c,
	    optindex,
	    ret,
	    port_found;

	if (argc == 2) {
		context.port = atoi(argv[1]);
		return (context.port < 1);
	}
	
	ret = 0;
	port_found = 0;

	while ((c = getopt_long(argc, argv, "p:", options_server, &optindex)) &&
		c != -1) {

		switch (c) {
			case 'p': {
				printf("port = %s", optarg);
				port_found = 1;
				context.port = atoi(optarg);
			} break;
			default:
				printf("Parameter unknown\n");
		}
	}

	if (!port_found) {
		fprintf(stderr, "It is necessary to say what port is gonna be used\n");
		ret |= 1;
	}

	return ret;
}

static int load_config_file() {
	dictionary *dic;
	int i, n;
	char *name;

	dic = iniparser_load(context.config_filename);
	n = iniparser_getnsec(dic);	

	if (n < 0)
		goto freedic;

	//XXX: Falta verificar o retorno das funções
	//
	iniparser_dump(dic, stdout);

	for (i = 0; i < n; i++) {
		name = iniparser_getsecname(dic, i);
		if (!strcmp(name, "server")) {
			char *pathval;
	
			pathval = iniparser_getstring(dic, "server:path", "notfound");
			context.directory = strdup(pathval);
		}
	}

freedic:
	iniparser_freedict(dic);
	return 0;
}

int main(int argc, char **argv) {

	init();
	if (parse_args(argc, argv)) {
		return EXIT_FAILURE;
	}
	if (load_config_file()) {
		return EXIT_FAILURE;
	}

	httpd_init(&context.httpd, context.port, context.nclients, context.directory);
	httpd_run(&context.httpd);

	return EXIT_SUCCESS;
}
