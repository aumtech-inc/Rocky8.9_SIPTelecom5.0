#ifndef ARC_DECODER_DOT_H
#define ARC_DECODER_DOT_H

#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef SILK
   struct arc_silk_decoder_parms_t
   {
      int samplerate;
   };
#endif

   struct arc_decoder_t
   {
      int codec;
      void *context;
      int (*cb) ();
      // plc will have to wait a bit 
      int seqno;
      void *plc_context;
      int (*plc) ();
   };

   enum arc_decoder_e
   {
      ARC_DECODE_G711 = 0,
      ARC_DECODE_SILK,
      ARC_DECODE_OPUS,
      ARC_DECODE_END
   };

// g711u
// other codecs require 
// more and more variadic params 
// for spandsp 

   struct arc_g711_parms_t
   {
      int mode;
   };

   struct arc_silk_parms_t
   {
      int bitrate;
   };

   struct arc_opus_decoder_parms_t
   {
      int bitrate;              /* 8k for now */
      int channels;             /* 1 for now  */
   };

#define ARC_OPUS_DECODER_PARMS_INIT(parms)\
do { memset(&parms, 0, sizeof(struct arc_opus_decoder_parms_t)); parms.bitrate = 8000; parms.channels = 1;} while(0)

#define ARC_G711_PARMS_INIT(parms, UA)\
do { memset(&parms, 0, sizeof(struct arc_g711_parms_t)); parms.mode = UA;} while(0)

   int arc_decoder_init (struct arc_decoder_t *dc, int codec, void *parms, int size, int freeme);

   int arc_decoder_plc_init (struct arc_decoder_t *dc);

   int arc_decode_buff (int zLine, struct arc_decoder_t *dc, const char *src, int size, char *dst, int dstsize);

   void arc_decoder_free (struct arc_decoder_t *dc);

#ifdef __cplusplus
};
#endif

#endif
