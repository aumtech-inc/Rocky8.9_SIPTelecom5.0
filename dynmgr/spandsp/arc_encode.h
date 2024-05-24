#ifndef ARC_ENCODER_DOT_H
#define ARC_ENCODER_DOT_H

#include <stdio.h>
#include <stdlib.h>

#include <opus/opus.h>

#ifdef __cplusplus
extern "C"
{
#endif

   struct arc_opus_encoder_parms_t
   {
      int bitrate;
      int channels;
      int mode;
   };

#ifdef SILK
   struct arc_silk_encoder_parms_t
   {
      int sample_rate;
      int internal_sample_rate;
      int packet_size;
      int bitrate;
      int packet_loss;
      int complexity;
      int inband_fec;
      int use_dtx;
   };
#endif

   struct arc_encoder_t
   {
      int codec;
      void *context;
      int (*cb) ();
      // plc will have to wait a bit 
   };

   enum arc_encoder_e
   {
      ARC_ENCODE_G711 = 0,
      ARC_ENCODE_SILK,
      ARC_ENCODE_OPUS,
      ARC_ENCODE_END
   };

// g711u
// other codecs require 
// more and more variadic params 
// for spandsp 

#define ARC_OPUS_ENCODER_PARMS_INIT(parms)\
do { memset(&parms, 0, sizeof(struct arc_opus_encoder_parms_t)); parms.bitrate = 8000; parms.channles = 1; parms.mode = } while(0)

   int arc_encoder_init (struct arc_encoder_t *dc, int codec, void *parms, size_t size, int freeme);

   int arc_encode_buff (struct arc_encoder_t *dc, const char *src, size_t size, char *dst, size_t dstsize);

   void arc_encoder_free (struct arc_encoder_t *dc);

#ifdef __cplusplus
};
#endif

#endif
