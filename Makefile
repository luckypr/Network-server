CC=gcc

all: 	srv client

srv:	srv.c
	$(CC)  srv.c -l sqlite3 -o srv

client: client.c
	$(CC) client.c -o client

