#include <pthread.h>
#include <stdio.h>


/*
  $Id: arc_audio_frame.c,v 1.4 2010/12/15 18:52:32 devmaul Exp $ 
*/

#define _XOPEN_SOURCE 500

#include <sys/types.h>
#include <sys/poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "arc_audio_frame.h"

#define SLIN16_ADD_SIGNED_BIT(val) (val &= 0x8000)
#define SLIN16_DEL_SIGNED_BIT(val) (val &= 0x7fff)

#define INITIAL_READ_INDEX 0
#define INITIAL_APPLY_INDEX 1
#define INITIAL_WRITE_INDEX 2

static int
slin16_gen_silence (char *buff, size_t size)
{

   short sbuff[] = { 0, 1, 1, 0, 0, -1, -1, 0 };
   short *ptr = (short *) buff;
   int i, j = 0;

   if (!buff || !size) {
      return -1;
   }

   for (i = 0; i < size; i++, i++) {
      ptr = (short *) &buff[i];
      *ptr = sbuff[j];
      j++;
      j &= 8 - 1;
   }

   return size;
}


static void *
slin16_setup_silence_gen (void *parms)
{

// fix logic = bad sequencing here 

   struct arc_audio_silence_parms_t *rc = NULL;
   struct arc_audio_silence_parms_t *p = (struct arc_audio_silence_parms_t *) parms;

   if (p) {
      if (p->len) {
         p->sbuffer = calloc (1, p->len);
         if (p->sbuffer) {
            slin16_gen_silence (p->sbuffer, p->len);
            rc = calloc (1, sizeof (struct arc_audio_silence_parms_t));
            if (rc) {
               memcpy (rc, p, sizeof (struct arc_audio_silence_parms_t));
            }
         }
      }
   }
   return rc;
}

static int
slin16_do_silence_gen (struct arc_audio_frame_t *f, int idx, char *buf, size_t size, void *context)
{

   int rc = -1;
   struct arc_audio_silence_parms_t *p = (struct arc_audio_silence_parms_t *) context;

   if (p->len == size) {
      memcpy (f->ringbuff[idx], p->sbuffer, size);
      rc = size;
   }

   return rc;
}



//
// dont forget to free alloced buffer for silince data 
//


void
slin16_destroy_silence_gen (void *context)
{

   struct arc_audio_silence_parms_t *cntx;

   if (context != NULL) {

      cntx = context;

      if (cntx->sbuffer) {
         free (cntx->sbuffer);
      }

      free (cntx);
   }
   return;
}


static void *
slin16_setup_audio_mixing (void *parms)
{
   struct arc_audio_mixing_parms_t *rc;

   rc = calloc (1, sizeof (struct arc_audio_mixing_parms_t));

   if (rc != NULL) {
      memcpy (rc, parms, sizeof (struct arc_audio_mixing_parms_t));
   }

   return rc;
}

static int
slin16_do_audio_mixing (struct arc_audio_frame_t *f, int idx, char *buf, size_t size, void *context)
{

   int rc = -1;
   int samples;
   short *ptra;
   short *ptrb;
   int val;
   int divisor = 32767;
   struct arc_audio_mixing_parms_t *p = (struct arc_audio_mixing_parms_t *) context;

   int i;

   if (!buf || size <= 0) {
      return -1;
   }

   samples = size / 2;

   for (i = 0; i < size; i++, i++) {

      ptra = (short *) &f->ringbuff[idx][i];
      ptrb = (short *) &buf[i];

      if (*ptra == 0 || *ptrb == 0) {
         val = *ptra + *ptrb;
      }
      else if (IS_SIGNED_16BIT_INT (*ptra) && IS_SIGNED_16BIT_INT (*ptrb)) {
         //fprintf(stderr, " %s() a=%d b=%d\n", __func__, *ptra, *ptrb);
         val = (*ptra + *ptrb) - (*ptra * *ptrb) / -divisor;
      }
      else if (!IS_SIGNED_16BIT_INT (*ptra) && !IS_SIGNED_16BIT_INT (*ptrb)) {
         //fprintf(stderr, " %s() a=%d b=%d\n", __func__, *ptra, *ptrb);
         val = (*ptra + *ptrb) - (*ptra * *ptrb) / divisor;
      }
      else {
         val = *ptra + *ptrb;
      }

      if (p && p->coef) {
         val *= p->coef;
         if (abs (val) > divisor) {
            p->coef *= .95;
            //fprintf(stderr, " %s() p->coef=%f\n", __func__, p->coef);
         }
      }
      *ptra = val;
   }

   return rc;
}





#if 0

//
// a / b comparison for audio mixing routines 
//


static int
slin16_do_audio_mixing (struct arc_audio_frame_t *f, int idx, char *buf, size_t size, void *context)
{

   int samples;
   short *ptra;
   short *ptrb;
   int val;
   int divisor = 26214;
   struct arc_audio_mixing_parms_t *p = (struct arc_audio_mixing_parms_t *) context;

   int i;

   if (!buf || size <= 0) {
      return -1;
   }

   samples = size / 2;

   for (i = 0; i < size; i++, i++) {

      ptra = (short *) &f->ringbuff[idx][i];
      ptrb = (short *) &buf[i];

      if (IS_SIGNED_16BIT_INT (*ptra) && IS_SIGNED_16BIT_INT (*ptrb)) {
         //fprintf(stderr, " %s() a=%d b=%d\n", __func__, *ptra, *ptrb);
         val = ((*ptra + *ptrb) - (*ptra * *ptrb)) & ((1 << 15) - 1);
         *ptra = val;
         SLIN16_ADD_SIGNED_BIT (*ptra);
      }
      else if (!IS_SIGNED_16BIT_INT (*ptra) && !IS_SIGNED_16BIT_INT (*ptrb)) {
         //fprintf(stderr, " %s() a=%d b=%d\n", __func__, *ptra, *ptrb);
         val = ((*ptra + *ptrb) - (*ptra * *ptrb)) & ((1 << 15) - 1);
         *ptra = val;
         SLIN16_DEL_SIGNED_BIT (*ptra);
      }
      else {
         *ptra = (*ptra + *ptrb);
      }

      if (p && p->coef) {
         *ptra *= p->coef;
         if (abs (*ptra) > divisor) {
            p->coef *= .95;
            //fprintf(stderr, " %s() p->coef=%f\n", __func__, p->coef);
         }
      }
   }

   return samples;
}

#endif






static void
slin16_destroy_audio_mixing (void *context)
{

   if (context != NULL) {
      free (context);
   }

   return;
}


static void *
slin16_setup_audio_gain_control (void *parms)
{

   void *rc;

   if (!parms) {
      return NULL;
   }

   rc = calloc (1, sizeof (struct arc_audio_gain_parms_t));

   if (rc != NULL) {
      memcpy (rc, parms, sizeof (struct arc_audio_gain_parms_t));
   }


   return rc;
}

static int
slin16_run_audio_gain_control (struct arc_audio_frame_t *f, int idx, char *buff, size_t size, void *context)
{

   int samples = size / 2;
   int i;
   short *val = (short *) buff;
   struct arc_audio_gain_parms_t *c = (struct arc_audio_gain_parms_t *) context;

   if (!buff || !context || !size) {
      return -1;
   }

   // if (c->current == 0.0 || c->current == 1.0) {
   if (c->current == 1.0) {
      c->current = 1.0;
      return samples;
   }

   for (i = 0; i < samples; i++) {
      *val *= c->current;
      if (abs (*val) >= 32700) {
         c->clipping++;
      }
      val++;
   }

   if (c->agc && c->clipping) {
      if (c->step) {
         c->current *= c->step;
         c->clipping = 0;
      }
   }

   return samples;
}

static void
slin16_destroy_audio_gain_control (void *context)
{
   if (context) {
      free (context);
   }
}


int
arc_audio_frame_init (struct arc_audio_frame_t *f, size_t size, int type, int do_lock)
{
   int i;
   int rc = -1;

   if (!f) {
      return -1;
   }

   memset (f, 0, sizeof (struct arc_audio_frame_t));

   f->type = type;
   f->size = size;


   if (do_lock) {
      pthread_mutex_init (&f->lock, NULL);
      pthread_cond_init (&f->cond, NULL);
      f->do_lock++;
   }


   for (i = 0; i < AUDIO_FRAME_RING_BUFF_SIZE; i++) {
      memset (f->ringbuff[i], 0, sizeof (f->ringbuff[i]));
      slin16_gen_silence (f->ringbuff[i], size);
      f->posts[i] = size;
      //f->posts[i] = 0;
      f->used[i] = 0;
   }


   f->idx[WRITE_IDX] = INITIAL_WRITE_INDEX;
   f->idx[APPLY_IDX] = INITIAL_APPLY_INDEX;
   f->idx[READ_IDX] = INITIAL_READ_INDEX;

   return rc;
}


float
arc_audio_frame_adjust_gain (struct arc_audio_frame_t *f, float rate, int direction)
{

   float rc = 0.0;
   struct arc_audio_gain_parms_t *p;

   switch (direction) {
   case ARC_AUDIO_GAIN_ADJUST_UP:
      if (rate > 3.0 || rate < 0.0) {
         fprintf (stderr, " %s() ADJUST UP rate must be a percentage between 0.0 and 1.0 cannot process\n", __func__);
         return 0.0;
      }
      rate = rate + 1.0;
      break;
   case ARC_AUDIO_GAIN_ADJUST_EXPLICIT:
      if (rate > 3.0 || rate < 0.0) {
         fprintf (stderr, " %s() EXPLICIT rate must be a percentage between 0.0 and 3.0 cannot process\n", __func__);
         return 0.0;
      }
      break;
   case ARC_AUDIO_GAIN_ADJUST_DOWN:
      if (rate > 1.0 || rate < 0.0) {
         fprintf (stderr, " %s() DOWN rate must be a percentage between 0.0 and 1.0 cannot process\n", __func__);
         return 0.0;
      }
      break;
   case ARC_AUDIO_GAIN_RESET:
      break;
   default:
      fprintf (stderr, " %s() unhandled audio gain direction passed in returning\n", __func__);
   }


   if (f->do_lock) {
      pthread_mutex_lock (&f->lock);
   }

   p = f->process_args[2];

   if (p != NULL) {
      if ((p->current * rate) <= p->max && (p->current * rate) >= p->min) {
         p->current *= rate;
         rc = p->current;
      }
      else {
         rc = p->current;
      }
   }

   // added to fix a bug with volume 

   if (direction == ARC_AUDIO_GAIN_RESET) {
      if (p != NULL) {
         rc = p->current = 1.0;
      }
   }

   if (direction == ARC_AUDIO_GAIN_ADJUST_EXPLICIT) {
      if (p != NULL) {
         p->current = rate;
         rc = p->current;
      }
   }

   if (f->do_lock) {
      pthread_mutex_unlock (&f->lock);
   }

   return rc;
}


int
arc_audio_frame_add_cb (struct arc_audio_frame_t *f, int option, void *parms)
{

   int rc = -1;

   void *(*setup[32]) (void *parms) = {
   NULL,
         slin16_setup_audio_mixing,
         slin16_setup_audio_gain_control,
         slin16_setup_silence_gen,
         NULL,
         NULL,
         NULL,
         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

   int (*process[32]) (struct arc_audio_frame_t * f, int idx, char *buff, size_t size, void *parms) = {
   NULL,
         slin16_do_audio_mixing,
         slin16_run_audio_gain_control,
         slin16_do_silence_gen,
         NULL,
         NULL,
         NULL,
         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

   switch (option) {

   case ARC_AUDIO_PROCESS_NONE:
      // this process posts raw audio data
      f->process_args[0] = NULL;
      f->process_chain[0] = process[0];
      break;
   case ARC_AUDIO_PROCESS_AUDIO_MIX:
      f->process_args[1] = setup[1] (parms);
      f->process_chain[1] = process[1];
      break;
   case ARC_AUDIO_PROCESS_GAIN_CONTROL:
      f->process_args[2] = setup[2] (parms);
      f->process_chain[2] = process[2];
      break;
   case ARC_AUDIO_PROCESS_GEN_SILENCE:
      f->process_args[3] = setup[3] (parms);
      f->process_chain[3] = process[3];
      break;
   default:
      fprintf (stderr, " %s() default case hit in setting up audio processing callbacks\n", __func__);
   }


   return rc;
}

int
arc_audio_frame_post (struct arc_audio_frame_t *f, char *buff, size_t size, int opts)
{

   //int rc = -1;
   int i;
   int idx;
   int do_copy = 1;

   if (!f) {
      return -1;
   }

   if (!size) {
      fprintf (stderr, " %s() posted size is %zu\n", __func__, size);
      return -1;
   }


   if (f->do_lock) {
      pthread_mutex_lock (&f->lock);
   }

   if (size != f->size) {
      fprintf (stderr, " %s() size[%zu] is too large to post to ringbuff with size[%zu], returning -1\n", __func__, size, f->size);
      pthread_mutex_unlock (&f->lock);
      return -1;
   }

   // if(f->idx[WRITE_IDX] == f->idx[READ_IDX] || f->idx[WRITE_IDX] == f->idx[APPLY_IDX]){
   //    fprintf(stderr, " %s() failed entry condition read=%d apply=%d write=%d\n", __func__, f->idx[READ_IDX], f->idx[APPLY_IDX], f->idx[WRITE_IDX]);
   //    pthread_mutex_unlock(&f->lock);
   //    return 0;
   // }


   idx = f->idx[WRITE_IDX];


   if (opts) {
      for (i = 0; i < 32; i++) {

         if ((1 << i) & opts) {

            switch (1 << i) {

            case ARC_AUDIO_PROCESS_GEN_SILENCE:
               if (f->process_chain[3] != NULL) {
                  f->process_chain[3] (f, idx, buff, size, f->process_args[3]);
                  do_copy = 0;
               }
               break;

            case ARC_AUDIO_PROCESS_GAIN_CONTROL:
               if (f->process_chain[2] != NULL) {
                  f->process_chain[2] (f, idx, buff, size, f->process_args[2]);
               }
               break;

            case ARC_AUDIO_PROCESS_AUDIO_MIX:
               if (f->process_chain[1] != NULL) {
                  f->process_chain[1] (f, idx, buff, size, f->process_args[1]);
               }
               break;

            default:
               fprintf (stderr, " %s() hit default in processing audio, this is harmless but should not happen\n", __func__);
               break;
            }
         }
      }
   }

   if (do_copy) {
      memcpy (f->ringbuff[idx], buff, size);
   }

   f->used[idx] = 1;
   f->posts[idx] = size;
   f->idx[WRITE_IDX]++;
   f->idx[WRITE_IDX] &= AUDIO_FRAME_RING_BUFF_SIZE - 1;
   f->packets++;

   if (f->do_lock) {
      pthread_mutex_unlock (&f->lock);
   }

   return size;
}


// returns the number of 
// audio frames processed 

int
arc_audio_frame_apply (struct arc_audio_frame_t *f, char *buff, size_t size, int opts)
{
   int rc = 0;
   int i;
   char *ptr;
   size_t bufsize;

   if (!f) {
      return -1;
   }

   int idx;


   if (f->do_lock) {
      pthread_mutex_lock (&f->lock);
   }

   if (f->packets < 5) {
      // fprintf(stderr, " %s() too few posts\n", __func__);
      pthread_mutex_unlock (&f->lock);
      return 0;
   }


   //if(f->idx[APPLY_IDX] == f->idx[WRITE_IDX] || f->idx[APPLY_IDX] == f->idx[READ_IDX]){
   //if(f->idx[APPLY_IDX] == f->idx[WRITE_IDX]){
   //   fprintf(stderr, " %s() failed entry condition read=%d apply=%d write=%d\n", __func__, f->idx[READ_IDX], f->idx[APPLY_IDX], f->idx[WRITE_IDX]);
   //   pthread_mutex_unlock(&f->lock);
   //   return 0;
   //}


   idx = f->idx[APPLY_IDX];

   if (f->posts[idx] == 0) {
      pthread_mutex_unlock (&f->lock);
      return -1;
   }

   for (i = 0; i < 32; i++) {
      if ((1 << i) & opts) {

         switch (1 << i) {

         case ARC_AUDIO_PROCESS_GAIN_CONTROL:
            // buff and size are from the frame slice 

            ptr = f->ringbuff[idx];
            bufsize = f->posts[idx];

            if (f->process_chain[2] != NULL) {
               f->process_chain[2] (f, idx, ptr, bufsize, f->process_args[2]);
            }
            break;

         case ARC_AUDIO_PROCESS_AUDIO_MIX:
            if (f->process_chain[1] != NULL) {
               f->process_chain[1] (f, idx, buff, size, f->process_args[1]);
            }
            break;

         default:
            break;
         }
      }
   }

   f->idx[APPLY_IDX]++;
   f->idx[APPLY_IDX] &= AUDIO_FRAME_RING_BUFF_SIZE - 1;

   if (f->do_lock) {
      pthread_mutex_unlock (&f->lock);
   }

   return rc;
}


// return the number of frames 
// processed 

#define SRC_FRAME 1
#define DST_FRAME 0

int
arc_audio_frame2frame_apply (struct arc_audio_frame_t *dst, struct arc_audio_frame_t *src, int opts)
{

   int rc = 0;
   int frame_cnt[2];
   int i;

   pthread_mutex_lock (&dst->lock);
   pthread_mutex_lock (&src->lock);

   frame_cnt[SRC_FRAME] = 0;
   frame_cnt[DST_FRAME] = 0;

   for (i = 0; i < 32; i++) {
      if ((1 << i) & opts) {
         fprintf (stderr, " %s() i=%d\n", __func__, i);
      }
   }

   pthread_mutex_unlock (&dst->lock);
   pthread_mutex_unlock (&src->lock);
   return rc;
}


/* read one frame of audio from trailing index             */
/* may block on condition variable it read idx = write idx */

int
arc_audio_frame_advance_idx (struct arc_audio_frame_t *f)
{

   int rc = 0;

   if (f->do_lock) {
      pthread_mutex_lock (&f->lock);
   }

   f->idx[APPLY_IDX]++;
   f->idx[APPLY_IDX] &= AUDIO_FRAME_RING_BUFF_SIZE - 1;

   if (f->do_lock) {
      pthread_mutex_unlock (&f->lock);
   }

   return rc;
}



int
arc_audio_frame_read (struct arc_audio_frame_t *f, char *buff, size_t bufsize)
{

   int rc = 0;


   if (!f || !buff) {
      return 0;
   }

   if (f->do_lock) {
      pthread_mutex_lock (&f->lock);
   }

   if (f->posts[f->idx[READ_IDX]] > bufsize) {
      fprintf (stderr, " %s() !!!!! audio frame too large for dest buffer\n", __func__);
      pthread_mutex_unlock (&f->lock);
      return 0;
   }

   if (f->used[f->idx[READ_IDX]] == 0) {
      // fprintf (stderr, " %s() !!!!! audio frame not used by writer yet.\n", __func__);
      //  pthread_mutex_unlock (&f->lock);
      //return 0;
   }


#if 0
   if (f->packets < 3) {
      fprintf (stderr, " %s() too few posts\n", __func__);
      pthread_mutex_unlock (&f->lock);
      return 0;
   }
#endif


   //if(f->idx[READ_IDX] == f->idx[APPLY_IDX] || f->idx[READ_IDX] == f->idx[WRITE_IDX]){
   //   fprintf(stderr, " %s() failed entry condition read=%d apply=%d write=%d\n", __func__, f->idx[READ_IDX], f->idx[APPLY_IDX], f->idx[WRITE_IDX]);
   //   pthread_mutex_unlock(&f->lock);
   //   return 0;
   //}

   if (f->posts[f->idx[READ_IDX]] > 0) {
      memcpy (buff, f->ringbuff[f->idx[READ_IDX]], f->posts[f->idx[READ_IDX]]);
      rc = f->posts[f->idx[READ_IDX]];
      //memset (f->ringbuff[f->idx[READ_IDX]], 0, f->size);                     DDN20110505
      f->posts[f->idx[READ_IDX]] = 0;
   }
   else {

#ifdef BITS64
      //fprintf (stderr, " %s(%p) read idx[%d] post size == %ld\n", __func__, f, f->idx[READ_IDX], f->posts[f->idx[READ_IDX]]);
#else
      //fprintf (stderr, " %s(%p) read idx[%d] post size == %d\n", __func__, f, f->idx[READ_IDX], f->posts[f->idx[READ_IDX]]);
#endif

   }

   if (rc) {

      f->idx[READ_IDX]++;
      f->idx[READ_IDX] &= AUDIO_FRAME_RING_BUFF_SIZE - 1;

      //f->idx[APPLY_IDX]++;
      //f->idx[APPLY_IDX] &= AUDIO_FRAME_RING_BUFF_SIZE - 1;

      // fprintf(stderr, " %s() frame=%p read idx=%d write idx=%d apply idx=%d ringbuff size=%d\n", __func__, f, f->idx[READ_IDX], f->idx[WRITE_IDX], f->idx[APPLY_IDX], AUDIO_FRAME_RING_BUFF_SIZE);
   }
   if (f->do_lock) {
      pthread_mutex_unlock (&f->lock);
   }

   return rc;
}


int
arc_audio_frame_reset (struct arc_audio_frame_t *f)
{

   int i;

   if (!f) {
      return -1;
   }

   if (f->do_lock) {
      pthread_mutex_lock (&f->lock);
   }

   f->idx[READ_IDX] = INITIAL_READ_INDEX;
   f->idx[APPLY_IDX] = INITIAL_APPLY_INDEX;
   f->idx[WRITE_IDX] = INITIAL_WRITE_INDEX;

   for (i = 0; i < AUDIO_FRAME_RING_BUFF_SIZE; i++) {
      slin16_gen_silence (f->ringbuff[i], f->size);
      // memset (f->ringbuff[i], 0, f->size);
      f->posts[i] = f->size;
      //f->posts[i] = 0;
      f->used[i] = 0;
   }

   if (f->do_lock) {
      pthread_mutex_unlock (&f->lock);
   }

   return 0;
}



void
arc_audio_frame_free (struct arc_audio_frame_t *f)
{

   void (*destroy[]) (void *) = {
   NULL,
         slin16_destroy_audio_mixing,
         slin16_destroy_audio_gain_control,
         slin16_destroy_silence_gen,
         NULL,
         NULL,
         NULL,
         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};


   if (!f) {
      return;
   }

   int i;

   for (i = 0; i < 32; i++) {
      if (f->process_args[i] != NULL) {
         destroy[i] (f->process_args[i]);
         f->process_args[i] = NULL;
      }
      if (f->process_parms[i] != NULL) {
         free (f->process_parms[i]);
         f->process_parms[i] = NULL;
      }
   }


   f->size = 0;

   return;
}


#ifdef MAIN

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

      /* call ioctls on for word size and bitrate */

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


int
main (int argc, char **argv)
{

   char string[320];
   char mod[320];
   char filebuf[320];
   int rc;
   int audio_fd;
   int file_fd;
   int other_fd;
   int outfile;

   float old;
   int count;

   struct arc_audio_frame_t frame;
   struct arc_audio_gain_parms_t gain;
   struct arc_audio_mixing_parms_t mix;

   outfile = open ("./outfile.raw", O_WRONLY | O_CREAT);

   if (outfile == -1) {
      fprintf (stderr, " %s() could not open outfile for writing exiting\n", __func__);
      exit (1);
   }

   audio_fd = audio_init ("/dev/dsp", 1, 16, 8000, O_RDWR);

   if (argv[1] != NULL) {
      file_fd = open (argv[1], O_RDONLY);
      if (file_fd == -1) {
         fprintf (stderr, " %s() failed to open audio file [%s]\n", __func__, argv[1]);
         exit (1);
      }
   }

   if (argv[2] != NULL) {
      other_fd = open (argv[2], O_RDONLY);
      if (other_fd == -1) {
         fprintf (stderr, " %s() failed to open audio file [%s]\n", __func__, argv[2]);
         exit (1);
      }
   }

   if (audio_fd == -1) {
      fprintf (stderr, " %s() failed to open audio device\n", __func__);
      exit (1);
   }

   ARC_AUDIO_GAIN_PARMS_INIT (gain, .5, .25, 2.5, .90, 1);
   ARC_AUDIO_MIX_PARMS_INIT (mix, 1.0, 1.0, 1.0);

   arc_audio_frame_init (&frame, 320, 1);
   arc_audio_frame_add_cb (&frame, ARC_AUDIO_PROCESS_GAIN_CONTROL, &gain);
   arc_audio_frame_add_cb (&frame, ARC_AUDIO_PROCESS_AUDIO_MIX, &mix);

   count = 0;

   while (1) {
      rc = read (audio_fd, string, sizeof (string));
      // fprintf(stderr, " rc = %d\n", rc);
      rc = arc_audio_frame_post (&frame, string, sizeof (string), ARC_AUDIO_PROCESS_GAIN_CONTROL);
      // fprintf(stderr, " rc = %d\n", rc);
      // rc = arc_audio_frame_apply (&frame, string, sizeof (string), ARC_AUDIO_PROCESS_GAIN_CONTROL);
      // fprintf(stderr, " rc = %d\n", rc);
      rc = read (file_fd, filebuf, sizeof (filebuf));
      if (rc == 320) {
         arc_audio_frame_apply (&frame, filebuf, sizeof (filebuf), ARC_AUDIO_PROCESS_AUDIO_MIX);
      }
      rc = read (other_fd, filebuf, sizeof (filebuf));
      if (rc == 320) {
         arc_audio_frame_apply (&frame, filebuf, sizeof (filebuf), ARC_AUDIO_PROCESS_AUDIO_MIX);
      }
      rc = arc_audio_frame_read (&frame, mod, sizeof (mod));
      //fprintf(stderr, " rc = %d\n", rc);
      rc = write (audio_fd, mod, sizeof (mod));
      write (outfile, mod, sizeof (mod));
      //fprintf(stderr, " rc = %d\n", rc);
      count++;

      if (count % 300 == 0) {
         old = arc_audio_frame_adjust_gain (&frame, .1, ARC_AUDIO_GAIN_ADJUST_UP);
         fprintf (stderr, " %s() current volume %f\n", __func__, old);
      }
   }

   arc_audio_frame_free (&frame);

   return 0;
}

#endif
