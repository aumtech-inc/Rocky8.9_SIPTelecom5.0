#ifndef ARC_TONES_RINGBUFF_DOT_H
#define ARC_TONES_RINGBUFF_DOT_H



#ifdef __cplusplus
extern "C"
{
#endif


#include <spandsp/dc_restore.h>




#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>


   enum arc_tones_type_e
   {
      ARC_TONES_NONE = 0,                               /* 0  */
      ARC_TONES_HUMAN = (1 << 0),                       /* 1  */
      ARC_TONES_RECORDING = (1 << 1),                   /* 2  */
      ARC_TONES_FAST_BUSY = (1 << 2),                   /* 3  */
      ARC_TONES_FAX_TONE = (1 << 3),                    /* 4  */
      ARC_TONES_INBAND_DTMF = (1 << 4),                 /* 5  */
      ARC_TONES_DIAL_TONE = (1 << 5),                   /* 6  */
      ARC_TONES_RINGBACK = (1 << 6),                    /* 7  */
      ARC_TONES_BEGINNING_AUDIO_TIMEOUT = (1 << 7),     /* 8  */
      ARC_TONES_TRAILING_AUDIO_TIMEOUT = (1 << 8),      /* 9  */
      ARC_TONES_PACKET_TIMEOUT = (1 << 9),              /* 10 */
      ARC_TONES_FAX_CED = (1 << 10),                    /* 11 */
      ARC_TONES_ACTIVE_SPEAKER = (1 << 11),             /* 12 */
      ARC_TONES_SIT_TONE = (1 << 12)             		/* 13 */
   };

#define RING_BUFF_SIZE 4096

#define READ 0
#define WRITE 1

   struct arc_tones_ringbuff_t
   {
      pthread_mutex_t lock;
      pthread_cond_t cond;
      // cd restoration 
      dc_restore_state_t dc_state;
      // these were implemented by myself
      // they require their own settings
      float human_floor;
      float human_thresh;
      float recording_thresh;
      int samples;              // max duration for human detection must be long than two seconds 
      float silence_floor;
      int leading_samples;
      int trailing_samples;
      // 
      float answering_noise_floor;
      int leading_answering_samples;
      int trailing_answering_samples;
      int answering_samples;
      int answering_thresh_ms;
      int answering_slow_start;
      // span dsp stuff 
      float thresh;
      float diff;
      char *results[32];
      void *contexts[32];
      int (*cbs[32]) ();
      int detected;
      char use_threads;
      char buff[RING_BUFF_SIZE];
      int idx[2];
      int last;
      char dtmf[100];
      // hard timeouts for silence suppression and the like
      // in detecting human vs. answering machine detection 
      int timeo;
      size_t last_post;
   };
   typedef struct arc_tones_ringbuff_t arc_tones_ringbuff_t;


   struct voice_energy_level_t
   {
      float a0;
      float a1;
      float a2;
      float a3;
      float a4;
      float a5;
      float a6;
      float a7;
      float a8;
      float peak_rms;
      float total_rms;
      long int samples;
      // these are hard clips, a count of abs(2**16/2) values - even one or two per session 
      // seems to foul the goertzel algo 
      long int clips;
      // 
      int state;
      int beginning_timeo;
      int trailing_timeo;
      float current_rms;
      time_t time_transitions[3];
   };

   struct silence_detection_t
   {
      int samples;
      float rms;
      int state;
      int beginning_timeo;
      int trailing_timeo;
   };

   struct answering_detection_t
   {
      int samples;
      int sample_count;
      int count;
      float rms;
      int state;
      int reason;
      int utterance_count;
      int beginning_timeo;
      int trailing_timeo;
      struct timeval transitions[4];
      // beginning, active audio, trailtime timeout, overall 
      char result[256];
   };

/** arc_tones_ringbuff_init();
 *
 *  rb = destination ringbuff
 *  detect = mask representing what you want to detect, (this sets up contexts for detection)
 *  thresh = min thresh for detection 
 *  diff = comparison value for determining other found frequencies 
 *  threaded = lock or unlock for multi threaded use 
 */

   int arc_tones_ringbuff_init (arc_tones_ringbuff_t * rb, int detect, float thresh, float diff, int threaded);

/** arc_tones_set_silence_params()
 *
 * rb = ringbuff 
 * leading = number of 8k samples before leading silence is triggered
 * trailing = numbet of 8k samples before trailing slience is triggered
 * floor = noise floor 
*/

   int arc_tones_set_silence_params (arc_tones_ringbuff_t * rb, float floor, int leading, int trailing);

   int arc_tones_set_leading_samples(arc_tones_ringbuff_t * rb, int leading);			// GS - google streaming

   int arc_tones_increment_leading_samples(arc_tones_ringbuff_t * rb, int incrementVal);// GS - google streaming

/** arc_tones_set_human_thresh()
 * 
 * rb = ringbuff
 * floor = minimum rms for any voice activity at all
 * thresh = threshold for barrier between human answer and answering machine detection
 * max_samples = max number of 8000k per second samples for human detection 
 * 
 * this routine is only used when human detection is being used to set threshholds 
 * returns 0 on success 
 */
   int arc_tones_set_answering_thresh (arc_tones_ringbuff_t * rb, float floor, int leading_ms, int trailing_ms, int thresh_ms, int slow_start_ms);

   int arc_tones_set_human_thresh (arc_tones_ringbuff_t * rb, float floor, float thresh, float recording_thresh, int max_samples);

/** arc_tones_ringbuff_post()
 *  
 * rb = ringbuff
 * buff = unencoded audio data sl16 8k samples/sec format  
 * size = size of buffers in chars
 * returns number of bytes posted to ringbuffer may not equal size
 */

   int arc_tones_ringbuff_post (arc_tones_ringbuff_t * rb, char *buff, size_t size);

/**
 *
 * rb = ringbuff
 * detect = mask of wanted detection routines  !must match in some way the ones used at init time 
 * 
 * returns mask of deteted audio signal 
 */

   int arc_tones_ringbuff_detect (arc_tones_ringbuff_t * rb, int detect, int timeout);

/** 
 * arc_tones_ringbuff_free();
 * free routine to destroy the audio contexts 
 */

   void arc_tones_ringbuff_free (arc_tones_ringbuff_t * rb);

/**
 *  these are for getting threshold results
 *  this one takes multiple modes
 */

   int arc_tones_set_result_string (arc_tones_ringbuff_t * rb, int modes);

/**
 * this one takes a single mode
 */

   char *arc_tones_get_result_string (arc_tones_ringbuff_t * rb, int mode, char *buff, size_t bufsize);


   int arc_tones_get_human_energy_results (arc_tones_ringbuff_t * rb, struct voice_energy_level_t *el);

   int arc_tones_ringbuff_unblock(arc_tones_ringbuff_t *rb);

#ifdef __cplusplus
};
#endif


#endif
