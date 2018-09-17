#
# NAME: Jonathan Chang
# EMAIL: j.a.chang820@gmail.com
# ID: 104853981
#

.SILENT:

CC = gcc
CFLAGS = -g -pthread
TAR = lab2a-104853981.tar.gz
SORTED = SortedList
OUTPUT =$(ADD).csv $(LIST).csv $(ADD)-1.png $(ADD)-2.png $(ADD)-3.png \
	$(ADD)-4.png $(ADD)-5.png $(LIST)-1.png $(LIST)-2.png \
	$(LIST)-3.png $(LIST)-4.png
INPUT = README Makefile $(ADD).c $(LIST).c $(SORTED).h $(SORTED).c
GP = /usr/local/cs/bin/gnuplot
ADD = lab2_add
LIST = lab2_list
THR = --threads=$(thread)
ITR = --iterations=$(iter)
SYN = --sync=$(lock)
YD1 = --yield
YD2 = --yield=$(yd_opt)

threads1 := 1 2 4 8 12
iters1 := 10 20 40 80 100 1000 10000 100000
sync1 := m s c

threads2_1 := 1
iters2_1 := 10 100 1000 10000 20000

threads2_2 := 2 4 8 12
iters2_2 := 1 2 4 8 10 16 32 100 1000
yields2_2 := i d il dl

sync2_3 := m s

threads2_4 := 1 2 4 8 12 16 24
iters2_4 := 1000


.PHONY: test dist clean all build


# (default)
all: build
build: $(ADD) $(LIST)
lab2_add: $(ADD).c
	$(CC) $(CFLAGS) $(ADD).c -o $@
lab2_list: $(LIST).c $(SORTED).c
	$(CC) $(CFLAGS) $(SORTED).c $(LIST).c -o $@


test: test_add test_list
test_add: $(ADD)
# lab2_add-1-5.png
	@$(foreach thread, $(threads1), \
	$(foreach iter, $(iters1), \
	./$(ADD) $(THR) $(ITR);))

	@$(foreach thread, $(threads1), \
	$(foreach iter, $(iters1), \
	$(foreach lock, $(sync1), \
	./$(ADD) $(THR) $(ITR) $(SYN);)))

	@$(foreach thread, $(threads1), \
	$(foreach iter, $(iters1), \
	./$(ADD) $(THR) $(ITR) $(YD1);))

	@$(foreach thread, $(threads1), \
	$(foreach iter, $(iters1), \
	$(foreach lock, $(sync1), \
	./$(ADD) $(THR) $(ITR) $(SYN) $(YD1);)))


test_list: $(LIST)
# lab2_list-1.png
	@$(foreach thread, $(threads2_1), \
	$(foreach iter, $(iters2_1), \
	./$(LIST) $(THR) $(ITR);))
# lab2_list-2.png
	-@$(foreach thread, $(threads2_2), \
	$(foreach iter, $(iters2_2), \
	./$(LIST) $(THR) $(ITR);))

	-@$(foreach thread, $(threads2_2), \
	$(foreach iter, $(iters2_2), \
	./$(LIST) $(THR) $(ITR) $(YD2);)))
# lab2_list-3.png
	@$(foreach thread, $(threads2_2), \
	$(foreach iter, $(iters2_2), \
	$(foreach yd_opt, $(yields2_2), \
	$(foreach lock, $(sync2_3), \
	./$(LIST) $(THR) $(ITR) $(SYN) $(YD2);))))
# lab2_list-4.png
	@$(foreach thread, $(threads2_4), \
	$(foreach iter, $(iters2_4), \
	$(foreach lock, $(sync2_3), \
	./$(LIST) $(THR) $(ITR) $(SYN);)))

graphs: $(ADD).gp $(LIST).gp
	-@$(GP) $(ADD).gp
	-@$(GP) $(LIST).gp

dist: $(TAR)
lab2a-104853981.tar.gz: $(INPUT) $(OUTPUT)
	tar -czvf $@ $(INPUT) $(OUTPUT)

clean:
	rm -f $(OUTPUT) $(TAR) $(ADD) $(LIST)
