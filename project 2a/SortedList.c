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

enum sync_options {UNSYNCED, MUTEX, SPINLOCK} sync_opt = UNSYNCED;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int spinlock = 0;
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

void destroy_mutex() {
  pthread_mutex_destroy(&mutex);
}

void limit_iterations(long elements) {
  num_elements = elements;
}

void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
  SortedListElement_t *it = list;
  int limiter = 0;

  if (sync_opt == MUTEX) pthread_mutex_lock(&mutex);
  if (sync_opt == SPINLOCK)
    while (__sync_lock_test_and_set(&spinlock, 1));

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

  if (sync_opt == MUTEX) pthread_mutex_unlock(&mutex);
  if (sync_opt == SPINLOCK) __sync_lock_release(&spinlock);
}


int SortedList_delete(SortedListElement_t *element) {
  if (sync_opt == MUTEX) pthread_mutex_lock(&mutex);
  if (sync_opt == SPINLOCK)
    while (__sync_lock_test_and_set(&spinlock, 1)); 

  if (element->prev == NULL)            /* head cannot be deleted or */
    return 1;
  if (element->prev->next != element)   /* list is corrupted */
    return 1;
  if (element->next != NULL &&
      element->next->prev != element)
    return 1;

  if (opt_yield & DELETE_YIELD)
    sched_yield();

  if (element->next == NULL) {            /* last element in list       */
    element->prev->next = NULL;
  }
  else {
    element->prev->next = element->next;
    element->next->prev = element->prev;
  }

  if (sync_opt == MUTEX) pthread_mutex_unlock(&mutex);
  if (sync_opt == SPINLOCK) __sync_lock_release(&spinlock);

  element->next = NULL;
  element->prev = NULL;
  return 0;
}


SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
  SortedListElement_t *it = list;
  SortedListElement_t *result;
  int limiter = 0;

  if (sync_opt == MUTEX) pthread_mutex_lock(&mutex);
  if (sync_opt == SPINLOCK)
    while (__sync_lock_test_and_set(&spinlock, 1));

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

  if (sync_opt == MUTEX) pthread_mutex_unlock(&mutex);
  if (sync_opt == SPINLOCK) __sync_lock_release(&spinlock);

  return result;
}


int SortedList_length(SortedList_t *list) {
  SortedListElement_t *it = list;
  int limiter = 0;

  if (opt_yield & LOOKUP_YIELD)
    sched_yield();

  if (sync_opt == MUTEX) pthread_mutex_lock(&mutex);
  if (sync_opt == SPINLOCK)
    while (__sync_lock_test_and_set(&spinlock, 1));

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

  if (sync_opt == MUTEX) pthread_mutex_unlock(&mutex);
  if (sync_opt == SPINLOCK) __sync_lock_release(&spinlock);

  return limiter;
}
