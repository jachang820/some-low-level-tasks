#
# NAME: Jonathan Chang
# EMAIL: j.a.chang820@gmail.com
# ID: 104853981
#

CC = gcc
CFLAGS = -g
TAR = lab1a-104853981.tar.gz
.SILENT:

# (default)
all: lab1a
lab1a: lab1a.c
	$(CC) $(CFLAGS) lab1a.c -o $@

dist: $(TAR)
files = README Makefile lab1a.c
lab1a-104853981.tar.gz: $(files)
	tar -czvf $@ $(files)

.PHONY: clean dist

clean:
	rm -f $(TAR) lab1a
