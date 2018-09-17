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
#include <signal.h>
#include "SortedList.h"

/* program parameter values */
int num_threads;
long num_iterations;
long num_elements;
char str_sync[5];
char str_yield[5];

const int KEY_BITS = 128;
SortedList_t list = { NULL, NULL, NULL };
SortedListElement_t *list_elements;
int *thread_id;
int file_fd;
int list_deleted = 0;

/* function declarations */
static void* list_operations(void*);
void get_monotonic_time(struct timespec*);
long long diff_time(struct timespec*, struct timespec*);
void process_args(int, char**);
void randomize_list_elements(int);
void create_threads(pthread_t*);
void join_threads(pthread_t*);
void check_correct_list_length(int);
int delete_list(void);
void append_csv(long long);
char* compute_test_name(void);
void sighandler(int);
void cleanup(void);


int main(int argc, char *argv[]) {
  struct timespec start_time, end_time;
  pthread_t *threads;
  long long diff;

  /* handle segmantation faults */
  struct sigaction act_h;
  struct sigaction old_act;
  memset(&act_h, 0, sizeof(act_h));
  act_h.sa_handler = sighandler;
  sigaction(SIGSEGV, &act_h, &old_act);

  /* clean up if any errors */
  if (atexit(cleanup) != 0) {
    fprintf(stderr, "Cleanup at exit cannot be registered. Exiting now.\r\n");
    exit(2);
  }

  process_args(argc, argv);
  threads = calloc(num_threads, sizeof(pthread_t));
  randomize_list_elements(time(NULL));
  get_monotonic_time(&start_time);
  create_threads(threads);
  join_threads(threads);
  destroy_mutex();
  check_correct_list_length(1);
  get_monotonic_time(&end_time);
  diff = diff_time(&start_time, &end_time);
  list_deleted = delete_list();
  append_csv(diff);
  free(threads);

  exit(0);
}


/*! Function to be used by pthread */
static void* list_operations(void* thread_id) {
  int id = *((int*) thread_id);
  long start_index = id * num_iterations;
  long end_index = ((id+1) * num_iterations) - 1;
  SortedListElement_t *matching;
  long n;

  /* insert elements to list */
  for (n = start_index; n <= end_index; n++) {
    SortedList_insert(&list, (SortedListElement_t*)(list_elements + n));
  }

  /* get list length */
  check_correct_list_length(0);

  /* delete elements from list */
  for (n = start_index; n <= end_index; n++) {
    matching = SortedList_lookup(&list, 
			 ((SortedListElement_t*)(list_elements + n))->key);

    if (matching == NULL) {
      fprintf(stderr, "No matching element found during list lookup.\r\n");
      exit(2);
    }
    if (SortedList_delete(matching) == 1) {
      fprintf(stderr, "List was corrupted during 'delete' operation.\r\n");
      exit(2);
    }
  }

  return NULL;
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
  long long sec_diff = (long long) 1E9 * (end->tv_sec - start->tv_sec);
  long long nsec_diff = (long long) (end->tv_nsec - start->tv_nsec);
  return sec_diff + nsec_diff;
}


void process_args(int argc, char* argv[]) {
  int opt, longindex;

  char correct_usage[289] = 
    "Correct usage:\r\n"
    "/lab2_add --threads=# --iterations=# --sync=m|s --yield=[idl]\r\n"
    "--thread     : number of threads used to add\r\n"
    "--iterations : number of iterations add will be run\r\n"
    "--sync       : synchronize with mutex or spinlock\r\n"
    "--yield      : whether to yield and increase failure rate\r\n\0";
  
  char sync_usage[101] =
    "Sync options are:\r\n"
    "m            : mutex\r\n"
    "s            : spin-lock\r\n\0";

  char yield_usage[96] =
    "Yield options are: [idl]\r\n"
    "i            : insert\r\n"
    "d            : delete\r\n"
    "l            : lookup\r\n\0";

  /* default values */
  num_threads = 1;
  num_iterations = 1;
  opt_yield = 0;
  strcpy(str_sync, "none\0");
  strcpy(str_yield, "none\0");

  while(1) {
    longindex =0;
    static struct option longopt[] = {
      {"threads"    , required_argument, 0, 't' },
      {"iterations" , required_argument, 0, 'i' },
      {"yield"      , required_argument, 0, 'y' },
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
      if (yield_by(optarg) == 1) {
	fprintf(stderr, yield_usage);
	exit(1);
      }
      strncpy(str_yield, optarg, 5);
      break;
    case 's':
      if (sync_by(*optarg) == 1) {
	fprintf(stderr, sync_usage);
	exit(1);
      }
      strncpy(str_sync, optarg, 5);
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
  num_elements = num_threads * num_iterations;
  limit_iterations(num_elements);
}


void randomize_list_elements(int seed) {
  int element_size = sizeof(SortedListElement_t);
  list_elements = malloc(num_elements * element_size);
  srand(seed);
  int n, m;
  char key[129];
  memset(key, 0, 129);
  for (n = 0; n < num_elements; n++) {
    for (m = 0; m < KEY_BITS; m++) {
      /* 32-127 are visible ascii characters */
      key[m] = (rand() % 95) + 32; 
    }
    SortedListElement_t element = { NULL, NULL, strdup(key) };
    list_elements[n] = element;
  }
}


void create_threads(pthread_t* threads) {
  int t;
  thread_id = calloc(num_threads, sizeof(int));
  for (t = 0; t < num_threads; t++) {
    thread_id[t] = t;
  }
  for (t = 0; t < num_threads; t++) {
    if (pthread_create(&threads[t], NULL, list_operations, 
		       &thread_id[t]) != 0) {
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


void check_correct_list_length(int enforce) {
  if (SortedList_length(&list) == -1 && enforce == 1) {
    fprintf(stderr, "List was corrupted during 'length' operation.\r\n");
    exit(2);
  }
}


int delete_list(void) {
  int n;
  SortedListElement_t *current;
  if (list_elements != NULL) {
    for (n = 0; n < num_elements; n++) {
      free((void*)list_elements[n].key);
      list_elements[n].prev = NULL;
      list_elements[n].next = NULL;
    }
  free(list_elements);
  }
  return 1;
}


void append_csv(long long run_time) {
  file_fd = open("lab2_list.csv", O_CREAT|O_RDWR|O_APPEND, 0644);
  if (file_fd == -1) {
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

  /* insert, lookup, delete for each thread for # iterations */
  char* test_name = compute_test_name();
  long num_operations = 3 * num_threads * num_iterations;
  int num_lists = 1;
  long long average_time_per_op = run_time / (long long)num_operations;
  char output[80];
  int num_chars;
  num_chars = sprintf(output, "%s,%d,%ld,%d,%ld,%lld,%ld\n",
	  test_name,
	  num_threads,
	  num_iterations,
	  num_lists,
	  num_operations,
	  run_time,
	  average_time_per_op);
  
  if (num_chars == -1) {
    fprintf(stderr, 
	    "An error occurred in building the string for output.\r\n%s\r\n",
	    strerror(errno));
    exit(2);
  }

  if (write(file_fd, output, num_chars) == -1) {
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
  close(file_fd);
}


char* compute_test_name(void) {
  static char str_result[16];
  memset(str_result, 0, 16);
  sprintf(str_result, "list-%s-%s", str_yield, str_sync);
  return str_result;
}


void sighandler(int signum) {
  fprintf(stderr, "Segmentation fault caught: --thread=%d --iter=%d --yield=%s --sync=%s\r\n", num_threads, num_iterations, str_yield, str_sync);
  exit(2);
}


void cleanup(void) {
  if (file_fd != 0) close(file_fd);
  if (list_deleted == 0) delete_list();
}
