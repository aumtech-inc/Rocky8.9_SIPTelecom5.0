#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "arc_conference_frames.h"

#define ARC_CONF_READ_IDX  0
#define ARC_CONF_WRITE_IDX 1

#define ARC_CONFERENCE_HIGH_WATER_MARK 10.0

#define ADV_IDX(idx, pow2)\
do{ idx++; idx &= (pow2 -1);} while(0)

#define INC_COUNTER(cntr, pow2)\
do { cntr++; cntr &= (pow2 - 1);} while(0)
#define DEC_COUNTER(cntr, pow2)\
do { cntr--; cntr &= (pow2 - 1);} while(0)


#if 0

struct arc_conference_frame_t
{
   char *ringbuffer[ARC_CONF_RINGBUFFER_SIZE];
   int posts[ARC_CONF_RINGBUFFER_SIZE];
   int stats[ARC_CONF_RINGBUFFER_SIZE];
   int size;
   int count;
   int idx[2];
   int freeme;
};


#endif


static inline void
audio_noise_gate (char *buff, size_t size, int thresh)
{

   short *ptra;
   int i;

   for (i = 0; i < size; i++, i++) {
      ptra = (short *) &buff[i];
      if (abs (*ptra) < thresh) {
         *ptra = 0;
      }
   }

   return;
}


static inline void
audio_gen_silence (char *buff, size_t size)
{

   short pattern[] = { -5, -5, 5, 5, -5, -5, 5, 5 };
   // short pattern[] = {5, 5, 5, 5, 5, 5, 5, 5};
   // short pattern[] = {0, 0, 0, 0, 0, 0, 0, 0};
   short *ptra;

   int samples = size / 2;
   int i, j;

   for (i = 0, j = 0; i < samples; i += 2) {
      ptra = (short *) &buff[i];
      *ptra = pattern[j &= (8 - 1)];
   }

   return;

}

static inline int
audio_mix_cb (struct arc_conference_frame_t *cf, int idx, char *buff, size_t size)
{

   int i;
   int samples;
   short *ptra, *ptrb;
   // int val;
   double val;
   int divisor = 32767;

   if (!buff || !size) {
      fprintf (stderr, " %s() nothing to do, buff or buffsize empty\n", __func__);
      return -1;
   }

#define SLIN16_IS_SIGNED(sample) (sample & 0x8000)

   // audio_noise_gate(buff, size, 64);

   samples = size / 2;

   for (i = 0; i < size; i++, i++) {

      ptra = (short *) &cf->ringbuffer[idx][i];
      ptrb = (short *) &buff[i];

#if 0

      val = *ptra + *ptrb;
      if (val == divisor) {
         val = divisor;
      }
      else if (val == -divisor) {
         val = -divisor;
      }

#endif


#if 1

      if (*ptrb == 0) {
         val = *ptra;
      }
      else if (*ptra == 0) {
         val = *ptrb;
      }
      else if (SLIN16_IS_SIGNED (*ptra) && SLIN16_IS_SIGNED (*ptrb)) {
         val = ((*ptra + *ptrb) - (*ptra * *ptrb) / -divisor);
      }
      else if (!SLIN16_IS_SIGNED (*ptra) && !SLIN16_IS_SIGNED (*ptrb)) {
         val = ((*ptra + *ptrb) - (*ptra * *ptrb) / divisor);
      }
      else {
         // different signed bits 
         val = (*ptra + *ptrb);
      }
#endif

      *ptra = val;
   }

#undef SLIN16_IS_SIGNED

   // fprintf(stderr, " %s() rc=%d\n", __func__, samples);

   return samples;
}


struct arc_conference_frame_t *
arc_conference_frame_init (struct arc_conference_frame_t *cf, int size)
{

   struct arc_conference_frame_t *rc;
   int i;

   if (cf) {
      rc = cf;
   }
   else {
      rc = calloc (1, sizeof (struct arc_conference_frame_t));
      if (rc) {
         rc->freeme++;
      }
   }

   for (i = 0; i < ARC_CONF_RINGBUFFER_SIZE; i++) {
      rc->ringbuffer[i] = calloc (1, size);
      if (rc->ringbuffer[i] == NULL) {
         goto bail;
      }
   }

   rc->dbl_buff = calloc (1, size);

   rc->idx[ARC_CONF_WRITE_IDX] = ARC_CONF_RINGBUFFER_SIZE / 2;
   rc->size = size;

   return rc;

 bail:

   fprintf (stderr, " %s() failed to allocate conference frame, bailing...\n", __func__);

   for (i = 0; i < ARC_CONF_RINGBUFFER_SIZE; i++) {
      if (rc->ringbuffer[i] != NULL) {
         free (rc->ringbuffer[i]);
      }
   }

   if (rc->freeme) {
      free (rc);
   }

   return NULL;

}

void
arc_conference_frame_free (struct arc_conference_frame_t *cf)
{

   int i;

   for (i = 0; i < ARC_CONF_RINGBUFFER_SIZE; i++) {
      if (cf->ringbuffer[i] != NULL) {
         free (cf->ringbuffer[i]);
      }
   }

   if (cf->dbl_buff) {
      free (cf->dbl_buff);
   }

   if (cf->freeme) {
      free (cf);
   }

   return;
}

int
arc_conference_frame_post (struct arc_conference_frame_t *cf, char *buff, size_t size, int seq)
{

   int rc = -1;
   int idx;
   float status, divisor;

   if (size != cf->size) {
      fprintf (stderr, " %s() size of ringbuffer=%d size of buffer=%zu\n", __func__, cf->size, size);
      return -1;
   }

   if (cf->state == ARC_CONFERENCE_FRAME_STATE_RESEQUENCE) {
      return -1;
   }


   if (seq == -1) {
      idx = cf->idx[ARC_CONF_WRITE_IDX];
   }
   else {
      idx = (seq & (ARC_CONF_RINGBUFFER_SIZE - 1));
   }

   //if(cf->state == ARC_CONFERENCE_FRAME_STATE_RESEQUENCE){
   //  idx = cf->idx[ARC_CONF_WRITE_IDX];
   //}

   /// fprintf(stderr, " %s() idx[read]=%d idx[write]=%d\n", __func__, cf->idx[ARC_CONF_READ_IDX], cf->idx[ARC_CONF_WRITE_IDX]);
   rc = audio_mix_cb (cf, idx, buff, size);
   cf->posts[idx] = rc;
   cf->stats[idx]++;

   if (cf->stats[idx] == 1) {
      INC_COUNTER (cf->count, ARC_CONF_RINGBUFFER_SIZE);
   }
   divisor = ARC_CONF_RINGBUFFER_SIZE;

   status = cf->count / divisor;
   //fprintf(stderr, " %s() counter=%d buffer=%2f %% full\n", __func__, cf->count, status * 100);

   // idx++;

   return idx;
}


int
arc_conference_frame_read (struct arc_conference_frame_t *cf, char *buff, size_t size)
{

   float status, divisor;
   char silence[512];
   float percentage = 0.0;
   int i;

   int idx = cf->idx[ARC_CONF_READ_IDX];

   if (!buff || !size) {
      fprintf (stderr, " %s() buff or buffsize is null, returning -1\n", __func__);
      return -1;
   }

   divisor = ARC_CONF_RINGBUFFER_SIZE;
   status = cf->count / divisor;
   percentage = status * 100;

   switch (cf->state) {

   case ARC_CONFERENCE_FRAME_STATE_START:
      if (percentage > 0.0) {
         cf->state = ARC_CONFERENCE_FRAME_STATE_BUFFERING;
      }
      break;

   case ARC_CONFERENCE_FRAME_STATE_BUFFERING:

      if (percentage >= ARC_CONFERENCE_HIGH_WATER_MARK) {
         cf->state = ARC_CONFERENCE_FRAME_STATE_HIGH_WATER;
      }
      else if (percentage == 0.0) {
         cf->state = ARC_CONFERENCE_FRAME_STATE_RESEQUENCE;
         //fprintf(stderr, " %s() frame state set for resequence.\n", __func__);
      }
      break;
   case ARC_CONFERENCE_FRAME_STATE_BUFFERED:
      if (percentage <= 0.0) {
         cf->state = ARC_CONFERENCE_FRAME_STATE_RESEQUENCE;
         // cf->state = ARC_CONFERENCE_FRAME_STATE_START;
         // cf->state = ARC_CONFERENCE_FRAME_STATE_BUFFERING;
      }
      break;
   case ARC_CONFERENCE_FRAME_STATE_HIGH_WATER:
      //fprintf(stderr, " %s() HIGH_WATER (%f) frame buffered\n", __func__, percentage);
      cf->state = ARC_CONFERENCE_FRAME_STATE_BUFFERED;
      break;
   case ARC_CONFERENCE_FRAME_STATE_RESEQUENCE:
      //fprintf(stderr, " %s() frame resequence (%f)\n", __func__, percentage);
      cf->idx[ARC_CONF_READ_IDX] = 0;
      //cf->idx[ARC_CONF_WRITE_IDX] = ARC_CONF_RINGBUFFER_SIZE / 2;
      cf->idx[ARC_CONF_WRITE_IDX] = ARC_CONF_RINGBUFFER_SIZE / 4;
      memset (cf->stats, 0, sizeof (cf->stats));
      memset (cf->posts, 0, sizeof (cf->posts));
      cf->count = 0;
      for (i = 0; i < ARC_CONF_RINGBUFFER_SIZE; i++) {
         memset (cf->ringbuffer[i], 0, cf->size);
      }
      cf->state = ARC_CONFERENCE_FRAME_STATE_START;
      break;
   default:
      //fprintf(stderr, " %s() hit default case in determining buffer state\n", __func__);
      break;
   }


   // if((status * 100) < 50.0){
   if (cf->state == ARC_CONFERENCE_FRAME_STATE_BUFFERING || cf->state == ARC_CONFERENCE_FRAME_STATE_START || cf->state == ARC_CONFERENCE_FRAME_STATE_RESEQUENCE) {
      if (percentage) {
         // fprintf(stderr, " %s(idx=%d) counter=%d buffer=%2f %% full frame=%p\n", __func__, idx, cf->count, percentage, cf);
      }
      memset (silence, 0, cf->size);
      memcpy (buff, silence, cf->size);
      return -1;
   }

   // fprintf(stderr, " %s() idx[read]=%d idx[write]=%d stats=%d\n", __func__, cf->idx[ARC_CONF_READ_IDX], cf->idx[ARC_CONF_WRITE_IDX], cf->stats[idx]);

   ADV_IDX (cf->idx[ARC_CONF_READ_IDX], ARC_CONF_RINGBUFFER_SIZE);
   ADV_IDX (cf->idx[ARC_CONF_WRITE_IDX], ARC_CONF_RINGBUFFER_SIZE);
   memcpy (buff, cf->ringbuffer[idx], cf->size);
   memset (cf->ringbuffer[idx], 0, cf->size);
   // audio_gen_silence(cf->ringbuffer[idx], cf->size);

   if (cf->stats[idx]) {
      DEC_COUNTER (cf->count, ARC_CONF_RINGBUFFER_SIZE);
   }

   cf->stats[idx] = 0;
   cf->posts[idx] = 0;

   return idx;
}

#ifdef MAIN
#include <pthread.h>


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

   struct arc_conference_frame_t *frame;
   int i;
   int rc;
   char buff[1024];
   int seq = -1;
   int audiofd;
   FILE *fps[6];
   int seqno[6];

   audiofd = audio_init ("/dev/dsp", 1, 16, 8000, O_RDWR);

   memset (fps, 0, sizeof (fps));

   for (i = 1; i != 6; i++) {
      if (argv[i] == NULL) {
         break;
      }
      else {
         fps[i] = fopen (argv[i], "r");
         if (!fps[i]) {
            fprintf (stderr, " %s() unable to open audio file [%s] for reading, exiting\n", __func__, argv[i]);
            exit (1);
         }
         seqno[i] = -1;
      }
   }

   frame = arc_conference_frame_init (NULL, 320);

   for (;;) {

      for (i = 1; i != 6; i++) {
         if (fps[i] != NULL) {
            fread (buff, 320, 1, fps[i]);
            seqno[i] = arc_conference_frame_post (frame, buff, 320, seqno[i]);
            seqno[i]++;
         }
      }

      rc = arc_conference_frame_read (frame, buff, 320);

      if (audiofd != -1) {
         write (audiofd, buff, 320);
      }
      memset (buff, 0, sizeof (buff));
   }
   arc_conference_frame_free (frame);
   return 0;

}

#endif
