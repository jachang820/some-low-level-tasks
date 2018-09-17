#
# NAME: Jonathan Chang
# EMAIL: j.a.chang820@gmail.com
# ID: 104853981
#

CC = gcc
CFLAGS = -g -lmcrypt
TAR = lab1b-104853981.tar.gz
CLIENT = lab1b-client
SERVER = lab1b-server
.SILENT:

# (default)
all: $(CLIENT) $(SERVER)
lab1b-client: $(CLIENT).c
	$(CC) $(CFLAGS) $(CLIENT).c -o $@
lab1b-server: $(SERVER).c
	$(CC) $(CFLAGS) $(SERVER).c -o $@

dist: $(TAR)
files = README Makefile $(CLIENT).c $(SERVER).c my.key
lab1b-104853981.tar.gz: $(files)
	tar -czvf $@ $(files)

.PHONY: clean dist

clean:
	rm -f $(TAR) $(CLIENT) $(SERVER)
