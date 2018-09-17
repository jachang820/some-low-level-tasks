/*
 * NAME: Jonathan Chang
 * EMAIL: j.a.chang820@gmail.com
 * ID: 104853981
 */

#include "SortedList.h"
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "PreciseTimer.h"
#include "ListInfo.h"

enum sync_options {UNSYNCED, MUTEX, SPINLOCK} sync_opt = UNSYNCED;
pthread_mutex_t *mutex;
int *spinlock;
int opt_yield = 0;
long num_elements = (long)1E7;


int yield_by(char* yield) {
  int t = 0;
  while (yield[t] != '\0' && t < 3) {
    switch(yield[t]) {
    case 'i':
      opt_yield |= INSERT_YIELD;
      break;
    case 'd':
      opt_yield |= DELETE_YIELD;
      break;
    case 'l':
      opt_yield |= LOOKUP_YIELD;
      break;
    default:
      return 1; /* error */
    }
    t++;
  }
  if (t > 3) return 1; /* error */
  return 0;
}

int sync_by(char sync) {
  if (sync == 'm') {
    sync_opt = MUTEX;
  }
  else if (sync == 's') {
    sync_opt = SPINLOCK;
  }
  else {
    return 1; /* error */
  }
  return 0;
}

void initialize_sync(int sublists) {
  int n;
  num_lists = sublists;
  mutex = malloc(num_lists * sizeof(pthread_mutex_t));
  spinlock = malloc(num_lists * sizeof(int));
  for (n = 0; n < num_lists; n++) {
    pthread_mutex_init(&mutex[n], NULL);
    spinlock[n] = 0;
  }
}

void destroy_sync() {
  int n;
  for (n = 0; n < num_lists; n++) {
    pthread_mutex_destroy(&mutex[n]);
  }
  free(mutex);
  free(spinlock);
}

void limit_iterations(long elements) {
  num_elements = elements;
}

void set_lock(int bin, long long *lock_time) {
  struct PreciseTimer timer;
  PreciseTimer_start(&timer);
  if (sync_opt == MUTEX) pthread_mutex_lock(&mutex[bin]);
  if (sync_opt == SPINLOCK)
    while (__sync_lock_test_and_set(&spinlock[bin], 1))
      ;
  PreciseTimer_end(&timer);
  if (sync_opt == UNSYNCED) {
    *lock_time = 0;
  }
  else {
    *lock_time = timer.diff;
  }
}

void release_lock(int bin) {
  if (sync_opt == MUTEX) pthread_mutex_unlock(&mutex[bin]);
  if (sync_opt == SPINLOCK) __sync_lock_release(&spinlock[bin]);
}

void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
  struct ListInfo *sList = (struct ListInfo*) list;
  SortedListElement_t *it = (SortedListElement_t*) sList->list_obj;
  int limiter = 0;
  int bin = sList->bin;

  set_lock(bin, &(sList->timer));

  while (it->next != NULL && strcmp(it->next->key, element->key) < 0) {
    if (limiter >= num_elements) break; /* prevent infinite loops */
    it = it->next;
    limiter++;
  }

  if (opt_yield & INSERT_YIELD)
    sched_yield();

  if (limiter <= num_elements) {
    if (it->next == NULL || limiter == num_elements) { /* end of list */
      element->next = NULL; 
    }
    else {
      it->next->prev = element;
      element->next = it->next;
    }
    element->prev = it;
    it->next = element;
  }

  release_lock(bin);
}


int SortedList_delete(SortedListElement_t *element) {
  struct ListInfo *sList = (struct ListInfo*) element;
  SortedListElement_t *el = (SortedListElement_t*) sList->list_obj;
  int bin = sList->bin;
  set_lock(bin, &(sList->timer));

  if (el->prev == NULL)            /* head cannot be deleted or */
    return 1;
  if (el->prev->next != el)        /* list is corrupted */
    return 1;
  if (el->next != NULL &&
      el->next->prev != el)
    return 1;

  if (opt_yield & DELETE_YIELD)
    sched_yield();

  if (el->next == NULL) {          /* last element in list       */
    el->prev->next = NULL;
  }
  else {
    el->prev->next = el->next;
    el->next->prev = el->prev;
  }

  release_lock(bin);

  el->next = NULL;
  el->prev = NULL;
  return 0;
}


SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
  struct ListInfo *sList = (struct ListInfo*) list;
  SortedListElement_t *it = (SortedListElement_t*) sList->list_obj;
  int bin = sList->bin;
  SortedListElement_t *result;
  int limiter = 0;

  set_lock(bin, &(sList->timer));

  while (it->next != NULL && strcmp(it->next->key, key) != 0) {
    if (limiter >= num_elements) break; /* prevent infinite loops */
    it = it->next;
    limiter++;
  }

  if (opt_yield & LOOKUP_YIELD)
    sched_yield();

  if (limiter <= num_elements) {
    if (it->next == NULL || limiter == num_elements) 
      result = NULL;
    else result = it->next;
  }
  else result = NULL;

  release_lock(bin);

  return result;
}


int SortedList_length(SortedList_t *list) {
  struct ListInfo *sList = (struct ListInfo*) list;
  SortedListElement_t *it = (SortedListElement_t*) sList->list_obj;
  int bin = sList->bin;
  int limiter = 0;

  if (opt_yield & LOOKUP_YIELD)
    sched_yield();

  set_lock(bin, &(sList->timer));

  while (it->next != NULL) {
    if (it->next->prev != it) { /* list corrupted */
      limiter = -1;
      break;
    }
    if (limiter >= num_elements ) { /* prevent infinite loop */
      limiter = -1;
      break;
    }
    limiter++;
    it = it->next;
  }

  release_lock(bin);

  return limiter;
}
