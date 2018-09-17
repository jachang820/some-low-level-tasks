#! /usr/bin/gnuplot
#
# purpose:
#	 generate data reduction graphs for the multi-threaded list project
#
# input: lab2b_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # lists
#	5. # operations performed (threads x iterations x (ins + lookup + delete))
#	6. run time (ns)
#	7. run time per operation (ns)
#	8. wait for lock time (ns)
#
# output:
#	lab2b_1.png ... throughput vs threads with single list
#	lab2b_2.png ... wait for lock time and time per operation vs threads
#	lab2b_3.png ... threads and iterations that run w/o failure
#	lab2b_4.png ... throughput vs threads with sub lists w/mutex
#       lab2b_5.png ... throughput vs threads with sub lists w/spin-lock
#
# Note:
#	Managing data is simplified by keeping all of the results in a single
#	file.  But this means that the individual graphing commands have to
#	grep to select only the data they want.
#

# general plot parameters
set terminal png
set datafile separator ","

# how much throughput per number of threads competing (w/o yielding)
set title "List-1: Scalability of Synchronization Mechanisms with Bottleneck"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput (1/s)"
set logscale y 10
set output 'lab2b_1.png'
set key left top

#synchronized, non-yield, 1000 iterations, 1 list results`
plot \
     "< cat lab2b_list.csv | grep 'list-none-m,[0-9]*,1000,1,' | \
        grep -e 'm,[1248],' -e 'm,12,' -e 'm,16' -e 'm,24'"  \
	using ($2):(1000000000/($7)) \
	title 'list w/mutex' with linespoints lc rgb 'blue', \
     "< cat lab2b_list.csv | grep 'list-none-s,[0-9]*,1000,1,' | \
        grep -e 's,[1248],' -e 's,12,' -e 's,16' -e 's,24'"  \
	using ($2):(1000000000/($7)) \
	title 'list w/spin-lock' with linespoints lc rgb 'orange', \


# time waiting for a lock vs. overall time per operation per \
#competing threads
set title "List-2: Cost of Synchronization on Single List w/ Mutex"
set xlabel "Threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Cost per operation (ns)"
set logscale y 10
set output 'lab2b_2.png'
set key left top

# grep out mutex-synced, non-yield, 1000 iterations, 1 list results
plot \
     "< cat lab2b_list.csv | grep 'list-none-m,[0-9]*,1000,1,' | \
        grep -e 'm,[1248],' -e 'm,16,' -e 'm,24,'" \
	using ($2):($8) \
	title 'time waiting for lock' with linespoints lc rgb 'blue', \
     "< cat lab2b_list.csv | grep 'list-none-m,[0-9]*,1000,1,' | \
        grep -e 'm,[1248],' -e 'm,16,' -e 'm,24,'" \
	using ($2):($7) \
	title 'time per operation' with linespoints lc rgb 'orange'


# time waiting for a lock as a fraction of over time per operation
# per competing threads
set title "List-3: Iterations that run without failure"
set xlabel "Threads"
set logscale x 2
unset xrange
set xrange [0.75:]
set ylabel "successful iterations"
set logscale y 10
set output 'lab2b_3.png'
set key left top

# note that unsuccessful runs should have produced no output
plot \
     "< cat lab2b_list.csv | grep 'list-id-none,[0-9]*,[0-9]*,4' | \
     	grep -e 'e,[148],[1248],' -e 'e,12,[1248],' -e 'e,16,[1248],' \
	-e 'e,[148],16,' -e 'e,12,16,' -e 'e,16,16,'" \
	using ($2):($3) \
	title '4 sublists w/o sync' with points lc rgb 'violet', \
     "< cat lab2b_list.csv | grep 'list-id-m,[0-9]*,[0-9]*,4' | \
     	grep -e 'm,[148],[1248]0,' -e 'm,12,[1248]0,' -e 'm,16,[1248]0,'" \
	using ($2):($3) \
	title '4 sublists w/mutex' with points pointtype 12 lc rgb 'blue', \
     "< cat lab2b_list.csv | grep 'list-id-s,[0-9]*,[0-9]*,4' | \
     	grep -e 's,[148],[1248]0,' -e 's,12,[1248]0,' -e 's,16,[1248]0,'" \
	using ($2):($3) \
	title '4 sublists w/spin-lock' with points pointtype 2 lc rgb 'orange'


# how much throughput per number of threads competing with bottleneck removed
set title "List-4: Scalability of Mutex with Sub-Lists"
set xlabel "Threads"
set logscale x 2
unset xrange
set xrange [0.75:]
set ylabel "Throughput (1/s)"
set logscale y 10
set output 'lab2b_4.png'
set key left top

# grep out mutex, non-yield, 1000 iterations results
plot \
     "< cat lab2b_list.csv | grep 'list-none-m' | \
        grep -e 'm,[1248],1000,1,' -e 'm,12,1000,1,'" \
	using ($2):(1000000000/($7)) \
	title '1 sublist' with linespoints lc rgb 'blue', \
     "< cat lab2b_list.csv | grep 'list-none-m' | \
        grep -e 'm,[1248],1000,4,' -e 'm,12,1000,4,'" \
	using ($2):(1000000000/($7)) \
	title '4 sublists' with linespoints lc rgb 'violet', \
     "< cat lab2b_list.csv | grep 'list-none-m' | \
        grep -e 'm,[1248],1000,8,' -e 'm,12,1000,8,'" \
	using ($2):(1000000000/($7)) \
	title '8 sublists' with linespoints lc rgb 'red', \
     "< cat lab2b_list.csv | grep 'list-none-m' | \
        grep -e 'm,[1248],1000,16,' -e 'm,12,1000,16,'" \
	using ($2):(1000000000/($7)) \
	title '16 sublists' with linespoints lc rgb 'orange'


# how much throughput per number of threads competing with bottleneck removed
set title "List-5: Scalability of Spin-lock with Sublists"
set xlabel "Threads"
set logscale x 2
unset xrange
set xrange [0.75:]
set ylabel "Throughput (1/s)"
set logscale y 10
set output 'lab2b_5.png'
set key left top

# grep out spin-lock, non-yield, 1000 iterations results
plot \
     "< cat lab2b_list.csv | grep 'list-none-s' | \
        grep -e 's,[1248],1000,1,' -e 's,12,1000,1,'" \
	using ($2):(1000000000/($7)) \
	title '1 sublist' with linespoints lc rgb 'blue', \
     "< cat lab2b_list.csv | grep 'list-none-s' | \
        grep -e 's,[1248],1000,4,' -e 's,12,1000,4,'" \
	using ($2):(1000000000/($7)) \
	title '4 sublists' with linespoints lc rgb 'violet', \
     "< cat lab2b_list.csv | grep 'list-none-s' | \
        grep -e 's,[1248],1000,8,' -e 's,12,1000,8,'" \
	using ($2):(1000000000/($7)) \
	title '8 sublists' with linespoints lc rgb 'red', \
     "< cat lab2b_list.csv | grep 'list-none-s' | \
        grep -e 's,[1248],1000,16,' -e 's,12,1000,16,'" \
	using ($2):(1000000000/($7)) \
	title '16 sublists' with linespoints lc rgb 'orange'