#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <unistd.h>

#include "arc_small_conference.h"
#include "arc_conference_frames.h"

#include <time.h>
#include <sys/time.h>

#define MAX_CONFERENCE_SIZE 5

#define CONF_IN_AUDIO 0
#define CONF_OUT_AUDIO 1

/* 
 * this is a util for looping through an array, while locking to find the conference by name
 * returns 1 if a match occurs
 */


/*
 *  if name == NULL find first empty conference index
 */



int
arc_small_conference_refcount (struct arc_small_conference_t *c)
{

   int rc = -1;

   if (!c) {
      return -1;
   }

   pthread_mutex_lock (&c->lock);
   rc = c->refcount;
   pthread_mutex_unlock (&c->lock);

   return rc;
}


int
arc_small_conference_find (struct arc_small_conference_t *c, char *name)
{

   int rc = 0;

   if (!c || !name) {
      return 0;
   }

   pthread_mutex_lock (&c->lock);

   if (c->name[0] && !strcmp (c->name, name)) {
      rc = 1;
   }

   pthread_mutex_unlock (&c->lock);

   return rc;
}

int
arc_small_conference_find_free (int zCall, struct arc_small_conference_t *c)
{

   time_t currentTime;
   struct timeval tp;
   struct tm *tm;
   char current_time[40];

   int rc = 0;

   pthread_mutex_lock (&c->lock);

   if (c && !c->name[0]) {
      rc = 1;
   }
   else {
      time (&currentTime);
      gettimeofday (&tp, NULL);
      tm = localtime (&tp.tv_sec);
      strftime (current_time, sizeof (current_time) - 1, "%m:%d:%Y %H:%M:%S", tm);

      fprintf (stderr, "%s:%03ld|%d|DJB: c->name(%s) is not empty.\n", current_time, tp.tv_usec / 10000, zCall, c->name);
   }

   pthread_mutex_unlock (&c->lock);

   return rc;
}



int
arc_small_conference_init (struct arc_small_conference_t *c, const char *name)
{

   if (!c || !name) {
      fprintf (stderr, " %s() initial condition were not met\n", __func__);
      return -1;
   }

   memset (c, 0, sizeof (struct arc_small_conference_t));
   pthread_mutex_init (&c->lock, NULL);
   snprintf (c->name, sizeof (c->name), "%s", name);

   //fprintf (stderr, " %s() conference name=%s\n", __func__, name);

   return 0;
}


void
arc_small_conference_free (struct arc_small_conference_t *c)
{

   int i;

   if (!c) {
      return;
   }

   pthread_mutex_lock (&c->lock);

   for (i = 0; i < MAX_CONFERENCE_SIZE; i++) {
      if (c->conf_frames[0][i] != NULL) {
         arc_conference_frame_free (c->conf_frames[0][i]);
      }
      c->conf_frames[0][i] = NULL;

      if (c->conf_frames[1][i] != NULL) {
         arc_conference_frame_free (c->conf_frames[1][i]);
      }
      c->conf_frames[1][i] = NULL;
   }

   memset (c, 0, sizeof (struct arc_small_conference_t));

   c->refcount = 0;

   pthread_mutex_unlock (&c->lock);

   return;
}


struct arc_conference_frame_t *
arc_small_conference_add_ref (struct arc_small_conference_t *c, int opts, int which)
{

   int i;
   struct arc_conference_frame_t *rc = NULL;
   struct arc_conference_frame_t *f = NULL;
   char silence[320];

   if (!c) {
      fprintf (stderr, " %s() failed entry conditions\n", __func__);
      return NULL;
   }

   pthread_mutex_lock (&c->lock);

   for (i = 0; i < MAX_CONFERENCE_SIZE; i++) {
      if (c->conf_frames[which][i] == NULL) {
         f = arc_conference_frame_init (NULL, 320);
         if (f) {
            c->conf_frames[which][i] = f;
            c->opts[which][i] = opts;
            c->seqno[which][i][i] = -1;
            rc = f;
            c->refcount++;
         }
         break;
      }
   }

   memset (silence, 0, sizeof (silence));

   pthread_mutex_unlock (&c->lock);

   return rc;
}

int
arc_small_conference_del_ref (struct arc_small_conference_t *c, struct arc_conference_frame_t *f, int which)
{

   int i;
   int rc = -1;

   pthread_mutex_lock (&c->lock);

   for (i = 0; i < MAX_CONFERENCE_SIZE; i++) {
      if (c->conf_frames[which][i] == f) {
         arc_conference_frame_free (f);
         c->conf_frames[which][i] = NULL;
         c->opts[which][i] = 0;
         c->seqno[which][i][i] = -1;
         c->refcount--;
         if (c->conf_delete_cb[i] != NULL) {
            c->conf_delete_cb[i] (c->conf_delete_data[i]);
            if (c->conf_delete_data[i] != NULL) {
               free (c->conf_delete_data[i]);
               c->conf_delete_data[i] = NULL;
            }
         }
         rc = 0;
         break;
      }
   }

   pthread_mutex_unlock (&c->lock);

   return rc;
}


int
arc_conference_add_delete_cb (struct arc_small_conference_t *c, int idx, int (cb) (void *), void *data)
{

   int rc = -1;

   if (idx < 0 || idx >= MAX_CONFERENCE_SIZE) {
      return rc;
   }

   if (cb == NULL) {
      return rc;
   }

   pthread_mutex_lock (&c->lock);

   c->conf_delete_cb[idx] = cb;
   c->conf_delete_data[idx] = data;

   pthread_mutex_unlock (&c->lock);

   return rc;
}




int
arc_small_conference_post (struct arc_small_conference_t *c, struct arc_conference_frame_t *self, char *buff, size_t buffsize, int opts, int which)
{

   int rc = 0;
   int i;
   int count = 0;
   int applies = 0;
   int seq;
   int idx = -1;

   if (!c || !self || !buff || !buffsize) {
      fprintf (stderr, " %s() initial test conditions for function failed\n", __func__);
      return -1;
   }

   if (which < 0 || which > 1) {
      fprintf (stderr, " %s() invalid which parameter, %d\n", __func__, which);
      return -1;
   }

   pthread_mutex_lock (&c->lock);

   for (i = 0; i < MAX_CONFERENCE_SIZE; i++) {
      if (c->conf_frames[which][i] == self) {
         idx = i;
         break;
      }
   }

   if (idx == -1) {
      pthread_mutex_unlock (&c->lock);
      // fprintf(stderr, " %s() could not get index from ref returning -1\n", __func__);
      return -1;
   }

   for (i = 0; i < MAX_CONFERENCE_SIZE; i++) {
      if (c->conf_frames[which][i] != NULL && c->conf_frames[which][i] != self) {
         seq = arc_conference_frame_post (c->conf_frames[which][i], buff, buffsize, c->seqno[which][idx][i]);
         if (seq != -1) {
            seq++;
         }
         c->seqno[which][idx][i] = seq;
         c->posts[which][i]++;
         applies++;
         count++;
      }
   }

   // fprintf (stderr, " %s() rc=%d count=%d posts=%d applies=%d\n", __func__, rc, count, posts, applies);

   pthread_mutex_unlock (&c->lock);

   return rc;
}


// this is now a non-blocking read 

int
arc_small_conference_read (struct arc_small_conference_t *c, struct arc_conference_frame_t *self, char *buff, size_t bufsize, int which)
{

   int rc = -1;
   int i;
   int count = 0;               // add one for self 

   if (!c || !self || !buff || !bufsize) {
      fprintf (stderr, " %s() test conditions for funcion failed\n", __func__);
      return -1;
   }

   if (which != 0 && which != 1) {
      fprintf (stderr, " %s() invalid which parameter, %d\n", __func__, which);
      return -1;
   }

   pthread_mutex_lock (&c->lock);

   for (i = 0; i < MAX_CONFERENCE_SIZE; i++) {
      if (c->conf_frames[which][i] != NULL) {
         count++;
      }
   }

   if (c->refcount != count) {
      // fprintf(stderr, " %s() refcount and posts do not match, returning -1, refcount = %d, count=%d\n", __func__, c->refcount, count);
      pthread_mutex_unlock (&c->lock);
      return rc;
   }

   for (i = 0; i < MAX_CONFERENCE_SIZE; i++) {
      if (c->conf_frames[which][i] == self) {
         if (c->conf_frames[which][i] != NULL && c->posts[which][i] < (c->refcount - 1)) {
            ;;                  // fprintf (stderr, " %s() posts do not equal refcount posts=%d refcount=%d\n", __func__, c->posts[which][i], c->refcount);
         }
         rc = arc_conference_frame_read (c->conf_frames[which][i], buff, bufsize);
         c->posts[which][i] = 0;
      }
   }

   pthread_mutex_unlock (&c->lock);

   return rc;
}


#ifdef MAIN

#include <unistd.h>

#include <ctype.h>
#include <linux/soundcard.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>


int
audio_init (const char *dev, int channels, int wordsize, int bitrate, int mode)
{

   int fd;
   int status;
   int arg;

   fd = open (dev, mode);

   if (fd > 0) {

      arg = channels;

      status = ioctl (fd, SOUND_PCM_WRITE_CHANNELS, &arg);

      if (status == -1) {
         fprintf (stderr, " %s() failed to set to single channel: reason = %s", __func__, strerror (errno));
         goto error;
      }

      arg = wordsize;

      status = ioctl (fd, SOUND_PCM_WRITE_BITS, &arg);

      if (status == -1) {
         fprintf (stderr, " %s() failed to set word size for audio device reason = %s", __func__, strerror (errno));
         goto error;
      }

      arg = bitrate;

      status = ioctl (fd, SOUND_PCM_WRITE_RATE, &arg);

      if (status == -1) {
         fprintf (stderr, " %s() failed to set bit rate for audio device reason = %s", __func__, strerror (errno));
         goto error;
      }
   }

   return fd;

 error:
   close (fd);
   return -1;
}


/*
  just open two stream files and mix the output 
  and play the mixed audio, then maybe three files 
*/

int
main (int argc, char **argv)
{

   char buff[320];
   int i;
   int rc;

   int files[5] = { -1, -1, -1, -1, -1 };

   int audio_fd;

   struct arc_conference_frame_t *frames[5];
   struct arc_small_conference_t conf;


   if (argc < 2) {
      fprintf (stderr, "\n usage %s file1 file2 file3 ....\n\n", argv[0]);
      exit (1);
   }

   arc_small_conference_init (&conf, "test conference");

   for (i = 0; i < 5; i++) {
      if (argv[i + 1] != NULL) {
         files[i] = open (argv[i + 1], O_RDONLY);
         if (files[i] == -1) {
            fprintf (stderr, " %s() failed to open %s for reading, exiting\n", __func__, argv[i + 1]);
            exit (1);
         }
         frames[i] = arc_small_conference_add_ref (&conf, 0, 0);
         fprintf (stderr, " %s() frames[%d] == %p\n", __func__, i, frames[i]);
      }
      else {
         break;
      }
   }

   audio_fd = audio_init ("/dev/dsp", 1, 16, 8000, O_WRONLY);

   if (audio_fd == -1) {
      fprintf (stderr, " %s() audio fd failed to open\n", __func__);
      exit (1);
   }

   int r = 1000;

   while (r--) {

      if (frames[0] != NULL) {
         rc = arc_small_conference_read (&conf, frames[0], buff, sizeof (buff), 0);
         write (audio_fd, buff, 320);
      }
      else {
         fprintf (stderr, " %s() conference frame[0] == NULL exiting\n", __func__);
         exit (1);
      }

#if 1
      for (i = 0; i < 5; i++) {
         if (files[i] != -1) {
            rc = read (files[i], buff, sizeof (buff));
            if (rc > 0 && (frames[i] != NULL)) {
               //fprintf (stderr, " %s() read %d bytes from fd %d\n", __func__, rc, files[i]);
               //arc_small_conference_post (&conf, NULL, buff, rc, 0, 0);
               arc_small_conference_post (&conf, frames[i], buff, rc, 0, 0);
               // fprintf (stderr, " post[%d]=%d\n", i, rc);
            }
            else if (rc <= 0) {
               close (files[i]);
               files[i] = -1;
               arc_small_conference_del_ref (&conf, frames[i], 0);
               frames[i] == NULL;
            }
         }
      }
#endif

   }

   //arc_small_conference_del_ref (&conf, &frame, ARC_SMALL_CONFERENCE_AUDIO_OUT);
   //arc_small_conference_del_ref (&conf, &frameTwo, ARC_SMALL_CONFERENCE_AUDIO_OUT);

   arc_small_conference_free (&conf);
   // arc_audio_frame_free (&frame);

   return 0;

}

#endif
