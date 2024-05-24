#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/types.h>
#include <time.h>           // for clock_gettime()
#include <errno.h>

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

//static int rtpSleep_orig (int millisec, struct timeb *zLastSleepTime, int line);
static int rtpSleep_orig (int millisec, struct timespec *zLastSleepTime, int line);


struct arc_clock_sync_t *
arc_clock_sync_init (struct arc_clock_sync_t *cs, int interval)
{

   struct arc_clock_sync_t *rc = NULL;
   int stat;
   int i;

   if (!cs) {
      rc = (struct arc_clock_sync_t *) calloc (1, sizeof (struct arc_clock_sync_t));
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
      pthread_mutex_init (&rc->clock_mutex, NULL);
      for (i = 0; i < CLOCK_MAX_USERS; i++) {
         pthread_cond_init (&rc->cond[i], NULL);
      }
      rc->interval = interval;
      rc->running = 1;

      stat = pthread_create (&rc->tid, NULL, arc_clock_sync_signal, (void *) rc);
      // fprintf(stderr, " %s() stat=%d\n", __func__, stat);
      if (stat != 0) {
         fprintf (stderr, " %s() failed to start thread sync thread\n", __func__);
      }
   }

   return rc;
}


void
arc_clock_sync_free (struct arc_clock_sync_t *cs)
{

   if (cs) {
      cs->running = 0;
      if (cs->freeme) {
         free (cs);
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
         // break;
      }
   }

   pthread_mutex_unlock (&cs->lock);
   return rc;
}

struct timespec
arc_clock_get_timespec (int nanodelay)
{
   struct timeval now;

   long int abstime_ns_large;

   gettimeofday (&now, NULL);

   abstime_ns_large = now.tv_usec * 1000 + nanodelay;

   struct timespec abstime = { now.tv_sec + (abstime_ns_large / 1000000000), abstime_ns_large % 1000000000 };

   return abstime;
}

void
//arc_clock_sync_wait (struct arc_clock_sync_t *cs, void *ref, int milli, struct timeb *last, int type)
arc_clock_sync_wait (struct arc_clock_sync_t *cs, void *ref, int milli, struct timespec *last, int type)
{

   int i;
   int idx = -1;
   struct timespec ts;

   pthread_mutex_lock (&cs->lock);

   for (i = 0; i < CLOCK_MAX_USERS; i++) {
      if (cs->ref[i] != NULL && cs->ref[i] == ref && cs->type[i] == type) {
         idx = i;
         break;
      }
   }

   if (idx == -1) {
      // fprintf (stderr, " %s() reference not found !! calling usleep(%d) by myself\n", __func__, cs->interval * 1000);
      usleep (cs->interval * 1000);
      pthread_mutex_unlock (&cs->lock);
      return;
   }
   pthread_mutex_unlock (&cs->lock);

   pthread_mutex_lock (&cs->clock_mutex);

#if 0
   ts.tv_sec = 0;
   ts.tv_nsec = cs->interval * 1000 * 1000 * 2;
#endif

   ts = arc_clock_get_timespec (cs->interval * 1000 * 1000 * 2);
   pthread_cond_timedwait (&cs->cond[idx], &cs->clock_mutex, &ts);
   // pthread_cond_wait (&cs->cond[idx], &cs->clock_mutex);
   pthread_mutex_unlock (&cs->clock_mutex);

}

void
arc_clock_sync_broadcast (struct arc_clock_sync_t *cs, void *ref, int type)
{
   int i;
   int idx = -1;

   pthread_mutex_lock (&cs->lock);

   for (i = 0; i < CLOCK_MAX_USERS; i++) {
      if (cs->ref[i] != NULL && cs->ref[i] == ref && cs->type[i] == type) {
         idx = i;
         break;
      }
   }

   if (idx == -1) {
      // fprintf (stderr, " %s() reference not found !! calling usleep(%d) by myself\n", __func__, cs->interval * 1000);
      usleep (cs->interval * 1000);
      pthread_mutex_unlock (&cs->lock);
      return;
   }
   pthread_mutex_unlock (&cs->lock);

   pthread_mutex_lock (&cs->clock_mutex);
   pthread_cond_broadcast (&cs->cond[idx]);
   pthread_mutex_unlock (&cs->clock_mutex);
}


int
util_u_sleep (long microSeconds)
{
   struct timeval SleepTime;

   SleepTime.tv_sec = 0;
   SleepTime.tv_usec = microSeconds;

   select (0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &SleepTime);

   return (0);

}                               /*END: int util_sleep */

int
//arc_clock_rtp_sleep (int millisec, struct timeb *zLastSleepTime)
arc_clock_rtp_sleep (int millisec, struct timespec *zLastSleepTime)
{

   rtpSleep_orig (millisec, zLastSleepTime, __LINE__);
   return 0;

}                               /*END: int arc_clock_rtp_sleep() */


///This function is designed for sleeping after sending/receiving RTP packets.
static int
//rtpSleep_orig (int millisec, struct timeb *zLastSleepTime, int line)
rtpSleep_orig (int millisec, struct timespec *zLastSleepTime, int line)
{
   long yIntSleepTime = 0;
   long yIntActualSleepTime = 0;
   long yIntSleepTimeDelta = 0;
   //struct timeb yCurrentTime;
	struct timespec yCTime;

	int rc;

#if 0
   long yIntStartMilliSec;
   long yIntStopMilliSec;
#endif

   //ftime (&yCurrentTime);
	if ((rc = clock_gettime(CLOCK_REALTIME, &yCTime)) < 0 )
	{
		fprintf(stderr, "Error getting the current time.  [%d, %s]\n", errno, strerror(errno));
	}


   //if (zLastSleepTime->time == 0) {
   if (zLastSleepTime->tv_sec == 0) {
      util_u_sleep (millisec * 1000);
      //ftime (zLastSleepTime);
    	if ((rc = clock_gettime(CLOCK_REALTIME, zLastSleepTime)) < 0 )
    	{
        	fprintf(stderr, "Error getting the current time.  [%d, %s]\n", errno, strerror(errno));
    	}
   }
   else {
      long current = 0;
      long last = 0;

      //current = (1000 * yCurrentTime.time) + yCurrentTime.millitm;
      //last = (1000 * zLastSleepTime->time) + zLastSleepTime->millitm;
      current = (1000 * yCTime.tv_sec) + yCTime.tv_nsec;
      last = (1000 * zLastSleepTime->tv_sec) + zLastSleepTime->tv_nsec;

      yIntSleepTime = millisec - (current - last);

      if (yIntSleepTime > 0) {
         util_u_sleep (yIntSleepTime * 1000);
      }

      //ftime (zLastSleepTime);
    	if ((rc = clock_gettime(CLOCK_REALTIME, zLastSleepTime)) < 0 )
    	{
        	fprintf(stderr, "Error getting the current time.  [%d, %s]\n", errno, strerror(errno));
    	}

      //current = (1000 * yCurrentTime.time) + yCurrentTime.millitm;
      //last = (1000 * zLastSleepTime->time) + zLastSleepTime->millitm;
      current = (1000 * yCTime.tv_sec) + yCTime.tv_nsec;
      last = (1000 * zLastSleepTime->tv_sec) + zLastSleepTime->tv_nsec;

      yIntActualSleepTime = last - current;
      yIntSleepTimeDelta = yIntActualSleepTime - yIntSleepTime;

		/**
      if (yIntSleepTimeDelta > 0) {
         if (zLastSleepTime->millitm < yIntSleepTimeDelta) {
            zLastSleepTime->time--;
            zLastSleepTime->millitm += 1000;
         }
         zLastSleepTime->millitm = zLastSleepTime->millitm - yIntSleepTimeDelta;
      }
		**/
      if (yIntSleepTimeDelta > 0) {
         if (zLastSleepTime->tv_nsec < yIntSleepTimeDelta) {
            zLastSleepTime->tv_sec--;
            zLastSleepTime->tv_nsec += 1000;
         }
         zLastSleepTime->tv_nsec = zLastSleepTime->tv_nsec - yIntSleepTimeDelta;
      }

   }

   return 0;

}                               /*END: int rtpSleep() */




void *
arc_clock_sync_signal (void *arg)
{

   int i;
   struct arc_clock_sync_t *cs = (struct arc_clock_sync_t *) arg;
   //struct timeb t;
   //struct timeb last;
	struct timespec t;
	struct timespec last;

	int rc;

#if 0
   unsigned long current_time = 0;
   unsigned long last_time = 0;
   unsigned long sleep_time = 0;
#endif

   memset (&t, 0, sizeof (t));
   memset (&last, 0, sizeof (last));

   while (cs->running) {

#if 0
      if (last.time == 0) {
         usec = cs->interval * (1000 / 2);
         // fprintf(stderr, " %s() sleep set to %lu\n", __func__, usec);
      }
      else {
         current_time = (t.time * 1000) + t.millitm;
         last_time = (last.time * 1000) + last.millitm;
         usec = (cs->interval * 1000) + (current_time - last_time);
         // usec = how long it took 
         sleep_time = usec - (cs->interval * 1000);
         sleep_time = (cs->interval * 1000) - sleep_time;
         fprintf (stderr, " %s() sleep time = %lu\n", __func__, sleep_time);
         if (usec > ((cs->interval * 1000) * 2)) {
            fprintf (stderr, " %s() timevall way too large %lu \n", __func__, usec);
            usec = (cs->interval * (1000 / 2));
         }
         else {
            usec = usec / 2;
         }
         //fprintf(stderr, " %s() sleep set to %lu (current=%lu) (last=%lu)\n", __func__, usec, current_time, last_time);
      }

      //ftime (&t);
    	if ((rc = clock_gettime(CLOCK_REALTIME, &t)) < 0 )
    	{
        	fprintf(stderr, "Error getting the current time.  [%d, %s]\n", errno, strerror(errno));
    	}

      // this is a debugging line 
      //usec = ((cs->interval - 2) * (1000 / 2));
      // delete when the clock is fixed 
#endif

      pthread_mutex_lock (&cs->lock);
      pthread_mutex_lock (&cs->clock_mutex);

      for (i = 0; i < CLOCK_MAX_USERS; i++) {
         if (cs->ref[i] != NULL && cs->type[i] == ARC_CLOCK_SYNC_WRITE) {
            pthread_cond_signal (&cs->cond[i]);
         }
      }
      pthread_mutex_unlock (&cs->clock_mutex);

      //usleep (sleep_time / 2);
      arc_clock_rtp_sleep (10, &last);

      pthread_mutex_lock (&cs->clock_mutex);

      for (i = 0; i < CLOCK_MAX_USERS; i++) {
         if (cs->ref[i] != NULL && cs->type[i] == ARC_CLOCK_SYNC_READ) {
            pthread_cond_signal (&cs->cond[i]);
         }
      }
      pthread_mutex_unlock (&cs->clock_mutex);

      pthread_mutex_unlock (&cs->lock);

      //usleep (sleep_time / 2);
      arc_clock_rtp_sleep (10, &last);

#if 0
      ftime (&last);
      fprintf (stderr, " %s() loop took %lu milliseconds (%d)\n", __func__, ((t.time * 1000) + t.millitm) - ((last.time * 1000) + last.millitm), last.millitm);
#endif

      //ftime (&last);
    	if ((rc = clock_gettime(CLOCK_REALTIME, &last)) < 0 )
    	{
        	fprintf(stderr, "Error getting the current time.  [%d, %s]\n", errno, strerror(errno));
    	}


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
      arc_clock_sync_wait (cs, ref, 20, NULL, ARC_CLOCK_SYNC_READ);
      //fprintf (stderr, " %s() idx=%d pthread_id=%d\n", __func__, idx, (int)pthread_self());
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
      arc_clock_sync_wait (cs, ref, 20, NULL, ARC_CLOCK_SYNC_WRITE);
      //fprintf (stderr, " %s() idx=%d pthread_id=%d\n", __func__, idx, (int)pthread_self());
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

   for (i = 0; i < 10; i++) {
      if (i & 1) {
         pthread_create (&tids[i], NULL, test_thread_read, cs);
      }
      else {
         pthread_create (&tids[i], NULL, test_thread_write, cs);
      }
   }

   while (counter--) {
      sleep (1);
   }

   arc_clock_sync_free (cs);

   return 0;

}


#endif
