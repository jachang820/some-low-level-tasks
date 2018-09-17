#
# NAME: Jonathan Chang
# EMAIL: j.a.chang820@gmail.com
# ID: 104853981
#

.SILENT:

CC = gcc
CFLAGS = -g -lmraa -lm -lssl -lcrypto
LAB = lab4c
TAR = $(LAB)-104853981.tar.gz
TCP = $(LAB)_tcp
TLS = $(LAB)_tls
EDISON = EdisonSensors
TIMER = PreciseTimer
INPUT = README Makefile $(LAB).c $(TIMER).h $(TIMER).c $(EDISON).h \
	$(EDISON).c

# (default)
all: $(TCP) $(TLS)
$(TCP): $(LAB).c $(TIMER).c $(EDISON).c
	$(CC) -o $@ $(TIMER).c $(EDISON).c $(LAB).c $(CFLAGS)
$(TLS): $(LAB).c $(TIMER).c $(EDISON).c
	$(CC) -o $@ $(TIMER).c $(EDISON).c $(LAB).c $(CFLAGS)

.PHONY: clean dist

dist: all create_tar
create_tar:
	tar -czvf $(TAR) $(INPUT)

clean:
	rm -f $(TAR) $(TCP) $(TLS)
