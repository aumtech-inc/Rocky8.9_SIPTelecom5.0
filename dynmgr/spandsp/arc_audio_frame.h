#ifndef ARC_AUDIO_FRAME_DOT_H
#define ARC_AUDIO_FRAME_DOT_H

#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>


#ifdef __cplusplus
extern "C"
{
#endif

// #define AUDIO_FRAME_RING_BUFF_SIZE 32
// #define AUDIO_FRAME_RING_BUFF_SIZE 16
#define AUDIO_FRAME_RING_BUFF_SIZE 8
#define AUDIO_FRAME_BUFF_SIZE 512


   enum arc_audio_frame_flags_e
   {
      ARC_AUDIO_FRAME_FLAG_NONE = 0,
      ARC_AUDIO_FRAME_FLAG_SILENT = (1 << 0)
   };


/*
 *
 *
 *  Frame type for now are assumed 16 bit
 *  trancoding from a to u law might be 
 *  a good use for the 8 bit format 
 *
 */

   enum arc_audio_frame_types_e
   {
      ARC_AUDIO_FRAME_SLIN_16 = 0,
      ARC_AUDIO_FRAME_UINT_8
   };


/*
 *  These are the methods for audio manipulation
 *  only mixing and audio are done for now
 *
 */


   enum arc_audio_processes_e
   {
      ARC_AUDIO_PROCESS_NONE = 0,       //0
      ARC_AUDIO_PROCESS_AUDIO_MIX = (1 << 0),   //1
      ARC_AUDIO_PROCESS_GAIN_CONTROL = (1 << 1),        //2
      ARC_AUDIO_PROCESS_GEN_SILENCE = (1 << 2), //3
   };

   struct arc_audio_frame_t
   {
      // experiment
      void *process_parms[32];
      void *process_args[32];
      int (*process_chain[32]) (struct arc_audio_frame_t * f, int idx, char *buff, size_t size, void *args);
      // end experiment 
      pthread_mutex_t lock;
      pthread_cond_t cond;
      char do_lock;
      //char *ringbuff[AUDIO_FRAME_RING_BUFF_SIZE];
      char ringbuff[AUDIO_FRAME_RING_BUFF_SIZE][AUDIO_FRAME_BUFF_SIZE];
      size_t posts[AUDIO_FRAME_RING_BUFF_SIZE];
      size_t flags[AUDIO_FRAME_RING_BUFF_SIZE];
      size_t used[AUDIO_FRAME_RING_BUFF_SIZE];
      size_t size;
      int type;
      int idx[3];
      unsigned int packets;
   };

#define READ_IDX 0
#define WRITE_IDX 2
#define APPLY_IDX 1

   struct arc_audio_gain_parms_t
   {
      float current;
      float wish;
      float min;
      float max;
      float step;
      int clipping;
      int agc;
   };

   struct arc_audio_mixing_parms_t
   {
      int clips;
      float coef;
      float min;
      float max;
   };

   struct arc_audio_silence_parms_t
   {
      char *sbuffer;
      int len;
   };


#define ARC_AUDIO_SILENCE_PARMS_INIT(parms, size)\
do { memset(&parms, 0, sizeof(parms)); parms.len = size;} while(0)

#define ARC_AUDIO_GAIN_PARMS_INIT(parms, desired, minimum, maximum, stepping, auto_gain)\
do { memset(&parms, 0, sizeof(parms)); parms.current = 1.0; parms.wish = desired; parms.min = minimum; parms.max = maximum; parms.step = stepping; parms.agc = auto_gain;} while(0)

#define ARC_AUDIO_MIX_PARMS_INIT(parms, volume, minimum, maximum)\
do { memset(&parms, 0, sizeof(parms)); parms.coef = volume; } while(0)


//  returns number of bytes processed
//


#define IS_SIGNED_16BIT_INT(sshort)\
(sshort & 0x8000)


   int arc_audio_frame_init (struct arc_audio_frame_t *f, size_t size, int type, int do_lock);

   enum arc_volume_adjust_e
   {
      ARC_AUDIO_GAIN_ADJUST_UP = 0,
      ARC_AUDIO_GAIN_ADJUST_DOWN,
      ARC_AUDIO_GAIN_ADJUST_EXPLICIT,
      ARC_AUDIO_GAIN_RESET
   };

   float arc_audio_frame_adjust_gain (struct arc_audio_frame_t *f, float rate, int direction);

   int arc_audio_frame_add_cb (struct arc_audio_frame_t *f, int option, void *parms);

   int arc_audio_frame_post (struct arc_audio_frame_t *f, char *buff, size_t size, int opts);

// returns the number of 
// audio frames processed 

   int arc_audio_frame_apply (struct arc_audio_frame_t *f, char *buff, size_t size, int opts);

// return the number of frames 
// processed 

   int arc_audio_frame2frame_apply (struct arc_audio_frame_t *dst, struct arc_audio_frame_t *src, int opts);

/* read one frame of audio from trailing index             */
/* may block on condition variable it read idx = write idx */

   int arc_audio_frame_read (struct arc_audio_frame_t *f, char *buff, size_t bufsize);

   int arc_audio_frame_advance_idx (struct arc_audio_frame_t *f);

   int arc_audio_frame_reset (struct arc_audio_frame_t *f);

   void arc_audio_frame_free (struct arc_audio_frame_t *f);

#ifdef __cplusplus
};
#endif


#endif
