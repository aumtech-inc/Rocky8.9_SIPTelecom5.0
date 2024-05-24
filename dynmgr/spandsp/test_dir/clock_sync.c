#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "clock_sync.h"

#if 0

struct arc_clock_sync_t
{
   pthread_mutex_t lock;
   int count;
   int running;
   int precision;
   int interval;
   void *ref[CLOCK_MAX_USERS];
   pthread_cond_t cond[CLOCK_MAX_USERS];
   char freeme;
};

#endif


struct arc_clock_sync_t *
arc_clock_sync_init (struct arc_clock_sync_t *cs, int interval)
{

   struct arc_clock_sync_t *rc = NULL;
   int stat;
   int i;

   if (!cs) {
      rc = (struct arc_clock_sync_t *)calloc (1, sizeof (struct arc_clock_sync_t));
      if (rc) {
         rc->freeme++;
      }
   }
   else {
      memset (cs, 0, sizeof (struct arc_clock_sync_t));
      rc = cs;
   }

   if (rc) {
      pthread_mutex_init (&rc->lock, NULL);
      for (i = 0; i < CLOCK_MAX_USERS; i++) {
         pthread_cond_init (&rc->cond[i], NULL);
      }
      rc->interval = interval;
      rc->running = 1;

      stat = pthread_create(&rc->tid, NULL, arc_clock_sync_signal, (void *)rc);
      // fprintf(stderr, " %s() stat=%d\n", __func__, stat);
      if(stat != 0){
         fprintf(stderr, " %s() failed to start thread sync thread\n", __func__);
      }
   }

   return rc;
}


void
arc_clock_sync_free (struct arc_clock_sync_t *cs){

  if(cs){
     cs->running = 0;
     if(cs->freeme){
       free(cs);
     }
  }

  return;
}


int
arc_clock_sync_add (struct arc_clock_sync_t *cs, void *ref, int type)
{

   int rc = -1;
   int i;

   if (!ref) {
      return -1;
   }

   pthread_mutex_lock (&cs->lock);

   for (i = 0; i < CLOCK_MAX_USERS; i++) {
      if (cs->ref[i] == NULL) {
         cs->ref[i] = ref;
         cs->type[i] = type;
         rc = i;
         break;
      }
   }

   if (rc != -1) {
      cs->count++;
   }

   pthread_mutex_unlock (&cs->lock);

   return rc;
}

int
arc_clock_sync_del (struct arc_clock_sync_t *cs, void *ref)
{


   int rc = -1;
   int i;

   pthread_mutex_lock (&cs->lock);

   for (i = 0; i < CLOCK_MAX_USERS; i++) {
      if (cs->ref[i] == ref) {
         cs->count--;
         cs->ref[i] = NULL;
         rc = i;
         break;
      }
   }

   pthread_mutex_unlock (&cs->lock);
   return rc;
}

void
arc_clock_sync_wait (struct arc_clock_sync_t *cs, void *ref, int sec, int milli)
{

   int i;
   int idx = -1;

   pthread_mutex_lock (&cs->lock);

   for (i = 0; i < CLOCK_MAX_USERS; i++) {
      if (cs->ref[i] != NULL && cs->ref[i] == ref) {
         idx = i;
         break;
      }
   }

   if (idx == -1) {
      fprintf (stderr, " %s() reference not found !!\n", __func__);
      pthread_mutex_unlock (&cs->lock);
      return;
   }

   pthread_cond_wait (&cs->cond[idx], &cs->lock);
   pthread_mutex_unlock (&cs->lock);

}

void *
arc_clock_sync_signal (void *arg)
{

   int i;
   struct arc_clock_sync_t *cs = (struct arc_clock_sync_t *) arg;
   int mode;

   while (cs->running) {

      pthread_mutex_lock (&cs->lock);

      for (i = 0; i < CLOCK_MAX_USERS; i++) {
         if (cs->ref[i] != NULL && cs->type[i] == ARC_CLOCK_SYNC_READ) {
            pthread_cond_signal (&cs->cond[i]);
         }
      }

      for (i = 0; i < CLOCK_MAX_USERS; i++) {
         if (cs->ref[i] != NULL && cs->type[i] == ARC_CLOCK_SYNC_WRITE) {
            pthread_cond_signal (&cs->cond[i]);
         }
      }


      pthread_mutex_unlock (&cs->lock);
      usleep (cs->interval * 1000);
   }

   return NULL;

}


#ifdef MAIN

void *
test_thread_read (void *arg)
{

   struct arc_clock_sync_t *cs = (struct arc_clock_sync_t *) arg;
   int idx;

   void *ref = calloc (1, 10);

   idx = arc_clock_sync_add (cs, ref, ARC_CLOCK_SYNC_READ);

   while (1) {
      arc_clock_sync_wait (cs, ref, 1, 1);
      fprintf (stderr, " %s() idx=%d pthread_id=%d\n", __func__, idx, pthread_self());
   }

   return NULL;
}


void *
test_thread_write (void *arg)
{

   struct arc_clock_sync_t *cs = (struct arc_clock_sync_t *) arg;
   int idx;

   void *ref = calloc (1, 10);

   idx = arc_clock_sync_add (cs, ref, ARC_CLOCK_SYNC_WRITE);

   while (1) {
      arc_clock_sync_wait (cs, ref, 1, 1);
      fprintf (stderr, " %s() idx=%d pthread_id=%d\n", __func__, idx, pthread_self());
   }

   return NULL;
}



int
main ()
{

   int i;
   pthread_t tids[CLOCK_MAX_USERS];
   pthread_t tid;
   int counter = 10;

   struct arc_clock_sync_t *cs;

   cs = arc_clock_sync_init (NULL, 20);

   for (i = 0; i < 6; i++) {
      if(i & 1){
         pthread_create (&tids[i], NULL, test_thread_read, cs);
      } else {
         pthread_create (&tids[i], NULL, test_thread_write, cs);
      }
   }

   while (counter--) {
      sleep (1);
   }

   arc_clock_sync_free(cs);

   return 0;

}


#endif
