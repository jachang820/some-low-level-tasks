#
# NAME: Jonathan Chang
# EMAIL: j.a.chang820@gmail.com
# ID: 104853981
#

CC = gcc
CFLAGS = -g
.SILENT:

# (default)
all: lab0
lab0: lab0.c
	$(CC) $(CFLAGS) lab0.c -o $@

check: checkall
checkall: errorfree errorfreecatch segfault segfaultcatch invalidoption \
	invalidarg nonexistentfile permissiondenied stdinstdout inputstdout \
	stdinoutput inputoutput

dist: lab0-104853981.tar.gz
files = README Makefile lab0.c backtrace.png breakpoint.png
lab0-104853981.tar.gz: $(files)
	tar -czvf $@ $(files)

.PHONY: clean dist check

clean:
	rm -f lab0-104853981.tar.gz lab0

# A series of tests for check

errorfree:
# error free build
	if [ -a log ]; then \
		rm log; \
	fi; \
	echo "...build should be free of errors"; \
	if ./lab0 2>stderr > /dev/null && [ ! -s stderr ]; then \
		echo "      OK: Reports no errors."; \
	else \
		echo -n "      ERROR: " >> log; \
		cat stderr; \
	fi; \
	if ./lab0 2>stderr > /dev/null && [ $$? -eq 0 ]; then \
		echo "      OK: RC = 0."; \
	else \
		echo "      ERROR: RC is incorrect." >> log; \
	fi
errorfreecatch:
# error free build --catch
	echo "...--catch should be free of errors"; \
	if ./lab0 2>stderr > /dev/null && [ ! -s stderr ]; then \
		echo "      OK: Reports no errors."; \
	else \
		echo -n "      ERROR: " >> log; \
		cat stderr; \
	fi; \
	if ./lab0 2>stderr > /dev/null && [ $$? -eq 0 ]; then \
		echo "      OK: RC = 0."; \
	else \
		echo "      ERROR: RC is incorrect." >> log; \
	fi
segfault:
# --segfault
	echo "...--segfault should cause segmentation fault"; \
	if ./lab0 --segfault | cat || [ $$? -eq 139 ]; then \
		echo "      OK: RC = 139. Segmentation fault."; \
	else \
		echo "      ERROR: Did not cause segmentation fault." >> log; \
	fi
segfaultcatch:
# --segfault --catch
	echo "...-segfault --catch should return error and exit code"; \
	if ./lab0 --segfault --catch 2>stderr > /dev/null || [ -s stderr ]; then \
		echo -n "      OK: "; \
		cat stderr; \
	else \
		echo -n "      ERROR: " >> log; \
		cat stderr; \
	fi; \
	echo ""; \
	if ./lab0 --segfault --catch 2>stderr > /dev/null || [ $$? -eq 4 ]; then \
		echo "      OK: RC = 4."; \
	else \
		echo "      ERROR: RC is incorrect." >> log; \
	fi
invalidoption:
# --bogus
	echo "...should return error and exit code on undefined options"
	if ./lab0 --bogus 2>stderr > /dev/null || [ -s stderr ]; then \
		echo -n "      OK: "; \
		cat stderr; \
	else \
		echo -n "      ERROR: " >> log; \
		cat stderr; \
	fi; \
	echo ""; \
	if ./lab0 --bogus 2>stderr > /dev/null || [ $$? -eq 1 ]; then \
		echo "      OK: RC = 1."; \
	else \
		echo "      ERROR: RC is incorrect." >> log; \
	fi
invalidarg:
# more bogus
	echo "...should return error and exit code on invalid arguments"
	if ./lab0 more bogus 2>stderr > /dev/null || [ -s stderr ]; then \
		echo -n "      OK: "; \
		cat stderr; \
	else \
		echo -n "      ERROR: " >> log; \
		cat stderr; \
	fi; \
	echo ""; \
	if ./lab0 more bogus 2>stderr > /dev/null || [ $$? -eq 1 ]; then \
		echo "      OK: RC = 1."; \
	else \
		echo "      ERROR: RC is incorrect." >> log; \
	fi
nonexistentfile:
# --input nonexistent file
	echo "...--input should return error if file does not exist"; \
	if ./lab0 --input=topsecret 2>stderr > /dev/null || [ -s stderr ]; then \
		echo -n "      OK: "; \
		cat stderr; \
	else \
		echo -n "      ERROR: " >> log; \
		cat stderr; \
	fi; \
	echo ""; \
	if ./lab0 --input=topsecret 2>stderr > /dev/null || [ $$? -eq 2 ]; then \
		echo "      OK: RC = 2."; \
	else \
		echo -n "      ERROR: RC is incorrect." >> log; \
	fi
permissiondenied:
# --output permission denied
	echo "...--output should return error is r/w permissions not granted"; \
	touch topsecret; \
	chmod 444 topsecret; \
	if ./lab0 --output=topsecret 2>stderr > /dev/null || [ -s stderr ]; then \
		echo -n "      OK: "; \
		cat sterr; \
	else \
		echo -n "      ERROR: " >> log; \
		cat stderr; \
	fi; \
	echo ""; \
	if ./lab0 --output=topsecret 2>stderr > /dev/null || [ $$? -eq 3 ]; then \
		echo "      OK: RC = 3."; \
	else \
		echo "      ERROR: RC is incorrect." >> log; \
	fi; \
	rm -f topsecret
stdinstdout:
# stdin --> stdout
	echo "...should copy standard in to standard out"; \
	head -c 1024 /dev/urandom > RANDOM; \
	if ./lab0 < RANDOM > W_OUT && [ $$? -eq 0 ]; then \
		echo "      OK: RC = 0."; \
	else \
		echo "      ERROR: RC is incorrect." >> log; \
	fi; \
	cmp RANDOM W_OUT > /dev/null; \
	if [ $$? -eq 0 ]; then \
		echo "      OK: Data comparison success."; \
	else \
		echo "      ERROR: Data comparison failure." >> log; \
	fi; \
	rm -f W_OUT
inputstdout:
# input --> stdout
	echo "...should copy input file to standard out"; \
	if ./lab0 --input=RANDOM > W_OUT && [ $$? -eq 0 ]; then \
		echo "      OK: RC = 0."; \
	else \
		echo "      ERROR: RC is incorrect." >> log; \
	fi; \
	cmp RANDOM W_OUT > /dev/null; \
	if [ $$? -eq 0 ]; then \
		echo "      OK: Data comparison success."; \
	else \
		echo "      ERROR: Data comparison failure." >> log; \
	fi; \
	rm -f W_OUT
stdinoutput:
# stdin --> output
	echo "...should copy standard in to output file"; \
	if ./lab0 < RANDOM --output=W_OUT && [ $$? -eq 0 ]; then \
		echo "      OK: RC = 0."; \
	else \
		echo "      ERROR: RC is incorrect." >> log; \
	fi; \
	cmp RANDOM W_OUT > /dev/null; \
	if [ $$? -eq 0 ]; then \
		echo "      OK: Data comparison success."; \
	else \
		echo "      ERROR: Data comparison failure." >> log; \
	fi; \
	rm -f W_OUT
inputoutput:
# input --> output
	echo "...should input file to standard out"; \
	if ./lab0 --input=RANDOM --output=W_OUT && [ $$? -eq 0 ]; then \
		echo "      OK: RC = 0."; \
	else \
		echo "      ERROR: RC is incorrect." >> log; \
	fi; \
	cmp RANDOM W_OUT > /dev/null; \
	if [ $$? -eq 0 ]; then \
		echo "      OK: Data comparison success."; \
	else \
		echo "      ERROR: Data comparison failure." >> log; \
	fi; \
	rm -f W_OUT; \
	rm -f RANDOM; \
	rm -f stderr; \
	echo; \
	echo; \
	if [ ! -a log ]; then \
		echo "passed sanity check"; \
	else \
		echo "failed sanity check"; \
	fi
