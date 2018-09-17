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
#include "PreciseTimer.h"
#include "ListInfo.h"

/* program parameter values */
int num_threads;
long num_iterations;
long num_elements;
int num_lists = 1;
long long *wait_for_time;
char str_sync[5];
char str_yield[5];

const int KEY_BITS = 128;
const int VISIBLE_ASCII_CHARS = 95;
const int VISIBLE_ASCII_OFFSET = 32;
SortedList_t *list;
SortedListElement_t *list_elements;
int *thread_id;
int file_fd;
int list_deleted = 0;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
int *threads_finished_inserting;
int *threads_finished_deleting;
long long *list_count_total;
int *list_count;


/* function declarations */
int get_bin(const char*);
void set_up_ListInfo(struct ListInfo*, void*, int);
static void* list_operations(void*);
void process_args(int, char**);
void initialize_list();
void randomize_list_elements(int);
void create_threads(pthread_t*);
void join_threads(pthread_t*);
void check_correct_list_length(int, long long*);
int delete_list(void);
void append_csv(long long);
char* compute_test_name(void);
void sighandler(int);
void cleanup(void);


int main(int argc, char *argv[]) {
  struct PreciseTimer timer;
  pthread_t *threads;

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

  long long *dummy;

  process_args(argc, argv);
  threads = calloc(num_threads, sizeof(pthread_t));
  initialize_list();
  randomize_list_elements(time(NULL));
  initialize_sync(num_lists);
  PreciseTimer_start(&timer);
  create_threads(threads);
  join_threads(threads);
  memset(list_count, 0, num_lists);
  check_correct_list_length(1, dummy);
  PreciseTimer_end(&timer);
  destroy_sync();
  pthread_mutex_destroy(&mut);
  list_deleted = delete_list();
  append_csv(timer.diff);
  free(threads);

  exit(0);
}


int get_bin(const char *key) {
  int bin_size = VISIBLE_ASCII_CHARS / num_lists;
  if (VISIBLE_ASCII_CHARS % num_lists != 0) {
    bin_size++;
  }
  int bin = 0;
  while (bin < num_lists) {
    if (*key - VISIBLE_ASCII_CHARS < (bin + 1) * bin_size) {
      return bin;
    }
    else {
      bin++;
    }
  }
  fprintf(stderr, "Invalid key does not fit into a sub list.\r\n");
  exit(2);
}


void set_up_ListInfo(struct ListInfo *sList, void* list_obj, int bin) {
  sList->list_obj = list_obj;
  sList->timer = 0;
  sList->bin = bin;
}


/*! Function to be used by pthread */
static void* list_operations(void* thread_id) {
  int id = *((int*) thread_id);
  long start_index = id * num_iterations;
  long end_index = ((id+1) * num_iterations) - 1;
  long long wait_per_thread_time = 0;
  SortedListElement_t *matching;
  long n;
  int bin;
  struct ListInfo sList;
  /* insert elements to list */
  for (n = start_index; n <= end_index; n++) {
    bin = get_bin(list_elements[n].key);
    set_up_ListInfo(&sList, (void*) &list[bin], bin);
    SortedList_insert((SortedList_t*) &sList, 
		      (SortedListElement_t*)(list_elements + n));
    wait_per_thread_time += sList.timer;
  }
  /* make sure all threads have finished inserting */
  pthread_mutex_lock(&mut);
  (*threads_finished_inserting)++;
  pthread_mutex_unlock(&mut);
  while (*threads_finished_inserting != num_threads)
    ;
  
  /* check list length */
  long long wait_timer;
  check_correct_list_length(0, &wait_timer);
  wait_per_thread_time += wait_timer;

  /* delete elements from list */
  for (n = start_index; n <= end_index; n++) {
    bin = get_bin(list_elements[n].key);
    set_up_ListInfo(&sList, (void*) &list[bin], bin);
    matching = SortedList_lookup((SortedList_t*) &sList, 
			 ((SortedListElement_t*)(list_elements + n))->key);
    wait_per_thread_time += sList.timer;

    if (matching == NULL) {
      fprintf(stderr, "No matching element found during list lookup.\r\n");
      exit(2);
    }
    set_up_ListInfo(&sList, (void*) matching, bin);
    if (SortedList_delete((SortedListElement_t*) &sList) == 1) {
      fprintf(stderr, "List was corrupted during 'delete' operation.\r\n");
      exit(2);
    }
    wait_per_thread_time += sList.timer;
  }

  /* make sure all threads have finished deleting */
  pthread_mutex_lock(&mut);
  (*threads_finished_deleting)++;
  pthread_mutex_unlock(&mut);
  while (*threads_finished_deleting != num_threads)
    ;

  pthread_mutex_lock(&mut);
  long long sum = *wait_for_time + wait_per_thread_time;;
  *wait_for_time = sum;
  pthread_mutex_unlock(&mut);

  return NULL;
}


void process_args(int argc, char* argv[]) {
  int opt, longindex;

  char correct_usage[325] = 
    "Correct usage:\r\n"
    "/lab2_add --threads=# --iterations=# --sync=m|s --yield=[idl]\r\n"
    "--thread     : number of threads used to add\r\n"
    "--iterations : number of iterations add will be run\r\n"
    "--sync       : synchronize with mutex or spinlock\r\n"
    "--yield      : whether to yield and increase failure rate\r\n"
    "--lists      : number of sub lists\r\n\0";
  
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
  num_lists = 1;
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
      {"lists"      , required_argument, 0, 'l' },
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
    case 'l':
      num_lists = atoi(optarg);
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


void initialize_list() {
  int n;
  wait_for_time = calloc(1, sizeof(long long));
  threads_finished_inserting = calloc(1, sizeof(int));
  threads_finished_deleting = calloc(1, sizeof(int));
  list_count_total = calloc(1, sizeof(long long));
  list_count = calloc(num_lists, sizeof(int));
  list = malloc(num_lists * sizeof(SortedList_t));
  for (n = 0; n < num_lists; n++) {
    list[n].prev = NULL;
    list[n].next = NULL;
    list[n].key = NULL;
  }
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
      key[m] = (rand() % VISIBLE_ASCII_CHARS) + VISIBLE_ASCII_OFFSET; 
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


void check_correct_list_length(int enforce, long long *timer) {
  struct ListInfo sList;
  int count;
  int bin;
  for (bin = 0; bin < num_lists; bin++) {
    set_up_ListInfo(&sList, (void*) &list[bin], bin);
    pthread_mutex_lock(&mut);
    if (list_count[bin] == 0) {
      list_count[bin] = 1;
      pthread_mutex_unlock(&mut);
      count = SortedList_length((SortedList_t*) &sList);
      if (count == -1 && enforce == 1) {
	fprintf(stderr, "List was corrupted during 'length' operation.\r\n");
	exit(2);
      }   
      pthread_mutex_lock(&mut);
      *list_count_total += (long long) count;
    }
    pthread_mutex_unlock(&mut);
  }
  *timer = sList.timer;
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
  if (list != NULL) {
    for (n = 0; n < num_lists; n++) {
      list[n].next = NULL;
    }
    free(list);
  }
  if (list_count != NULL) free(list_count);
  if (list_count_total != NULL) free(list_count_total);
  if (threads_finished_inserting != NULL) free(threads_finished_inserting);
  if (threads_finished_deleting != NULL) free(threads_finished_deleting);
  return 1;
}


void append_csv(long long run_time) {
  file_fd = open("lab2b_list.csv", O_CREAT|O_RDWR|O_APPEND, 0644);
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
  long long average_time_per_op = run_time / (long long)num_operations;
  long long wait_time = *wait_for_time / (long long)num_operations;
  char output[80];
  int num_chars;
  num_chars = sprintf(output, "%s,%d,%ld,%d,%ld,%lld,%ld,%lld\n",
	  test_name,
	  num_threads,
	  num_iterations,
	  num_lists,
	  num_operations,
	  run_time,
	  average_time_per_op,
	  wait_time);
  
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
  fprintf(stderr, "Segmentation fault caught: --thread=%d --iter=%d --yield=%s --sync=%s"
	  "--lists=%d\r\n", num_threads, num_iterations, str_yield, str_sync, num_lists);
  exit(2);
}


void cleanup(void) {
  if (file_fd != 0) close(file_fd);
  if (list_deleted == 0) delete_list();
}
