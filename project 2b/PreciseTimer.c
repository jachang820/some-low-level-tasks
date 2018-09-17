/*
 * NAME: Jonathan Chang
 * EMAIL: j.a.chang820@gmail.com
 * ID: 104853981
 */ 

#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "PreciseTimer.h"


static void get_monotonic_time(struct timespec *tp) {
  if (clock_gettime(CLOCK_MONOTONIC, tp) == -1) {
    switch(errno) {
    case EFAULT:
      fprintf(stderr, 
	      "Clock points outside the accessible address space.\r\n");
      break;
    case EINVAL:
      fprintf(stderr, 
	      "Monotonic clock is not supported on this system.\r\n");
      break;
    case EPERM:
      fprintf(stderr,
	      "No permission to set the clock.\r\n");
      break;
    default:
      fprintf(stderr,
	      "Miscellaneous clock error.\r\n%s\r\n",
	      strerror(errno));
    }
    exit(2);
  }
}


static long long get_diff(struct timespec *start, struct timespec *end) {
  long long billion = 1E9;
  long long sec_diff = (long long) (end->tv_sec - start->tv_sec);
  long long nsec_diff = (long long) (end->tv_nsec - start->tv_nsec);
  return (sec_diff * billion) + nsec_diff;
}


void PreciseTimer_start(struct PreciseTimer *timer) {
  struct timespec *start = &(timer->start_time);
  get_monotonic_time(start);
}


void PreciseTimer_end(struct PreciseTimer *timer) {
  struct timespec *start = &(timer->start_time);
  struct timespec *end = &(timer->end_time);
  get_monotonic_time(end);
  timer->diff = get_diff(start, end);
}


void PreciseTimer_report(struct PreciseTimer *timer) {
  struct timespec *start = &(timer->start_time);
  struct timespec *end = &(timer->end_time);
  fprintf(stderr, "Start sec: %lld nsec: %lld\r\n", start->tv_sec, start->tv_nsec);
  fprintf(stderr, "End sec: %lld nsec: %lld\r\n", end->tv_sec, end->tv_nsec);
  fprintf(stderr, "Diff nsec: %lld\r\n", timer->diff);
}
