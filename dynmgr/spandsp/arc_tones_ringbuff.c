#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <math.h>

#include <errno.h>
#include <linux/soundcard.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <spandsp.h>
#include <spandsp/private/queue.h>
#include <spandsp/private/tone_generate.h>
#include <spandsp/tone_detect.h>
#include <spandsp/private/dtmf.h>
#include <spandsp/private/tone_detect.h>
#include <spandsp/dtmf.h>
#include <spandsp/dc_restore.h>

#include "arc_tones_ringbuff.h"
// debug printfs 


#define NUMSAMPLES  320

enum silence_detection_state_e
{
   BEGINNING_SILENCE = 0,
   BEGINNING_AUDIO_TIMEOUT,
   ACTIVE_AUDIO,
   BEGINNING_TRAIL_SILENCE,
   TRAILING_AUDIO_TIMEOUT
};

enum answering_detection_state_e
{
   ANSWERING_BEGINNING_SILENCE = 0,
   ANSWERING_BEGINNING_TIMEOUT,
   ANSWERING_START_OF_AUDIO,
   ANSWERING_ACTIVE_AUDIO,
   ANSWERING_START_TRAIL_SILENCE,
   ANSWERING_ACTIVE_TRAIL_SILENCE,
   ANSWERING_TRAILING_AUDIO_TIMEOUT,
   ANSWERING_END
};


#define MAX_SIT_TONE_FREQUENCIES 5

struct sit_tones_t {
     float frequencies[MAX_SIT_TONE_FREQUENCIES];
     int found[MAX_SIT_TONE_FREQUENCIES];
     goertzel_state_t g_state[MAX_SIT_TONE_FREQUENCIES];
     goertzel_descriptor_t g_descriptor[MAX_SIT_TONE_FREQUENCIES];
	 struct timeb transistions[MAX_SIT_TONE_FREQUENCIES]; 
     int state;
     int result;
};


enum sip_tone_state_e {
   SIT_TONE_START = 0,
   SIT_TONE_FIRST, 
   SIT_TONE_SECOND, 
   SIT_TONE_THIRD,
   SIT_TONE_TIMEOUT, 
   SIT_TONE_ERROR, 
   SIT_TONE_COMPLETE
};

enum sip_tone_results_e {
	SIT_TONE_FIRST_HIGH  =   (1<< 0),
	SIT_TONE_FIRST_LOW   =   (1<< 1),
	SIT_TONE_SECOND_HIGH =   (1<< 2),
	SIT_TONE_SECOND_LOW  =   (1<< 3),
	SIT_TONE_THIRD_LOW   =   (1<< 4)
};

static int 
setup_sit_tone_context(void **dest){

  int rc = -1;
  struct sit_tones_t *st;

  *dest = calloc(1, sizeof(struct sit_tones_t));

  st = *dest;

  if(st){
	 st->state = SIT_TONE_START;
     rc = 0;
  }

  return rc;
}


static void 
destroy_sit_tone_context(void *cntx){

  if(cntx){
    free(cntx);
  }

  return;
}

static int
detect_sit_tone(arc_tones_ringbuff_t * rb, void *cntx, char *buff, int size){

  int rc = 0;
  int samples;
  float results[MAX_SIT_TONE_FREQUENCIES];
  float tones[MAX_SIT_TONE_FREQUENCIES] = {985.2, 913.8, 1428.5, 1370.6, 1776.7};
  int i;
  int status;

  struct sit_tones_t *st = cntx;

  if(!st){
    fprintf(stderr, " %s() line=%d no context passed in, returning -1\n", __func__, __LINE__);
    return -1;
  }

  if (size >= 160) {

      samples = size / 2;

      if(st->state != SIT_TONE_START){
         // update all  
         for (i = 0; i < MAX_SIT_TONE_FREQUENCIES; i++) {
           status = goertzel_update (&st->g_state[i], (short *) buff, samples);
            results[i] = goertzel_result (&st->g_state[i]);
         }
      }

      switch(st->state){
         case SIT_TONE_START:
               //fprintf(stderr, " %s() in SIT_TONE_START\n", __func__);
               for(i = 0; i < MAX_SIT_TONE_FREQUENCIES; i++){
                  st->frequencies[i] = tones[i];
                  make_goertzel_descriptor (&st->g_descriptor[i], tones[i], NUMSAMPLES / 2);
                  goertzel_init (&st->g_state[i], &st->g_descriptor[i]);
               }
               st->state = SIT_TONE_FIRST;
               break;
         case SIT_TONE_FIRST:
               //fprintf(stderr, " %s() in SIT_TONE_FIRST\n", __func__);
               if((results[0] >= rb->thresh) || (results[1] >= rb->thresh)){
                  if(results[0] >= rb->thresh){
	                st->result |= SIT_TONE_FIRST_HIGH;
                  } 
                  if(results[1] >- rb->thresh){
	                st->result &= SIT_TONE_FIRST_LOW;
                  }
                  st->state = SIT_TONE_SECOND;
                 }
                 break;
         case SIT_TONE_SECOND: 
                 //fprintf(stderr, " %s() in SIT_TONE_SECOND\n", __func__);
                 if((results[2] >= rb->thresh) || (results[3] >= rb->thresh)){
                   if(results[2] >= rb->thresh){
	                  st->result |= SIT_TONE_SECOND_HIGH;
                   } 
                   if(results[3] >- rb->thresh){
	                  st->result |= SIT_TONE_SECOND_LOW;
                   }
                  st->state = SIT_TONE_THIRD;
                 }
                 break;
         case SIT_TONE_THIRD: 
                 //fprintf(stderr, " %s() in SIT_TONE_THIRD\n", __func__);
                 if(results[4] >= rb->thresh){
	                st->result |= SIT_TONE_THIRD_LOW;
                    st->state = SIT_TONE_COMPLETE;
                 }
                 break;
         case SIT_TONE_COMPLETE:
               //fprintf(stderr, " %s() in SIT_TONE_COMPLETE\n", __func__);
			   //fprintf(stderr, " %s() sip tone result=%d\n", __func__,  st->result);
               rc = ARC_TONES_SIT_TONE;
			   st->state = SIT_TONE_START;
               break;
         case SIT_TONE_TIMEOUT: 
               fprintf(stderr, " %s() in SIT_TONE_TIMEOUT\n", __func__);
               break;
         case SIT_TONE_ERROR: 
               fprintf(stderr, " %s() in SIT_TONE_ERROR\n", __func__);
               break;
         default:
				fprintf(stderr, " %s() hit unhandled default case on line %d\n", __func__, __LINE__);
				break;

      }

  }

  return rc; 
}


static int
setup_silence_context (void **dest)
{

   int rc = -1;

   *dest = calloc (1, sizeof (struct silence_detection_t));

   if (*dest) {
      rc = 0;
   }

   //fprintf(stderr, " %s() called everything should be zero\n", __func__);

   return rc;
}


int
setup_answering_context (void **dest)
{

   int rc = -1;

   *dest = calloc (1, sizeof (struct answering_detection_t));

   if (*dest) {
      rc = 0;
   }

   return rc;
}


/*
    if t1 + milliseconds is greater than tv2 i want to know about it.
*/

int
tv_compare_gt (struct timeval *tv1, struct timeval *tv2, int ms)
{

   int rc = 0;

   if (!tv1 || !tv2) {
      return 0;
   }

   unsigned long tv_sec = 0;
   unsigned long tv_usec = 0;


   if (ms > 0) {
      tv_sec = ms / 1000;
      tv_usec = (ms % 1000) * 1000;
   }

   //fprintf(stderr, " %s() tv_sec=%d tv_usec=%d\n", __func__, tv_sec, tv_usec);

   if ((tv1->tv_sec + tv_sec) > tv2->tv_sec) {
      rc = 1;
   }
   else if ((tv1->tv_sec + tv_sec) == tv2->tv_sec) {
      if ((tv1->tv_usec + tv_usec) > tv2->tv_usec) {
         rc = 1;
      }
      else {
         rc = 0;
      }
   }
   else {
      rc = 0;
   }

   //fprintf(stderr, " %s() rc=%d\n", __func__, rc);

   return rc;
}



int
detect_answering (arc_tones_ringbuff_t * rb, void *cntx, char *buff, int size)
{

   int rc = ARC_TONES_NONE;
   int samples = size / 2;
   int i;
   int accum = 0;
   short *ptr;
   struct answering_detection_t *ad = (struct answering_detection_t *) cntx;

   if (size >= 160) {

      samples = size / 2;

      for (i = 0; i < samples; i++, i++) {
         ptr = (short *) &buff[i];
         accum += (*ptr) * (*ptr);
         rb->answering_samples++;
         ad->samples++;
         ad->sample_count++;

         if (rb->answering_samples % 80 == 0) {
            ad->rms = sqrt (accum / 80);
            accum = 0;
         }
      }                         // end for loop
   }

   // fprintf(stderr, " %s() silience_floot=%f\n", __func__, rb->answering_noise_floor);

   switch (ad->state) {


   case ANSWERING_BEGINNING_SILENCE:

      if(ad->sample_count <= rb->answering_slow_start){
        break;
      }

      gettimeofday (&ad->transitions[0], NULL);
//fprintf(stderr, " %s() answering beginning silence timestamp = %ld.%ld rms=%f\n", __func__, ad->transitions[0].tv_sec, ad->transitions[0].tv_usec, ad->rms);

      if (ad->rms > rb->answering_noise_floor) {
         ad->state = ANSWERING_START_OF_AUDIO;
         ad->samples = 0;
      }

      if (ad->rms <= rb->answering_noise_floor) {
         if (ad->samples >= rb->leading_answering_samples) {
            ad->state = ANSWERING_BEGINNING_TIMEOUT;
            ad->samples = 0;
         }
      }
      break;


   case ANSWERING_BEGINNING_TIMEOUT:
      // really a no input here, but we should say something 
      // in a log
      // fprintf(stderr, " %s() beginning timeout triggered\n", __func__);
      // fprintf(stderr, " %s() results= ARC_TONES_HUMAN, reason=no input, start time= tv_sec=%d, tv_usec=%d\n", __func__, (int)ad->transitions[0].tv_sec, (int)ad->transitions[0].tv_usec);
      ad->reason = ANSWERING_BEGINNING_TIMEOUT;
      ad->state = ANSWERING_END;
      break;


   case ANSWERING_START_OF_AUDIO:
      gettimeofday (&ad->transitions[1], NULL);
      ad->state = ANSWERING_ACTIVE_AUDIO;
      //fprintf(stderr, " %s() answering start audio timestamp = %ld rms=%f\n", __func__, ad->transitions[1].tv_sec, ad->rms);

      break;

   case ANSWERING_ACTIVE_AUDIO:
      if (ad->rms <= rb->answering_noise_floor) {
         ad->state = ANSWERING_START_TRAIL_SILENCE;
         ad->samples = 0;
      } 

      if(ad->sample_count >= rb->leading_answering_samples){
            gettimeofday (&ad->transitions[2], NULL);
            ad->state = ANSWERING_END;
            ad->reason = ANSWERING_TRAILING_AUDIO_TIMEOUT;
      }
     
      break;

   case ANSWERING_START_TRAIL_SILENCE:
      gettimeofday (&ad->transitions[2], NULL);
      ad->state = ANSWERING_ACTIVE_TRAIL_SILENCE;
      break;


   case ANSWERING_ACTIVE_TRAIL_SILENCE:
      if (ad->rms >= rb->answering_noise_floor) {
         ad->state = ANSWERING_ACTIVE_AUDIO;
         ad->samples = 0;
      }

      if (ad->rms <= rb->answering_noise_floor) {
         if (ad->samples >= rb->trailing_answering_samples) {
            ad->state = ANSWERING_TRAILING_AUDIO_TIMEOUT;
         }
      }
      break;

   case ANSWERING_TRAILING_AUDIO_TIMEOUT:
      // gettimeofday(&ad->transitions[2], NULL);
      //fprintf(stderr, " %s() trailing audio timeout timestamp = %ld.%ld\n", __func__, ad->transitions[3].tv_sec, ad->transitions[3].tv_usec);
      // calulate here the end result
      //fprintf(stderr, " %s() t1=%d t2=%d t3=%d t4=%d\n", __func__, 
      //   (int)ad->transitions[0].tv_sec, (int)ad->transitions[1].tv_sec, (int)ad->transitions[2].tv_sec, (int)ad->transitions[3].tv_sec);
      ad->state = ANSWERING_END;
      ad->reason = ANSWERING_TRAILING_AUDIO_TIMEOUT;
      // human or answering 
      break;

   case ANSWERING_END:
      gettimeofday (&ad->transitions[3], NULL);

      switch (ad->reason) {

      case ANSWERING_BEGINNING_TIMEOUT:
         if(rb->results[1] != NULL){
          snprintf (rb->results[1], 256, " %s() NO-INPUT human/machine answering results t0=%d.%d t1=%d.%d t2=%d.%d t3=%d.%d\n", __func__,
                   (int) ad->transitions[0].tv_sec, (int) ad->transitions[0].tv_usec, (int) ad->transitions[1].tv_sec, (int) ad->transitions[1].tv_usec,
                   (int) ad->transitions[2].tv_sec, (int) ad->transitions[2].tv_usec, (int) ad->transitions[3].tv_sec, (int) ad->transitions[3].tv_usec);
          //fprintf (stderr, " %s() no input tiggered and beginning audio timeout \n", __func__);
         }
         rc = ARC_TONES_HUMAN;
         break;
      case ANSWERING_TRAILING_AUDIO_TIMEOUT:
         if(rb->results[1] != NULL){
         snprintf (rb->results[1], 256, " %s() human/machine answering results t0=%d.%d t1=%d.%d t2=%d.%d t3=%d.%d\n", __func__,
                   (int) ad->transitions[0].tv_sec, (int) ad->transitions[0].tv_usec, (int) ad->transitions[1].tv_sec, (int) ad->transitions[1].tv_usec,
                   (int) ad->transitions[2].tv_sec, (int) ad->transitions[2].tv_usec, (int) ad->transitions[3].tv_sec, (int) ad->transitions[3].tv_usec);
         //fprintf (stderr, " %s() trailing audio timeout triggered thresh=%d\n", __func__, rb->answering_thresh_ms);
         }
         if (tv_compare_gt (&ad->transitions[1], &ad->transitions[2], rb->answering_thresh_ms) == 0) {
            rc = ARC_TONES_RECORDING;
         }
         else {
            rc = ARC_TONES_HUMAN;
         }
         break;
      default:
         fprintf (stderr, " %s() hit default case in processing reason code, not good, should not see this\n", __func__);
         break;
      }
      break;
   default:
      fprintf (stderr, " %s() hit default case in handling answering machine detetion\n", __func__);
      break;
   }

   return rc;
}


void
destroy_answering_context (void *cntx)
{

   if (cntx) {
      free (cntx);
   }

   return;
}



static int
detect_silence (arc_tones_ringbuff_t * rb, void *cntx, char *buff, int size)
{

   int rc = ARC_TONES_NONE;

   struct silence_detection_t *sd = (struct silence_detection_t *) cntx;
   int samples;
   float accum = 0;
   short *ptr;
   int i;


   if (size >= 160) {

      samples = size / 2;

      for (i = 0; i < samples; i++, i++) {
         ptr = (short *) &buff[i];
         accum += (*ptr) * (*ptr);
         sd->samples++;

         if (sd->samples % 80 == 0) {
            sd->rms = sqrt (accum / 80);
            accum = 0;
         }
      }                         // end for loop

      // test for conditions 
      // fprintf(stderr, " %s() sd->rms=%f\n", __func__, sd->rms);

      switch (sd->state) {

      case BEGINNING_SILENCE:
//fprintf(stderr, " %s()  sd->state=BEGINNING_SILENCE; rms=%f\n", __func__, sd->rms); 
         if (sd->rms >= rb->silence_floor) {
            sd->state = ACTIVE_AUDIO;
         }

         if (sd->rms < rb->silence_floor) {
            if (sd->samples >= rb->leading_samples) {
               sd->state = BEGINNING_AUDIO_TIMEOUT;
            }
         }

         break;

      case BEGINNING_AUDIO_TIMEOUT:
//fprintf(stderr, " %s() sd->state now = BEGINNING_AUDIO_TIMEOUT rms=%f\n", __func__, sd->rms);
         rc = ARC_TONES_BEGINNING_AUDIO_TIMEOUT;
         break;

      case ACTIVE_AUDIO:
//fprintf(stderr, " %s() sd->state = ACTIVE_AUDIO\n", __func__);
         if (sd->rms < rb->silence_floor) {
            sd->state = BEGINNING_TRAIL_SILENCE;
            sd->samples = 0;
         }
         rc = ARC_TONES_ACTIVE_SPEAKER;
         break;

      case BEGINNING_TRAIL_SILENCE:
//fprintf(stderr, " %s() sd->state = BEGINNING_TRAIL_SILENCE rms=%f\n", __func__, sd->rms);
         if (sd->rms >= rb->silence_floor) {
            sd->state = ACTIVE_AUDIO;
            sd->samples = 0;
         }

         if (sd->rms <= rb->silence_floor) {
            if (sd->samples >= rb->trailing_samples) {
               sd->state = TRAILING_AUDIO_TIMEOUT;
            }
         }

         break;

      case TRAILING_AUDIO_TIMEOUT:
//fprintf(stderr, " %s() sd->state = TRAILING_AUDIO_TIMEOUT; rms=%f\n", __func__, sd->rms);
         rc = ARC_TONES_TRAILING_AUDIO_TIMEOUT;
         break;
      }
   }

   if (rb->results[8] != NULL) {
      snprintf (rb->results[8], 256, " %s() sd->rms=%f state=%d rb->trailing_timeo=%d rb->leading_timeo=%d\n", __func__, sd->rms, sd->state,
                rb->trailing_samples, rb->leading_samples);
   }

   return rc;
}



static void
destroy_silence_context (void *dest)
{

   if (dest) {
      free (dest);
   }

   return;
}



static int
setup_ringback_context (void **dest)
{

   int rc = -1;
   float tones[4] = { 440.0, 480.0, 350.0, 620.0 };
   int i;

   goertzel_descriptor_t gd[4];
   goertzel_state_t gs[4];

   //fprintf (stderr, " %s() setting up context\n", __func__);

   for (i = 0; i < 4; i++) {
      make_goertzel_descriptor (&gd[i], tones[i], NUMSAMPLES);
      goertzel_init (&gs[i], &gd[i]);
   }

   *dest = calloc (1, sizeof (gs));

   memcpy (*dest, &gs, sizeof (gs));

   return rc;
}

static int
detect_ringback (arc_tones_ringbuff_t * rb, void *cntx, char *buff, int size)
{

   int rc = ARC_TONES_NONE;
   goertzel_state_t gs[4];
   int samples;
   int status;
   int hits = 0;
   float result[4];
   int i;


   if (size >= 160) {

      memcpy (&gs, cntx, sizeof (gs));
      samples = size / 2;

      for (i = 0; i < 4; i++) {
         status = goertzel_update (&gs[i], (short *) buff, samples);
         result[i] = goertzel_result (&gs[i]);
      }

      if (result[0] >= rb->thresh && result[1] >= rb->thresh) {
         for (i = 2; i < 4; i++) {
            if (result[0] <= (result[i] + rb->diff)) {
               fprintf (stderr, " %s() found extra  hits in freq detection \n", __func__);
               hits++;
            }
            if (result[1] <= (result[i] + rb->diff)) {
               hits++;
            }
         }
      }

      if ((result[0] >= rb->thresh) && (result[1] >= rb->thresh) && (hits == 0)) {
         rc = ARC_TONES_RINGBACK;
      }

   }

   return rc;
}


static void
destroy_ringback_context (void *dest)
{

   if (dest) {
      free (dest);
   }
}




#if 0
static int
setup_energy_context (void **dest)
{

   int rc = -1;

   *dest = calloc (1, sizeof (struct voice_energy_level_t));

   if (*dest) {
      rc = 0;
   }

   return rc;
}

#endif


#if 0
static int
detect_human_energy_scatter_plot (arc_tones_ringbuff + t * rb, void *cntx, char *buff, int size)
{

   int rc = ARC_TONES_NONE;


   return rc;
}

#endif

#if 0
static inline short
slin16_quantize (short sample, short noise_thresh)
{

   short rc = 2;

   if (abs (sample) >= noise_thresh) {
      rc = 5;
   }
   return rc;
}

#endif



#define RMS_AVERAGE(samples, interval, dest, src)\
do {if((samples % interval) == 0){if(dest == 0){dest = src;} else { dest = (dest + src) / 2; }}} while(0) \

#if 0

static int
detect_human_energy_level (arc_tones_ringbuff_t * rb, void *cntx, char *buff, int size)
{

   int rc = ARC_TONES_NONE;
   int samples = 0;
   int i;
   short *ptr;
   float accum = 0;
   // float rms;
   short noise_thresh = 2048;

   struct voice_energy_level_t *el = (struct voice_energy_level_t *) cntx;

   if (size >= 80) {

      samples = size / 2;
      //if(rb->human_thresh){
      //   noise_thresh = rb->human_thresh * rb->human_thresh;
      //}

      for (i = 0; i < samples; i++, i++) {

         ptr = (short *) &buff[i];

         //accum += abs (*ptr) * abs (*ptr);
         accum += slin16_quantize (*ptr, noise_thresh) * slin16_quantize (*ptr, noise_thresh);

         if (abs (*ptr) == 0x7fff) {
            el->clips++;
         }

         el->samples++;
#if 0
         if ((el->samples % 80) == 0) {
            rms = sqrt (accum / 80);
            if (rms > el->peak_rms) {
               el->peak_rms = rms;
            }
            accum = 0;
         }
#endif
         RMS_AVERAGE (el->samples, 1000, el->a0, accum);
         RMS_AVERAGE (el->samples, 2000, el->a1, el->a0);
         RMS_AVERAGE (el->samples, 4000, el->a2, el->a1);
         RMS_AVERAGE (el->samples, 8000, el->a3, el->a2);
         RMS_AVERAGE (el->samples, 16000, el->a4, el->a3);
         /*
            RMS_AVERAGE (el->samples, 16000, el->a5, el->a4);
            RMS_AVERAGE (el->samples, 16000, el->a6, el->a5);
            RMS_AVERAGE (el->samples, 16000, el->a7, el->a6);
            RMS_AVERAGE (el->samples, 16000, el->a8, el->a7);
          */
         accum = 0;
      }

      // total rms weighted results where w = peak rms 
      // a low weight would mean that nobody has said anything 
      // just line noise seems to hover arouns 200.00/80 samples or so
      // 


      if (el->samples > rb->samples && el->a4 != 0.0) { // at least two seconds of audio 

         //el->total_rms = ((el->a1 * el->peak_rms) + (el->a2 * el->peak_rms) + (el->a3 * el->peak_rms) + (el->a4 * el->peak_rms)) / 4;
         //el->total_rms = ((el->a1 * el->peak_rms) * (el->a2 * el->peak_rms) * (el->a3 * el->peak_rms) * (el->a4 * el->peak_rms)) / 4;
         el->total_rms = (el->a1) * (el->a2) * (el->a3) * (el->a4);

         if (rb->results[1] != NULL) {
            snprintf (rb->results[1], 256, " %s() a1=%f a2=%f a3=%f a4=%f weighted total=%f rms_peak/80=%f sample count=%ld hard clips=%ld samples=%ld",
                      __func__, el->a1, el->a2, el->a3, el->a4, el->total_rms, el->peak_rms, el->samples, el->clips, el->samples);
         }


         if (el->peak_rms >= rb->human_floor) {

            if (el->total_rms >= rb->human_thresh && el->total_rms < rb->recording_thresh) {
               rc = ARC_TONES_HUMAN;
               memset (cntx, 0, sizeof (struct voice_energy_level_t));
            }
            else if (el->total_rms >= rb->recording_thresh) {
               memset (cntx, 0, sizeof (struct voice_energy_level_t));
               rc = ARC_TONES_RECORDING;
            }
         }
         else {
            rc = ARC_TONES_HUMAN;
         }

         memset (el, 0, sizeof (struct voice_energy_level_t));
      }

   }

   return rc;
}


#endif

#if 0

void
destroy_energy_context (void *dest)
{

   if (dest) {
      free (dest);
   }
   return;
}


#endif



/*
 *
 * DTMF detection 
 * These are here for testing purposes mostly
 * it is the easiest feature to test using the the spandsp library
 *
*/

static int
setup_inband_dtmf_context (void **dest)
{

   int rc = -1;

   dtmf_rx_state_t ds;
   dtmf_rx_init (&ds, NULL, NULL);

   *dest = calloc (1, sizeof (ds));

   if (*dest) {
      memcpy (*dest, &ds, sizeof (ds));
      rc = 0;
   }

   return rc;
}

static int
detect_inband_dtmf (arc_tones_ringbuff_t * rb, void *cntx, char *buff, int size)
{

   int rc = ARC_TONES_NONE;
   int samples;
   int status;
   int count;


   if (size >= 160) {
      samples = size / 2;
      dtmf_rx (cntx, (short *) buff, samples);
      status = dtmf_rx_status (cntx);
      if (status != 'x') {
         count = dtmf_rx_get (cntx, rb->dtmf, sizeof (rb->dtmf));
      }

      if (count) {
         rc = ARC_TONES_INBAND_DTMF;
      }
   }

   return rc;
}


void
destroy_inband_dtmf_context (void *cntx)
{

   if (cntx != NULL) {
      free (cntx);
   }

   return;
}

/*
 *
 * END DTMF 
 *
*/


/*
 *
 * FAX CED detection
 * This is for detecting an inbound dialing fax machine
*/


static int 
setup_fax_ced_context(void **dest){

   int rc = -1;
   float tones[7] = { 2100.0, 697.0, 770.0, 852.0, 941.0, 350.0, 440.0 };
   int i;

   goertzel_descriptor_t gd[7];
   goertzel_state_t gs[7];

#ifdef DECODER_DEBUG
   fprintf (stderr, " %s() setting up context\n", __func__);
#endif


   for (i = 0; i < 7; i++) {
      make_goertzel_descriptor (&gd[i], tones[i], NUMSAMPLES);
   }

   for (i = 0; i < 7; i++) {
      goertzel_init (&gs[i], &gd[i]);
   }

   *dest = calloc (1, sizeof (gs));

   if (*dest) {
      memcpy (*dest, &gs, sizeof (gs));
      rc = 0;
   }

   return rc;
}

static int 
detect_fax_ced(arc_tones_ringbuff_t *rb, void *cntx, char *buff, int size){

   int rc = ARC_TONES_NONE;
   int samples;
   int status;
   float result[7];
   goertzel_state_t gs[7];
   int i;
   int hits = 0;

   static int count = 0;


   if (size >= 160) {

      memcpy (&gs, cntx, sizeof (gs));
      samples = size / 2;

      for (i = 0; i < 7; i++) {
         status = goertzel_update (&gs[i], (short *) buff, samples);
         result[i] = goertzel_result (&gs[i]);
      }

      if (result[0] >= rb->thresh) {
         for (i = 1; i < 7; i++) {
            if (result[0] <= (result[i] + rb->diff)) {
#ifdef TONES_DEBUG
               fprintf (stderr, " %s() found extra  hits in freq detection \n", __func__);
#endif
               hits++;
            }
         }
      }
#if 0
	  if((count % 1000) == 0){
      fprintf (stderr, " %s() r0=%f r1=%f r2=%f r3=%f r4=%f r5=%f r6=%f status=%d hits=%d", __func__, result[0], result[1], result[2],
                result[3], result[4], result[5], result[6], status, hits);
      }

#endif 
      count++;


      if ((result[0] >= rb->thresh) && (hits == 0)) {
         rc = ARC_TONES_FAX_CED;
      }

   }

   if (rb->results[9]) {
      snprintf (rb->results[8], 256, " %s() r0=%f r1=%f r2=%f r3=%f r4=%f r5=%f r6=%f status=%d hits=%d", __func__, result[0], result[1], result[2],
                result[3], result[4], result[5], result[6], status, hits);
   }

   return rc;
}

void 
destroy_fax_ced_context(void *context){

   if (context) {
      free (context);
   }
   return;
}


/*
  FAX CED Detection 

*/


static int
setup_fax_tone_context (void **dest)
{

   int rc = -1;
   float tones[7] = { 2100.0, 697.0, 770.0, 852.0, 941.0, 350.0, 440.0 };
   int i;

   goertzel_descriptor_t gd[7];
   goertzel_state_t gs[7];

#ifdef DECODER_DEBUG
   fprintf (stderr, " %s() setting up context\n", __func__);
#endif


   for (i = 0; i < 7; i++) {
      make_goertzel_descriptor (&gd[i], tones[i], NUMSAMPLES);
   }

   for (i = 0; i < 7; i++) {
      goertzel_init (&gs[i], &gd[i]);
   }

   *dest = calloc (1, sizeof (gs));

   if (*dest) {
      memcpy (*dest, &gs, sizeof (gs));
      rc = 0;
   }

   return rc;
}


static int
detect_fax_tone (arc_tones_ringbuff_t * rb, void *cntx, char *buff, int size)
{

   int rc = ARC_TONES_NONE;
   int samples;
   int status;
   float result[7];
   goertzel_state_t gs[7];
   int i;
   int hits = 0;


   if (size >= 160) {

      memcpy (&gs, cntx, sizeof (gs));
      samples = size / 2;

      for (i = 0; i < 7; i++) {
         status = goertzel_update (&gs[i], (short *) buff, samples);
         result[i] = goertzel_result (&gs[i]);
      }

      if (result[0] >= rb->thresh) {
         for (i = 1; i < 7; i++) {
            if (result[0] <= (result[i] + rb->diff)) {
#ifdef TONES_DEBUG
               fprintf (stderr, " %s() found extra  hits in freq detection \n", __func__);
#endif
               hits++;
            }
         }
      }

      if ((result[0] >= rb->thresh) && (hits == 0)) {
         rc = ARC_TONES_FAX_TONE;
      }

   }

   if (rb->results[4]) {
      snprintf (rb->results[4], 256, " %s() r0=%f r1=%f r2=%f r3=%f r4=%f r5=%f r6=%f status=%d hits=%d", __func__, result[0], result[1], result[2],
                result[3], result[4], result[5], result[6], status, hits);
   }

   return rc;
}

void
destroy_fax_tone_context (void *context)
{
   if (context) {
      free (context);
   }
   return;
}

/*
 * END FAX tone detection 
*/




/*
 *  Fast Busy detection 
 */


static int
setup_fast_busy_context (void **dest)
{

   int rc = -1;
   int i;

   float tones[4] = { 480.0, 620.0, 350.0, 440.0 };
   goertzel_descriptor_t gd[4];
   goertzel_state_t gs[4];

   for (i = 0; i < 4; i++) {
      make_goertzel_descriptor (&gd[i], tones[i], NUMSAMPLES);
      goertzel_init (&gs[i], &gd[i]);
   }

#ifdef TONES_DEBUG
   fprintf (stderr, " %s() setting up context size=%d\n", __func__, sizeof (gs));
#endif

   *dest = calloc (1, sizeof (gs));

   if (*dest) {
      memcpy (*dest, &gs, sizeof (gs));
   }

   return rc;
}

static int
detect_fast_busy (arc_tones_ringbuff_t * rb, void *cntx, char *buff, int size)
{

   int rc = ARC_TONES_NONE;
   float result[4];
   int samples;
   int hits = 0;
   int i;

   goertzel_state_t gs[4];

   // remove this copy after testing 
   memcpy (&gs, cntx, sizeof (gs));

   if (size >= 160) {

      samples = size / 2;

      for (i = 0; i < 4; i++) {
         goertzel_update (&gs[i], (short *) buff, samples);
         result[i] = goertzel_result (&gs[i]);
      }

      if (result[0] > rb->thresh && result[1] > rb->thresh) {
#ifdef TONES_DEBUG
         fprintf (stderr, " %s() buff=%p size=%d result[0]=%f result[1]=%f\n", __func__, buff, size, result[0], result[1]);
#endif
         for (i = 2; i < 4; i++) {
            if (result[i] + rb->diff > result[0]) {
               hits++;
            }
            if (result[i] + rb->diff > result[1]) {
               hits++;
            }
         }
         if (hits == 0) {
            rc = ARC_TONES_FAST_BUSY;
         }
      }

   }

   if (rb->results[3] != NULL) {
      snprintf (rb->results[3], 256, " %s() r0=%f r1=%f r2=%f r3=%f hits=%d", __func__, result[0], result[1], result[2], result[3], hits);
   }

   return rc;
}

void
destroy_fast_busy_context (void *context)
{

   if (context) {
      free (context);
   }
   return;
}


/*
 * END fast busy routines 
 */



static int
setup_dial_tone_context (void **dest)
{

   int rc = -1;
   int i;

   float tones[4] = { 350.0, 440.0, 480.0, 620.0 };
   goertzel_descriptor_t gd[4];
   goertzel_state_t gs[4];

   for (i = 0; i < 4; i++) {
      make_goertzel_descriptor (&gd[i], tones[i], NUMSAMPLES);
      goertzel_init (&gs[i], &gd[i]);
   }
#ifdef TONES_DEBUG
   fprintf (stderr, " %s() setting up context size=%d\n", __func__, sizeof (gs));
#endif

   *dest = calloc (1, sizeof (gs));

   if (*dest) {
      memcpy (*dest, &gs, sizeof (gs));
   }

   return rc;
}


int
detect_dial_tone (arc_tones_ringbuff_t * rb, void *cntx, char *buff, int size)
{

   int rc = ARC_TONES_NONE;
   int samples;
   int hits = 0;
   int i;

   goertzel_state_t gs[4];
   float result[4];

   if (size >= 160) {

      memcpy (&gs, cntx, sizeof (gs));

      samples = size / 2;

      for (i = 0; i < 4; i++) {
         goertzel_update (&gs[i], (short *) buff, samples);
         result[i] = goertzel_result (&gs[i]);
      }

      if (result[0] >= rb->thresh && result[1] >= rb->thresh) {
         for (i = 2; i < 4; i++) {
            if ((result[i] + rb->diff) > result[0]) {
               hits++;
            }
            if ((result[i] + rb->diff) > result[1]) {
               hits++;
            }
         }

         if (hits == 0) {
            rc = ARC_TONES_DIAL_TONE;
         }
      }

   }

   if (rb->results[6] != NULL) {
      snprintf (rb->results[6], 256, " %s() r0=%f r1=%f r2=%f r3=%f hits=%d", __func__, result[0], result[1], result[2], result[3], hits);
   }

   return rc;
}


void
destroy_dial_tone_context (void *cntx)
{

   if (cntx) {
      free (cntx);
   }

   return;
}






int
arc_tones_ringbuff_init (arc_tones_ringbuff_t * rb, int detect, float thresh, float diff, int threaded)
{

   int rc = -1;
   int i;
   static int (*context_cb[32]) (void **dest) = {
         setup_answering_context, 
         /* this one is always null */
         NULL,
         setup_fast_busy_context,
         setup_fax_tone_context,
         setup_inband_dtmf_context,
         setup_dial_tone_context,
         setup_ringback_context,
         setup_silence_context,
         NULL, 
         NULL, 
         setup_fax_ced_context, 
         NULL, 
         setup_sit_tone_context, 
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

   static int (*detect_cb[32]) (arc_tones_ringbuff_t * rb, void *cntx, char *buff, int size) = {
   		 detect_answering,
         NULL,
         detect_fast_busy,
         detect_fax_tone,
         detect_inband_dtmf,
         detect_dial_tone,
         detect_ringback,
         detect_silence,
		 NULL, 
         NULL, 
         detect_fax_ced, 
         NULL, 
         detect_sit_tone, 
         NULL, 
         NULL, 
         NULL, 
         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
         NULL, NULL, NULL};



   if (!rb) {
      return -1;
   }

   memset (rb, 0, sizeof (arc_tones_ringbuff_t));

   dc_restore_init(&rb->dc_state);

   if (threaded) {
      pthread_mutex_init (&rb->lock, NULL);
      pthread_cond_init (&rb->cond, NULL);
      rb->use_threads++;
   }

   rb->thresh = thresh;
   rb->diff = diff;

   for (i = 0; i < 32; i++) {
      if ((1 << i) & detect) {
#ifdef TONES_DEBUG
         fprintf (stderr, " %s() setting up context for %i\n", __func__, i);
#endif
         if (context_cb[i] != NULL) {
            context_cb[i] (&rb->contexts[i]);
         }
         if (detect_cb[i] != NULL) {
#ifdef TONES_DEBUG
            fprintf (stderr, " %s() setting up callback for %i\n", __func__, i);
#endif
            rb->cbs[i] = detect_cb[i];
         }
      }
   }

   return rc;
}


int
arc_tones_set_human_thresh (arc_tones_ringbuff_t * rb, float floor, float thresh, float recording_thresh, int maxsamples)
{

   int rc = -1;

   if (rb) {
      rb->human_floor = floor;
      rb->human_thresh = thresh;
      rb->recording_thresh = recording_thresh;
      rb->samples = maxsamples;
      rc = 0;
   }

   if (rb->samples > 8000) {
      rb->timeo = rb->samples / 8000;
   }

   return rc;
}

int
arc_tones_set_answering_thresh (arc_tones_ringbuff_t * rb, float floor, int leading_ms, int trailing_ms, int thresh_ms, int slow_start_ms)
{

   int rc = -1;
  
   // some are done as samples  i.e. ms * 8 = (1000 * 8) == 8000
   // others are actual milliseconds

   if (rb) {
      rb->answering_noise_floor = floor;
      rb->leading_answering_samples = leading_ms * 8;
      rb->trailing_answering_samples = trailing_ms * 8;
      rb->answering_samples = leading_ms * 8;
      rb->answering_thresh_ms = thresh_ms;
      rb->answering_slow_start = slow_start_ms * 8;
   }

   return rc;
}

int
arc_tones_set_silence_params (arc_tones_ringbuff_t * rb, float floor, int leading, int trailing)
{

   int rc = -1;

   if (rb) {
      rb->silence_floor = floor;
      rb->leading_samples = leading;
      rb->trailing_samples = trailing;
   }

   return rc;
}

int arc_tones_set_leading_samples(arc_tones_ringbuff_t * rb, int leading)           // GS - google streaming
{
	if (rb)
	{
		rb->leading_samples = leading;
	}
	return(0);
} // END: arc_tones_set_leading_samples()

int arc_tones_increment_leading_samples(arc_tones_ringbuff_t * rb, int incrementVal)	// GS - google streaming
{
	if (rb)
	{
		rb->leading_samples = rb->leading_samples + incrementVal;
	}
	return(0);
} // END: arc_tones_increment_leading_samples()


static inline int 
arc_dc_restore(arc_tones_ringbuff_t * rb, char *buff, size_t size){

  int rc = -1;
  short *ptr = (short *)&buff[0];
  int i;

  if(ptr){
      rc = 0;
      for(i = 0; i < size; i += 2){
          ptr = (short *)&buff[i];
          //fprintf(stderr, " %s() before *ptr=%d\n", __func__, *ptr);
          *ptr = dc_restore(&rb->dc_state, *ptr);
          // fprintf(stderr, " %s() after *ptr=%d\n", __func__, *ptr);
          rc++;
      }
  }
  return rc;
}



int
arc_tones_ringbuff_post (arc_tones_ringbuff_t * rb, char *buff, size_t size)
{

   int rc = -1;
   int bytes = sizeof (rb->buff);

   if (!rb) {
      return -1;
   }

   if (size > sizeof (rb->buff)) {
#ifdef DECODER_DEBUG
      fprintf (stderr, " %s() posted buffer size is larger than buffer returning -1\n", __func__);
#endif
      return -1;
   }

#ifdef DECODER_DEBUG
#ifdef BITS64
   fprintf (stderr, " %s() rb=%p buff=%p size=%ld\n", __func__, rb, buff, size);
#else
   fprintf (stderr, " %s() rb=%p buff=%p size=%d\n", __func__, rb, buff, size);
#endif
#endif

   if (rb->use_threads) {
      pthread_mutex_lock (&rb->lock);
   }

   bytes = sizeof (rb->buff) - rb->idx[WRITE];

   // fprintf(stderr, " %s() bytes=%d write index = %d size=%d\n", __func__, bytes, rb->idx[WRITE], size);


   if (bytes && (bytes < size)) {

      arc_dc_restore(rb, buff, bytes);

      memcpy (&rb->buff[rb->idx[WRITE]], buff, bytes);
      // fallback case 
      rc = bytes;
      int remaining = size - bytes;
      memcpy (&rb->buff[0], &buff[bytes], remaining);
      rb->idx[WRITE] = remaining % sizeof (rb->buff);
      rc += remaining;
   }
   else if (bytes && (bytes >= size)) {
      memcpy (&rb->buff[rb->idx[WRITE]], buff, size);
      rb->idx[WRITE] = (rb->idx[WRITE] + size) % sizeof (rb->buff);
      rc = size;
   }
   else if (bytes == 0) {
      memcpy (&rb->buff[0], buff, size);
      rb->idx[WRITE] = size % sizeof (rb->buff);
      rc = size;
   }

   if (rb->use_threads) {
      if (rc > 0) {
         pthread_cond_signal (&rb->cond);
      }

      pthread_mutex_unlock (&rb->lock);
   }

   // fprintf(stderr, " %s() rc = %d\n", __func__, rc);

   rb->last_post = rc;

   return rc;
}

int 
arc_tones_ringbuff_unblock(arc_tones_ringbuff_t *rb){

   if (rb->use_threads) {
      pthread_cond_signal (&rb->cond);
      pthread_mutex_unlock (&rb->lock);
   }

   return 0;
}

/*
** Maxsamples == -1, ignore sample count 
*/

int
arc_tones_ringbuff_detect (arc_tones_ringbuff_t * rb, int detect, int timeout)
{

   int rc = ARC_TONES_NONE;
   int bytes = 0;
   int i;
   char *ptr;
   struct timeval now;
   struct timespec timeo;
   int t;

   if (!rb) {
      return ARC_TONES_NONE;
   }

   gettimeofday (&now, NULL);

   timeo.tv_sec = now.tv_sec + timeout;
   timeo.tv_nsec = 0;

   if (rb->use_threads) {
      pthread_mutex_lock (&rb->lock);

      t = pthread_cond_timedwait (&rb->cond, &rb->lock, &timeo);

      if (t == ETIMEDOUT) {
         pthread_mutex_unlock (&rb->lock);
         return ARC_TONES_PACKET_TIMEOUT;
      }
   }

   if (rb->idx[READ] < rb->idx[WRITE]) {
      bytes = rb->idx[WRITE] - rb->idx[READ];
   }
   else if (rb->idx[READ] > rb->idx[WRITE]) {
      bytes = sizeof (rb->buff) - rb->idx[READ];
   }

   if (bytes) {

      ptr = &rb->buff[rb->idx[READ]];

      for (i = 0; i < 32; i++) {
         if (((1 << i) & detect) && (rb->contexts[i] != NULL)) {
            if (rb->cbs[i] != NULL) {
               rc |= rb->cbs[i] (rb, rb->contexts[i], ptr, bytes);
               rb->detected |= rc;
            }
         }
      }

      if (bytes) {
         rb->idx[READ] = (rb->idx[READ] + bytes) % sizeof (rb->buff);
      }
   }

   if (rb->use_threads) {
      pthread_mutex_unlock (&rb->lock);
   }

   return rc;
}


int
arc_tones_set_result_string (arc_tones_ringbuff_t * rb, int modes)
{

   int rc = -1;
   int i;

   if (rb->use_threads) {
      pthread_mutex_lock (&rb->lock);
   }

   for (i = 0; i < 32; i++) {
      rb->results[i] = calloc (1, 257);
   }

   if (rb->use_threads) {
      pthread_mutex_unlock (&rb->lock);
   }

   return rc;
}


char *
arc_tones_get_result_string (arc_tones_ringbuff_t * rb, int mode, char *buff, size_t bufsize)
{

   char *rc = NULL;

   if (rb->use_threads) {
      pthread_mutex_lock (&rb->lock);
   }

   switch (mode) {
   case ARC_TONES_HUMAN:
   case ARC_TONES_RECORDING:
      snprintf (buff, bufsize, "%s", rb->results[1]);
      break;
   case ARC_TONES_FAST_BUSY:
      snprintf (buff, bufsize, "%s", rb->results[3]);
      break;
   case ARC_TONES_FAX_TONE:
      snprintf (buff, bufsize, "%s", rb->results[4]);
      break;
   case ARC_TONES_FAX_CED:
      snprintf (buff, bufsize, "%s", rb->results[9]);
      break;
   case ARC_TONES_INBAND_DTMF:
      snprintf (buff, bufsize, "%s", rb->results[5]);
      break;
   case ARC_TONES_DIAL_TONE:
      snprintf (buff, bufsize, "%s", rb->results[6]);
      break;
   case ARC_TONES_RINGBACK:
      snprintf (buff, bufsize, "%s", rb->results[7]);
      break;
   case ARC_TONES_BEGINNING_AUDIO_TIMEOUT:
   case ARC_TONES_TRAILING_AUDIO_TIMEOUT:
      snprintf (buff, bufsize, "%s", rb->results[8]);
      break;
   case ARC_TONES_SIT_TONE:
      snprintf(buff, bufsize, "%s", rb->results[12]);
      break;
   default:
      fprintf (stderr, " %s() unhandled case statement or invalid mode, only one can be used at a time\n", __func__);
   }

   rc = buff;

   if (rb->use_threads) {
      pthread_mutex_unlock (&rb->lock);
   }

   return rc;

}

//
// this is called when 
// silience suppresion is used and is not likely to continue to send packets 
// after an initial "hello"

int
arc_tones_get_human_energy_results (arc_tones_ringbuff_t * rb, struct voice_energy_level_t *el)
{

   int rc = -1;

   if (!rb) {
      return -1;
   }

   if (!el) {
      return -1;
   }

   memset (el, 0, sizeof (struct voice_energy_level_t));

   if (rb->use_threads) {
      pthread_mutex_lock (&rb->lock);
   }

   if (rb->contexts[1] && el) {
      memcpy (el, rb->contexts[1], sizeof (struct voice_energy_level_t));
      rc = 0;
   }

   if (rb->use_threads) {
      pthread_mutex_unlock (&rb->lock);
   }

   return rc;
}



void
arc_tones_ringbuff_free (arc_tones_ringbuff_t * rb)
{

   int i;


   if (rb->use_threads) {
      pthread_mutex_lock (&rb->lock);
   }

   void (*cb[32]) (void *dst) = {
         destroy_answering_context,       /*0*/
         NULL,                            /*1*/
         destroy_fast_busy_context,       /*2*/
         destroy_fax_tone_context,        /*3*/
         destroy_inband_dtmf_context,     /*4*/
         destroy_dial_tone_context,       /*5*/
         destroy_ringback_context,        /*6*/
         destroy_silence_context,         /*7*/ 
         NULL,        					  /*8*/
         NULL,							  /*9*/
         destroy_fax_ced_context,         /*10*/
         NULL,         /*11*/
         destroy_sit_tone_context,        /* 12 */
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL, 
         NULL,
         NULL, 
         NULL, 
         NULL, 
         NULL};

   // free up set up contexts 
   for (i = 0; i < 32; i++) {
      if (rb->contexts[i] != NULL) {
         //if(i == 4){
         //  fprintf(stderr, " %s() freeing context[%p] at index [%d]\n", __func__, rb->contexts[i], i);
         //}
         cb[i] (rb->contexts[i]);
      }
   }

   // added later 

   for (i = 0; i < 32; i++) {
      if (rb->results[i]) {
         free (rb->results[i]);
      }
   }

   return;
}


#ifdef MAIN


struct read_thread_t
{
   int audio_fd;
   arc_tones_ringbuff_t *rb;
};

struct read_thread_t thread_args;

void *
reader_thread (void *args)
{

   char buff[1024];
   int size;
   struct read_thread_t *thread_args = args;

   while (1) {
      size = read (thread_args->audio_fd, buff, 320);
      if (size > 0) {
         arc_tones_ringbuff_post (thread_args->rb, buff, size);
      }
      else if (size == 0) {
#ifdef DECODER_DEBUG
         fprintf (stderr, " %s() end of file reached ending reader thread\n", __func__);
#endif
         return NULL;
      }
      else {
#ifdef DECODER_DEBUG
         fprintf (stderr, " %s() error reading from file descriptor \n", __func__);
#endif
         return NULL;
      }
   }

   return NULL;
}


void
usage (const char *progname)
{
   fprintf (stderr, "\n usage: %s <audio device> \n\n", progname);
}


arc_tones_ringbuff_t rb;


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
#ifdef DECODER_DEBUG
         fprintf (stderr, " %s() failed to set to single channel: reason = %s", __func__, strerror (errno));
#endif
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

   pthread_t tid;
   int fd = 0;
   int status;
   float thresh = 3415905148928.000000;
   float diff = 100.000000;
   int count = 0;
   char buff[1024];

   int detect = ARC_TONES_HUMAN | ARC_TONES_FAST_BUSY | ARC_TONES_FAX_TONE | ARC_TONES_INBAND_DTMF | ARC_TONES_DIAL_TONE;
   // int detect = ARC_TONES_FAST_BUSY | ARC_TONES_FAX_TONE | ARC_TONES_INBAND_DTMF | ARC_TONES_DIAL_TONE;

   if (argc < 2) {
      usage (argv[0]);
      exit (1);
   }

   fd = audio_init (argv[1], 1, 16, 8000, O_RDONLY);

   if (fd == -1) {
      fprintf (stderr, "failed to open audio pipe for raw audio stream\n");
      exit (1);
   }

   arc_tones_ringbuff_init (&rb, detect, thresh, diff, 1);

   // arc_tones_set_human_thresh (&rb, 3000.0, 1000000.0, 9000000.00, 8000 * 3);
   // these are in milliseconds 
   arc_tones_set_answering_thresh (&rb, 200.0, 2000, 1000, 1250, 2222); //RGD

   thread_args.audio_fd = fd;
   thread_args.rb = &rb;

   pthread_create (&tid, NULL, reader_thread, (void *) &thread_args);
   arc_tones_set_result_string (&rb, ARC_TONES_HUMAN);

#define ONE_SECOND 8000

   while (1) {

      fprintf (stderr, "hit any key to start\n");
      int t;

      t = getc (stdin);

      count = 1;

      while (count) {

         status = arc_tones_ringbuff_detect (&rb, detect, 10);

         if (status == ARC_TONES_NONE) {
            ;;
         }
         if (status & ARC_TONES_FAST_BUSY) {
            fprintf (stderr, " detected ARC_TONES_FAST_BUSY\n");
         }
         if (status & ARC_TONES_DIAL_TONE) {
            fprintf (stderr, " detected ARC_TONES_DIAL_TONE\n");
         }
         if (status & ARC_TONES_FAX_TONE) {
            fprintf (stderr, " detected ARC_TONES_FAX_TONE\n");
         }
         if (status & ARC_TONES_INBAND_DTMF) {
            fprintf (stderr, " detected ARC_TONES_INBAND_DTMF dtmf keys=[%s]\n", rb.dtmf);
         }
         if (status & ARC_TONES_HUMAN) {
            arc_tones_get_result_string (&rb, ARC_TONES_HUMAN, buff, sizeof (buff));
            fprintf (stderr, " detected ARC_TONES_HUMAN result=%s\n", buff);
            break;
            detect ^= ARC_TONES_HUMAN;
            count = 0;
         }
         if (status & ARC_TONES_RECORDING) {
            arc_tones_get_result_string (&rb, ARC_TONES_HUMAN, buff, sizeof (buff));
            fprintf (stderr, " detected ARC_TONES_RECORDING result=%s\n", buff);
            break;
            detect ^= ARC_TONES_HUMAN;
            count = 0;
         }
      }
   }

   return 0;
}


#endif
