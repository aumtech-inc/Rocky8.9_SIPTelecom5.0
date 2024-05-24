#ifndef ARC_SMALL_CONFERENCE_DOT_H
#define ARC_SMALL_CONFERENCE_DOT_H


#include <pthread.h>
#include <stdio.h>

#include "arc_conference_frames.h"

#ifdef __cplusplus
extern "C"
{
#endif


#define MAX_CONFERENCE_SIZE 5

#define CONF_IN_AUDIO 0
#define CONF_OUT_AUDIO 1

   enum arc_small_conf_audio_e
   {
      ARC_SMALL_CONFERENCE_AUDIO_IN = 0,
      ARC_SMALL_CONFERENCE_AUDIO_OUT
   };

   enum arc_small_conf_options_e
   {
      ARC_SMALL_CONFERENCE_OPTION_NONE = 0,
      ARC_SMALL_CONFERENCE_OPTION_MERGE
   };

   struct arc_small_conference_t
   {
      pthread_mutex_t lock;
      char name[256];
      void *conf_delete_data[MAX_CONFERENCE_SIZE];
      int (*conf_delete_cb[MAX_CONFERENCE_SIZE]) (void *data);
      int leases[MAX_CONFERENCE_SIZE][MAX_CONFERENCE_SIZE];
      struct arc_conference_frame_t *conf_frames[2][MAX_CONFERENCE_SIZE];
      int posts[2][MAX_CONFERENCE_SIZE];
      int opts[2][MAX_CONFERENCE_SIZE];
      int seqno[2][MAX_CONFERENCE_SIZE][MAX_CONFERENCE_SIZE];
      int refcount;
   };


/* 
 * this is a util for looping through an array, while locking to find the conference by name
 * returns 1 if a match occurs
 */

   int arc_small_conference_find (struct arc_small_conference_t *c, char *name);

   int arc_small_conference_find_free (int zCall, struct arc_small_conference_t *c);

   int arc_small_conference_init (struct arc_small_conference_t *c, const char *name);

   void arc_small_conference_free (struct arc_small_conference_t *c);

   int arc_conference_add_delete_cb (struct arc_small_conference_t *c, int idx, int (cb) (void *), void *data);

   struct arc_conference_frame_t *arc_small_conference_add_ref (struct arc_small_conference_t *c, int opts, int which);

   int arc_small_conference_del_ref (struct arc_small_conference_t *c, struct arc_conference_frame_t *f, int which);

   int arc_small_conference_refcount (struct arc_small_conference_t *c);

   int arc_small_conference_post (struct arc_small_conference_t *c, struct arc_conference_frame_t *self, char *buff, size_t buffsize, int opts, int which);

   int arc_small_conference_read (struct arc_small_conference_t *c, struct arc_conference_frame_t *self, char *buff, size_t bufsize, int which);

#ifdef __cplusplus
};
#endif


#endif
