#ifndef ARC_CONFERENCE_FRAMES_DOT_H
#define ARC_CONFERENCE_FRAMES_DOT_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//#define ARC_CONF_RINGBUFFER_SIZE 128 
#define ARC_CONF_RINGBUFFER_SIZE 64


enum arc_conference_frame_state_t
{
   ARC_CONFERENCE_FRAME_STATE_START = 0,
   ARC_CONFERENCE_FRAME_STATE_BUFFERING,
   ARC_CONFERENCE_FRAME_STATE_BUFFERED,
   ARC_CONFERENCE_FRAME_STATE_HIGH_WATER,
   ARC_CONFERENCE_FRAME_STATE_RESEQUENCE
};

struct arc_conference_frame_t
{
   int state;
   char *dbl_buff;
   char *ringbuffer[ARC_CONF_RINGBUFFER_SIZE];
   int posts[ARC_CONF_RINGBUFFER_SIZE];
   int stats[ARC_CONF_RINGBUFFER_SIZE];
   int size;
   unsigned int count;
   int idx[2];
   int freeme;
};

struct arc_conference_frame_t *arc_conference_frame_init (struct arc_conference_frame_t *cf, int size);

void arc_conference_frame_free (struct arc_conference_frame_t *cf);

int arc_conference_frame_post (struct arc_conference_frame_t *cf, char *buff, size_t size, int seq);

int arc_conference_frame_read (struct arc_conference_frame_t *cf, char *buff, size_t size);

#endif
