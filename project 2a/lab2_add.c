/*
 * NAME: Jonathan Chang
 * EMAIL: j.a.chang820@gmail.com
 * ID: 104853981
 */ 

#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <getopt.h>

/* program parameter values */
int opt_yield;
int num_threads;
long num_iterations;
enum sync_option {SYNC_MUTEX, SYNC_SPIN_LOCK, SYNC_COMPARE_SWAP, UNSYNCED} 
  sync_opt;

/* result of operations */
long long counter = 0;
int *thread_id;

/* sync objects */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int spinlock = 0;

/* function declarations */
static void* add_wrapper(void*);
void add_compare_and_swap(long long*, long long);
void add(long long*, long long);
void get_monotonic_time(struct timespec*);
long long diff_time(struct timespec*, struct timespec*);
void process_args(int, char**);
void create_threads(pthread_t*);
void join_threads(pthread_t*);
void append_csv(long long);
char* compute_test_name(void);


int main(int argc, char *argv[]) {
  struct timespec start_time, end_time;
  long long diff;
  pthread_t *threads;

  process_args(argc, argv);
  threads = calloc(num_threads, sizeof(pthread_t));
  get_monotonic_time(&start_time);
  create_threads(threads);
  join_threads(threads);
  get_monotonic_time(&end_time);
  diff = diff_time(&start_time, &end_time);
  free(threads);
  pthread_mutex_destroy(&mutex);
  append_csv(diff);

  exit(0);
}


/*! Allows add function to be used by pthread */
static void* add_wrapper(void* thread_id) {
  int n;
  int tid = *((int*)thread_id);

  void (*operation)(long long*, long long);
  if (sync_opt == SYNC_COMPARE_SWAP) {
    operation = &add_compare_and_swap;
  }
  else { /* UNSYNCED */
    operation = &add;
  }

  /* lock */
  if (sync_opt == SYNC_MUTEX) { 
    pthread_mutex_lock(&mutex);
  }
  else if (sync_opt == SYNC_SPIN_LOCK) {
    while (__sync_lock_test_and_set(&spinlock, 1));
  }
     
  /* operation */
  for (n = 0; n < num_iterations; n++) {
    (*operation)(&counter, 1);
  }
  for (n = 0; n < num_iterations; n++) {
    (*operation)(&counter, -1);
  }

  /* unlock */
  if (sync_opt == SYNC_MUTEX) {
    pthread_mutex_unlock(&mutex);
  }
  else if (sync_opt == SYNC_SPIN_LOCK) {
    __sync_lock_release(&spinlock);
  }
  return NULL;
}


/*! Adds an integer to a pointer */
void add(long long *pointer, long long value) {
  long long sum = *pointer + value;
  if (opt_yield)
    sched_yield();
  *pointer = sum;
}

void add_compare_and_swap(long long *pointer, long long value) {
  long long sum, prev;
  do {
    prev = *pointer;
    sum = prev + value;
    if (opt_yield)
      sched_yield();
  } while(__sync_val_compare_and_swap(pointer, prev, sum) != prev);
}

void get_monotonic_time(struct timespec *tp) {
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


long long diff_time(struct timespec* start, struct timespec* end) {
  long long sec_diff = (long long)1E9 * (end->tv_sec - start->tv_sec);
  long long nsec_diff = (long long) (end->tv_nsec - start->tv_nsec);
  return sec_diff + nsec_diff;
}


void process_args(int argc, char* argv[]) {
  int opt, longindex;

  char correct_usage[304] = 
    "Correct usage:\r\n"
    "/lab2_add --threads=# --iterations=# --sync=m|s|c --yield\r\n"
    "--thread     : number of threads used to add\r\n"
    "--iterations : number of iterations add will be run\r\n"
    "--sync       : synchronize with mutex, spinlock, or compare and swap\r\n"
    "--yield      : whether to yield and increase failure rate\r\n\0";
  
  char sync_usage[101] =
    "Sync options are:\r\n"
    "m            : mutex\r\n"
    "s            : spin-lock\r\n"
    "c            : compare and swap\r\n\0";

  /* default values */
  num_threads = 1;
  num_iterations = 1;
  opt_yield = 0;
  sync_opt = UNSYNCED;

  while(1) {
    longindex =0;
    static struct option longopt[] = {
      {"threads"    , required_argument, 0, 't' },
      {"iterations" , required_argument, 0, 'i' },
      {"yield"      , no_argument      , 0, 'y' },
      {"sync"       , required_argument, 0, 's' },
      {0            , 0                , 0,  0  }
    };
    opt = getopt_long(argc, argv, "", longopt, &longindex);

    if (opt == -1)
      break;

    switch (opt) {
    case 't':
      num_threads = atoi(optarg);
      break;
    case 'i':
      num_iterations = atol(optarg);
      break;
    case 'y':
      opt_yield = 1;
      break;
    case 's':
      switch (*optarg) {
      case 'm':
	sync_opt = SYNC_MUTEX;
	break;
      case 's':
	sync_opt = SYNC_SPIN_LOCK;
	break;
      case 'c':
	sync_opt = SYNC_COMPARE_SWAP;
	break;
      default:
	fprintf(stderr, sync_usage);
	exit(1);
      }
      break;
    default:
      fprintf(stderr, correct_usage);
      exit(1);
    }
  }
  if (optind < argc) {
    fprintf(stderr, "Invalid arguments: ");
    while (optind < argc)
      fprintf(stderr, "%s ", argv[optind++]);
    fprintf(stderr, ". %s", correct_usage);
    exit(1);
  }
}


void create_threads(pthread_t* threads) {
  int t;
  thread_id = calloc(num_threads, sizeof(int));
  for (t = 0; t < num_threads; t++) {
    thread_id[t] = t;
  }
  for (t = 0; t < num_threads; t++) {
    if (pthread_create(&threads[t], NULL, add_wrapper, &thread_id[t]) != 0) {
      fprintf(stderr, "On thread %d: ", t+1);
      switch (errno) {
      case EAGAIN:
	fprintf(stderr, 
		"Insufficient resources to create another thread.\r\n");
	break;
      case EINVAL:
	fprintf(stderr,
		"Invalid settings in attr.\r\n");
	break;
      case EPERM:
	fprintf(stderr,
		"No permission to set parameters specificed in attr.\r\n");
	break;
      default:
	fprintf(stderr,
		"Miscellaneous thread create error.\r\n%s\r\n",
		strerror(errno));
      }
      exit(2);
    }
  }
}


void join_threads(pthread_t* threads) {
  int t;
  for (t = 0; t < num_threads; t++) {
    if (pthread_join(threads[t], NULL) != 0) {
      fprintf(stderr, "On thread %d: ", t+1);
      switch (errno) {
      case EDEADLK:
	fprintf(stderr, 
		"A deadlock was detected.\r\n");
	break;
      case EINVAL:
	fprintf(stderr,
		"Thread %d is not a joinable thread.\r\n", t);
	break;
      case ESRCH:
	fprintf(stderr,
		"No thread with the ID %d could be found.\r\n", t);
	break;
      default:
	fprintf(stderr,
		"Miscellaneous thread join error.\r\n%s\r\n",
		strerror(errno));
      }
      exit(2);
    }
  }
  free(thread_id);
}


void append_csv(long long run_time) {
  int fd = open("lab2_add.csv", O_CREAT|O_RDWR|O_APPEND, 0644);
  if (fd == -1) {
    switch(errno) {
    case EACCES:
      fprintf(stderr,
	      "The requested access to file is not allowed.\r\n");
      break;
    case EDQUOT:
      fprintf(stderr,
	      "The quota of disk blocks on filesystem has been "
	      "exhausted.\r\n");
      break;
    case EFAULT:
      fprintf(stderr,
	      "The file points outside your accessible address space.\r\n");
      break;
    case EINTR:
      fprintf(stderr,
	      "The call was interrupted by a signal handler.\r\n");
      break;
    case EOVERFLOW:
      fprintf(stderr,
	      "The file is too large to be opened.\r\n");
      break;
    case EROFS:
      fprintf(stderr,
	      "The file is read-only.\r\n");
      break;
    default:
      fprintf(stderr,
	      "Miscellaneous file error.\r\n%s\r\n",
	      strerror(errno));
    }
    exit(2);
  }

  /* add and minus for each thread for # iterations */
  char* test_name = compute_test_name();
  long num_operations = 2 * num_threads * num_iterations; 
  long long average_time_per_op = run_time / (long long)num_operations;
  char output[64];
  int num_chars;
  num_chars = sprintf(output, "%s,%d,%ld,%ld,%lld,%lld,%ld\n",
	  test_name,
	  num_threads,
	  num_iterations,
	  num_operations,
	  run_time,
	  average_time_per_op,
	  counter);
  
  if (num_chars == -1) {
    fprintf(stderr, 
	    "An error occurred in building the string for output.\r\n%s\r\n",
	    strerror(errno));
    exit(2);
  }

  if (write(fd, output, num_chars) == -1) {
    switch (errno) {
    case EAGAIN:
      fprintf(stderr,
	      "The file descriptor for writing has been marked "
	      "nonblocking.\r\n");
      break;
    case EBADF:
      fprintf(stderr,
	      "The file descriptor is not valid or open for writing.\r\n");
      break;
    case EDQUOT:
      fprintf(stderr,
	      "The quota of disk blocks on filesystem has been "
	      "exhausted.\r\n");
      break;
    case EFAULT:
      fprintf(stderr,
	      "Buffer is outside accessible address space.\r\n");
      break;
    case EINTR:
      fprintf(stderr,
	      "The call was interrupted by a signal before any data was "
	      "written.\r\n");
      break;
    case EINVAL:
      fprintf(stderr,
	      "The file is unsuitable for writing, or is misaligned.\r\n");
      break;
    default:
      fprintf(stderr,
	      "Miscellaneous writing error.\r\n%s\r\n",
	      strerror(errno));
    }
    exit(2);
  }
  fprintf(stdout, output);
  close(fd);
}


char* compute_test_name(void) {
  static char str_result[16];
  memset(str_result, 0, 16);
  if (opt_yield == 0) {
    switch(sync_opt) {
    case UNSYNCED:
      strcpy(str_result, "add-none");
      break;
    case SYNC_MUTEX:
      strcpy(str_result, "add-m");
      break;
    case SYNC_SPIN_LOCK:
      strcpy(str_result, "add-s");
      break;
    case SYNC_COMPARE_SWAP:
      strcpy(str_result, "add-c");
    }
  }
  else { /* opt_yield == 1 */
    switch(sync_opt) {
    case UNSYNCED:
      strcpy(str_result, "add-yield-none");
      break;
    case SYNC_MUTEX:
      strcpy(str_result, "add-yield-m");
      break;
    case SYNC_SPIN_LOCK:
      strcpy(str_result, "add-yield-s");
      break;
    case SYNC_COMPARE_SWAP:
      strcpy(str_result, "add-yield-c");
    }
  }
  return str_result;
}
