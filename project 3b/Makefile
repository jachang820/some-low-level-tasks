#
# NAME: Jonathan Chang
# EMAIL: j.a.chang820@gmail.com
# ID: 104853981

.SILENT:

INPUT = README Makefile lab3b.rb
TAR = lab3b-104853981.tar.gz

define RUN_SCRIPT
#
# NAME: Jonathan Chang
# EMAIL: j.a.chang820@gmail.com
# ID: 104853981
#
#!/bin/bash

ruby lab3b.rb $$*

endef

export RUN_SCRIPT

# (default)
all: build
build: lab3b.rb
	echo "$$RUN_SCRIPT" > lab3b
	chmod 775 ./lab3b

dist: build tar
tar:
	tar -czvf $(TAR) $(INPUT)

clean:
	rm -f $(TAR) lab3b
