#
# NAME: Jonathan Chang
# EMAIL: j.a.chang820@gmail.com
# ID: 104853981

CC = gcc
CFLAGS = -g
TAR = lab3a-104853981.tar.gz
FILES = README Makefile lab3a.c ImgReader.c CsvWriter.c print_error.c \
	ImgReader.h print_error.h ext2_fs.h

.SILENT:

.PHONY: dist clean

# (default)
build: lab3a
lab3a: lab3a.c ImgReader.c CsvWriter.c print_error.c
	$(CC) $(CFLAGS) lab3a.c ImgReader.c CsvWriter.c print_error.c -o $@

dist: build create_tar
create_tar:
	tar -czvf $(TAR) $(FILES)

clean:
	rm -f $(TAR) lab3a
