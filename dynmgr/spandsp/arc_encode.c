#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <spandsp.h>
#include <spandsp/g711.h>
#include <spandsp/private/g711.h>

#include "arc_encode.h"

#include <opus/opus.h>

#ifdef SILK
/* silk includes */

#include <silk/SKP_Silk_control.h>
#include <silk/SKP_Silk_errors.h>
#include <silk/SKP_Silk_SDK_API.h>
#include <silk/SKP_Silk_typedef.h>

/* opus context */

struct arc_opus_context_t
{
   OpusEncoder *context;
};

static void *
arc_opus_context (void *parms)
{
   struct arc_opus_context_t *rc = NULL;
   struct arc_opus_encoder_parms_t *op = parms;
   int err;

   if(!parms){
     fprintf(stderr, " %s() parameters not supplied, cannot initialize...returning NULL\n", __func__);
     return NULL;
   }

   rc = calloc(1, sizeof(struct arc_opus_context_t));

   if(rc){
     rc->context = opus_encoder_create(op->bitrate, op->channels, op->mode, &err);
     if(err){
       fprintf(stderr, " %s() err is set value=%d\n", __func__, err);
     }
   } 

   return rc;
}

static int 
arc_opus_encode(void *context, char *src, int srcsize, char *dst, int dstsize){

  return 0;
}

static void 
arc_opus_context_free(void *context){

   if(context && context->context){
     ;; // free context 
   }

   return;
}

/* end opus */


/* silk codec */

struct arc_silk_context_t
{
   SKP_SILK_SDK_EncControlStruct status;
   void *silk_context;
};

static void *
arc_silk_context (void *parms)
{

   struct arc_silk_context_t *rc = NULL;
   // struct arc_silk_encoder_parms_t *silk_parms = (struct arc_silk_encoder_parms_t *)parms;
   int status = -1;

   int size = 0;

   SKP_Silk_SDK_Get_Encoder_Size (&size);

   if (size) {
      rc = (struct arc_silk_context_t *) calloc (1, sizeof (struct arc_silk_context_t));
      if (rc != NULL) {
         rc->silk_context = calloc (1, size);
         if (rc->silk_context != NULL) {
            status = SKP_Silk_SDK_InitEncoder (rc->silk_context, &rc->status);
            fprintf (stderr, " silk init status = %d\n", status);
         }
         rc->status.API_sampleRate = 8000;
         rc->status.maxInternalSampleRate = 8000;
         rc->status.packetSize = 160;
         rc->status.bitRate = 8000;
         rc->status.packetLossPercentage = 10;
         rc->status.complexity = 0;
         rc->status.useInBandFEC = 0;
         rc->status.useDTX = 0;
      }
   }

   return rc;
}

static int
arc_silk_encode (void *context, char *src, int srcsize, char *dst, int dstsize)
{

   int rc = -1;
   struct arc_silk_context_t *sc = context;
   SKP_int16 nBytes = dstsize;

   if (sc) {
      rc = SKP_Silk_SDK_Encode (sc->silk_context, &sc->status, (SKP_int16 *) src, srcsize, (SKP_uint8 *) dst, &nBytes);
   }

   return nBytes;
}


static void
arc_silk_context_free (void *context)
{

   struct arc_silk_context_t *c = context;

   if (!c) {
      return;
   }

   if (c->silk_context != NULL) {
      free (c->silk_context);
   }

   free (c);

   return;
}



/* end silk */
#endif

// g711u
// other codecs require 
// more and more variadic params 
// for spandsp 

struct arc_g711_parms_t
{
   int mode;
};


static void *
arc_g711_context (void *parms)
{
   void *rc;
   g711_state_t g;
   struct arc_g711_parms_t *p = (struct arc_g711_parms_t *) parms;


   g711_init (&g, p->mode);

   rc = calloc (1, sizeof (g711_state_t));

   if (rc) {
      memcpy (rc, &g, sizeof (g711_state_t));
   }

   return rc;
}

static int
arc_g711_encode (void *context, char *src, int srcsize, char *dst, int dstsize)
{

   int rc = 0;

   if (!src || !dst) {
      return 0;
   }

// SPAN_DECLARE(int) g711_encode(g711_state_t *s, uint8_t g711_data[], const int16_t amp[], int len);

   rc = g711_encode ((g711_state_t *) context, (uint8_t *) dst, (int16_t *) src, srcsize);
   return rc;
}



static void
arc_g711_context_free (void *context)
{

   if (context) {
      free (context);
   }
   return;
}


#define ARC_G711_PARMS_INIT(parms, UA)\
do { memset(&parms, 0, sizeof(struct arc_g711_parms_t)); parms.mode = UA;} while(0)

int
arc_encoder_init (struct arc_encoder_t *ec, int codec, void *parms, size_t size, int freeme)
{

   int rc = -1;
   int i;

   void *(*setup_cbs[32]) () = {
      arc_g711_context, 
      NULL,   //arc_silk_context,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

   int (*encode_cbs[32]) () = {
      arc_g711_encode, 
      NULL,    //arc_silk_encode,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

   if (!rc) {
      return rc;
   }

   if (freeme) {
      arc_encoder_free (ec);
   }

   for (i = 0; i < 32; i++) {
      if (i == codec) {
         ec->context = setup_cbs[i] (parms);
         if (ec->context) {
            ec->cb = encode_cbs[i];
            ec->codec = i;
            rc = 0;
         }
         break;
      }
   }

   if (rc != 0) {
      fprintf (stderr, " %s() failed to init encoder, check codec type\n", __func__);
   }


   return rc;
}


int
arc_encode_buff (struct arc_encoder_t *ec, const char *src, size_t size, char *dst, size_t dstsize)
{

   int rc = -1;
   int samples = size / 2;

   if (!ec || !src || !dst) {
      return rc;
   }

   if (ec && ec->context && ec->cb) {
      // fprintf(stderr, " %s() codec=%d\n", __func__, ec->codec);
      rc = ec->cb (ec->context, src, samples, dst, dstsize);
   }

   return rc;
}

void
arc_encoder_free (struct arc_encoder_t *ec)
{

   void (*destroy_cbs[32]) (void *) = {
      arc_g711_context_free, NULL,      //arc_silk_context_free,
   NULL,
         NULL,
         NULL,
         NULL,
         NULL,
         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};


   if (ec && ec->context) {
      destroy_cbs[ec->codec] (ec->context);
      ec->context = NULL;
   }

   return;
}


#ifdef MAIN

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

   struct arc_encoder_t enc;
   struct arc_g711_parms_t parms;
   char in_buff[1024];
   char out_buff[1024];
   int count = 1000;
   int rc;
   int fd;

   ARC_G711_PARMS_INIT (parms, 0);

   arc_encoder_init (&enc, ARC_ENCODE_G711, &parms, sizeof (parms));

   fd = audio_init ("/dev/dsp", 1, 16, 8000, O_RDONLY);
   if (fd == -1) {
      fprintf (stderr, " %s() fd=%d exiting\n", __func__, fd);
      exit (1);
   }

   while (count) {
      rc = read (fd, in_buff, sizeof (in_buff));
      fprintf (stderr, " %s() read rc = %d\n", __func__, rc);
      rc = arc_encode (&enc, out_buff, sizeof (out_buff), in_buff, sizeof (in_buff));
      fprintf (stderr, " %s() encode rc = %d\n", __func__, rc);
   }

   arc_encoder_free (&enc);

   return 0;
}


#endif
