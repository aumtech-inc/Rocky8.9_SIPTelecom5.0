#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <spandsp.h>
#include <spandsp/g711.h>
#include <spandsp/private/g711.h>

#include "arc_decode.h"

/* opus codec includes */
#include <opus/opus.h>


#ifdef SILK
#include <silk/SKP_Silk_control.h>
#include <silk/SKP_Silk_errors.h>
#include <silk/SKP_Silk_SDK_API.h>
#include <silk/SKP_Silk_typedef.h>

/* silk decoders */

struct arc_silk_context_t
{
   void *decState;
   SKP_SILK_SDK_DecControlStruct decStatus;
};

static void *
arc_silk_init (void *parms)
{

   struct arc_silk_context_t *rc = NULL;
   int size;

   SKP_Silk_SDK_Get_Decoder_Size (&size);

   rc = calloc (1, sizeof (struct arc_silk_context_t));

   rc->decState = calloc (1, size);

   if (rc) {
      SKP_Silk_SDK_InitDecoder (rc->decState);
      rc->decStatus.API_sampleRate = 8000;
      rc->decStatus.frameSize = 0;
      rc->decStatus.framesPerPacket = 0;
      rc->decStatus.moreInternalDecoderFrames = 0;
      rc->decStatus.inBandFECOffset = 0;
   }

   return rc;
}


static void
arc_silk_free (void *cntx)
{

   struct arc_silk_context_t *c = cntx;

   if (!c) {
      return;
   }

   if (c->decState) {
      free (c->decState);
   }

   free (c);
   return;
}

static int
arc_silk_decode (struct arc_decoder_t *dc, void *cntx, char *buf, int size, char *dest, int dstsize)
{

   struct arc_silk_context_t *c = cntx;
   SKP_int16 nBytes = 0;

   if (c != NULL) {
      SKP_Silk_SDK_Decode (c->decState, &c->decStatus, 0, (SKP_uint8 *) buf, (SKP_int) size, (SKP_int16 *) dest, &nBytes);
   }

   // fprintf(stderr, " %s() nBytes=%d rc=%d", __func__, nBytes, rc);

   return nBytes;
}
#endif

/*  end silk */

/* opus codec */


static void *
arc_opus_init (void *parms)
{

   void *rc = NULL;

   return rc;
}


static int
arc_opus_decode (struct arc_decoder_t *dc, void *cntx, char *buf, int size, char *dest, int dstsize)
{

   int rc = 0;

   if (!dc || !cntx) {
      fprintf (stderr, " %s() either decoder[%p] or context[%p] equals NULL, cannot continue\n", __func__, dc, cntx);
      return -1;
   }


   return rc;
}

static void
arc_opus_free (void *cntx)
{

   if (cntx) {
      free (cntx);
   }

   return;
}

/* end opus codec */

static void *
arc_g711_init (void *parms)
{

   void *rc = NULL;

   struct arc_g711_parms_t *p = (struct arc_g711_parms_t *) parms;

   g711_state_t g711s;

   if (!p) {
      return NULL;
   }

   g711_init (&g711s, p->mode);

   rc = calloc (1, sizeof (g711s));

   if (rc) {
      memcpy (rc, &g711s, sizeof (g711s));
   }
   return rc;
}


static int
arc_g711_decode (struct arc_decoder_t *dc, void *cntx, char *buf, int size, char *dest, int dstsize)
{

   int rc = -1;

   if (!cntx) {
      fprintf (stderr, " %s() context is set to null cannot continue...\n", __func__);
      return rc;
   }

   rc = g711_decode ((g711_state_t *) cntx, (short *) dest, (unsigned char *) buf, size);

   return rc;
}

static void
arc_g711_free (void *cntx)
{

   if (cntx) {
      free (cntx);
   }
   return;
}

// end g711

int
arc_decoder_init (struct arc_decoder_t *dc, int codec, void *parms, int size, int freeme)
{

   void *(*cbs[ARC_DECODE_END]) (void *) = {
      arc_g711_init, NULL,      //arc_silk_init
   arc_opus_init};

   int (*decode[]) () = {
      arc_g711_decode, NULL,    // silk 
   arc_opus_decode};

   if (!dc) {
      return -1;
   }

   if (freeme) {
      arc_decoder_free (dc);
   }

   if (codec < 0 || codec >= ARC_DECODE_END) {
      return -1;
   }

   memset (dc, 0, sizeof (struct arc_decoder_t));

   dc->context = cbs[codec] (parms);
   dc->cb = decode[codec];

   return 0;
}

int
arc_decoder_plc_init (struct arc_decoder_t *dc)
{
   int rc = -1;

   return rc;
}

int
arc_decode_buff (int zLine, struct arc_decoder_t *dc, const char *src, int size, char *dst, int dstsize)
{

   int bytes = -1;

   if (!dc) {
      return -1;
   }


   if ((size * 2) > dstsize) {
#ifdef DECODER_DEBUG
      fprintf (stderr, " %s() minimum size for dest buffer is too small \n", __func__);
#endif
      return -1;
   }


   switch (dc->codec) {
   case ARC_DECODE_G711:
      if (dc->cb != NULL) {
         bytes = dc->cb (dc, dc->context, src, size, dst, dstsize);
      }
      break;
   case ARC_DECODE_SILK:
      if (dc->cb != NULL) {
         bytes = dc->cb (dc, dc->context, src, size, dst, dstsize);
      }
      break;
   case ARC_DECODE_OPUS:
      if (dc->cb != NULL) {
         bytes = dc->cb (dc, dc->context, src, size, dst, dstsize);
      }
      break;
   default:
#ifdef DECODER_DEBUG
      fprintf (stderr, " %s() hit default case in decoding audio: you should not see this message !\n", __func__);
#endif
      break;
   }

   if (dc->plc && dc->plc_context) {
      ;;                        // we might have to pass in a extra arg for sequence no
   }

   return bytes;
}


void
arc_decoder_free (struct arc_decoder_t *dc)
{

   void (*cbs[ARC_DECODE_END]) () = {
   arc_g711_free, NULL, arc_opus_free};

   if (!dc) {
      return;
   }

   if (dc->codec < 0 || dc->codec >= ARC_DECODE_END) {
      return;
   }

   if (dc && dc->context) {
      cbs[dc->codec] (dc->context);
      dc->context = NULL;
   }


   if (dc->plc_context) {
      free (dc->plc_context);
   }

   return;
}


#ifdef MAIN

#include "arc_tones_ringbuff.h"

int
main (int argc, char **argv)
{

   struct arc_decoder_t adc;
   struct arc_g711_parms_t parms;
   char in_buff[1024];
   char out_buff[2048];
   int rc;
   int count = 1000;
   int fd;
   int detected;

   arc_tones_ringbuff_t rb;


   fd = open ("/dev/audio", O_RDONLY);

   if (fd == -1) {
      fprintf (stderr, " %s() \n", __func__);
   }

   arc_tones_ringbuff_init (&rb, ARC_TONES_RINGBACK | ARC_TONES_INBAND_DTMF | ARC_TONES_DIAL_TONE, 80000000000.0, 1000.0, 0);

   ARC_G711_PARMS_INIT (parms, 1);

   arc_decoder_init (&adc, ARC_DECODE_G711, &parms, sizeof (parms));

   while (count) {

      rc = read (fd, in_buff, sizeof (in_buff));

      fprintf (stderr, " %s() read %d bytes from device\n", __func__, rc);

      if (rc > 0) {

         rc = arc_decode_buff (&adc, in_buff, sizeof (in_buff), out_buff, sizeof (out_buff));

         arc_tones_ringbuff_post (&rb, out_buff, sizeof (out_buff));

         detected = arc_tones_ringbuff_detect (&rb, 0, ARC_TONES_RINGBACK | ARC_TONES_INBAND_DTMF | ARC_TONES_HUMAN, 8000 * 2);

         fprintf (stderr, " detected = %d\n", detected);

         if (detected == ARC_TONES_NONE) {
            fprintf (stderr, " %s() no tones detected\n", __func__);
         }

         if (detected & ARC_TONES_INBAND_DTMF) {
            fprintf (stderr, " %s() detected inband dtmf keys=%s\n", __func__, rb.dtmf);
         }
         fprintf (stderr, " %s() processed %d samples\n", __func__, rc);

      }
   }

   arc_decoder_free (&adc);

   return 0;
}

#endif
