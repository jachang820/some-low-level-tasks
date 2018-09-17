#
# NAME: Jonathan Chang
# EMAIL: j.a.chang820@gmail.com
# ID: 104853981
#

.SILENT:

CC = gcc
CFLAGS = -g -pthread
GPROF = ~/usr/lib/libtcmalloc_and_profiler.a
TAR = lab2b-104853981.tar.gz
SORTED = SortedList
TIMER = PreciseTimer
OUTPUT =lab2b_1.png lab2b_2.png lab2b_3.png lab2b_4.png lab2b_5.png \
	lab2b_list.csv profile.raw profile.out
INPUT = README Makefile lab2_list.c $(SORTED).h $(SORTED).c lab2_list.gp \
	$(TIMER).h $(TIMER).c ListInfo.h
GP = /usr/local/cs/bin/gnuplot
THR = --threads=$(thread)
ITR = --iterations=$(iter)
SYN = --sync=$(lock)
YLD = --yield=$(yd_opt)
LST = --lists=$(list)

threads1 := 1 2 4 8 12 16 24
iters1 := 1000
sync1 := m s
lists1 := 1

threads2 := 1 2 4 8 16 24
iters2 := 1000
sync2 := m
lists2 := 1

threads3 := 1 4 8 12 16
iters3a := 1 2 4 8 16
iters3b := 10 20 40 80
sync3b := m s
yields3 := id
lists3 := 4

threads4 := 1 2 4 8 12
iters4 := 1000
sync4 := m
lists4 := 1 4 8 16

threads5 := 1 2 4 8 12
iters5 := 1000
sync5 := s
lists5 := 1 4 8 16


.PHONY: tests dist clean profile


# (default)
all: build
build: lab2_list
lab2_list: lab2_list.c $(SORTED).c $(TIMER).c
	$(CC) $(CFLAGS) $(SORTED).c $(TIMER).c lab2_list.c -o $@


tests:
# lab2b_1.png
	@$(foreach thread, $(threads1), \
	$(foreach iter, $(iters1), \
	$(foreach lock, $(sync1), \
	$(foreach list, $(lists1), \
	./lab2_list $(THR) $(ITR) $(SYN) $(LST);))))
# lab2b_2.png
	@$(foreach thread, $(threads2), \
	$(foreach iter, $(iters2), \
	$(foreach lock, $(sync2), \
	$(foreach list, $(lists2), \
	./lab2_list $(THR) $(ITR) $(SYN) $(LST);))))
# lab2b_3.png
	-@$(foreach thread, $(threads3), \
	$(foreach iter, $(iters3a), \
	$(foreach yd_opt, $(yields3), \
	$(foreach list, $(lists3), \
	./lab2_list $(THR) $(ITR) $(YLD) $(LST);))))
	@$(foreach thread, $(threads3), \
	$(foreach iter, $(iters3b), \
	$(foreach lock, $(sync3b), \
	$(foreach yd_opt, $(yields3), \
	$(foreach list, $(lists3), \
	./lab2_list $(THR) $(ITR) $(SYN) $(YLD) $(LST);)))))
# lab2b_4.png
	@$(foreach thread, $(threads4), \
	$(foreach iter, $(iters4), \
	$(foreach lock, $(sync4), \
	$(foreach list, $(lists4), \
	./lab2_list $(THR) $(ITR) $(SYN) $(LST);))))
# lab2b_5.png
	@$(foreach thread, $(threads5), \
	$(foreach iter, $(iters5), \
	$(foreach lock, $(sync5), \
	$(foreach list, $(lists5), \
	./lab2_list $(THR) $(ITR) $(SYN) $(LST);))))

profile: lab2_list
	LD_PRELOAD=~/usr/lib/libprofiler.so CPUPROFILE=./profile.raw \
	./lab2_list --threads=12 --iterations=1000 --sync=s
	~/usr/bin/pprof ./lab2_list ./profile.raw --list SortedList_insert > \
	profile.out

graphs: lab2_list lab2_list.gp
	$(GP) lab2_list.gp

dist: build tests profile graphs create_tar
create_tar:
	tar -czvf $(TAR) $(INPUT) $(OUTPUT)

clean:
	rm -f $(OUTPUT) $(TAR) lab2_list
