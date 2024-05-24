#ifndef CLOCK_SIGNAL_DOT_H
#define CLOCK_SIGNAL_DOT_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif 


#define CLOCK_MAX_USERS 48 * 2


enum arc_clock_sync_e {
   ARC_CLOCK_SYNC_READ = 0,
   ARC_CLOCK_SYNC_WRITE
};

struct arc_clock_sync_t
{
   pthread_mutex_t lock;
   int running;
   int count;
   pthread_t tid;
   int interval;
   void *ref[CLOCK_MAX_USERS];
   int type[CLOCK_MAX_USERS];
   pthread_cond_t cond[CLOCK_MAX_USERS];
   char freeme;
};


struct arc_clock_sync_t *arc_clock_sync_init (struct arc_clock_sync_t *cs, int interval);

void arc_clock_sync_free (struct arc_clock_sync_t *cs);

int arc_clock_sync_add (struct arc_clock_sync_t *cs, void *ref, int type);

int arc_clock_sync_del (struct arc_clock_sync_t *cs, void *ref);

void arc_clock_sync_wait (struct arc_clock_sync_t *cs, void *ref, int sec, int milli);

void *arc_clock_sync_signal (void *arg);

#ifdef __cplusplus
}
#endif 

#endif
