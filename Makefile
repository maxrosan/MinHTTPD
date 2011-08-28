
CC = gcc
LIBS =  -L$(PWD)/lib/iniparser-2.17 -liniparser
INC = -I./lib/iniparser-2.17/src
FLAGS = -ggdb
OBS = main.o httpd.o
PROGRAM = minhttpd

all: $(OBS) iniparser
	$(CC) $(FLAGS) $(OBS) $(LIBS) -o $(PROGRAM)

iniparser:
	make -C lib/iniparser-2.17/

%.o: %.c
	$(CC) $(FLAGS) $(INC) -c $^ -o $@
