#ifndef CALL_OUTBOUND_EVENT_HANDLERS
#define CALL_OUTBOUND_EVENT_HANDLERS

#include  <iostream>
#include <eXosip2/eXosip.h>


#define IMPORT_GLOBAL_DEFS
#include "ArcSipCommon.h"

class CallOutboundEventHandlers {

  private:

  int hold_state[MAX_PORTS];

  struct new_media_params {
    struct sockaddr_in source_rtp;
    struct sockaddr_in dest_rtp;
    int send_mode;
    int audio_codecs;
    int video_codecs;
  };

  new_media_params *media_params[MAX_PORTS];

  public:

  enum send_mode_e {
     MODE_SEND,
     MODE_RECV
  };

  enum media_types {
     MEDIA_TYPE_AUDIO, 
     MEDIA_TYPE_VIDEO
  };

  enum hold_opts_e {
    CALL_HAS_VIDEO = (1 << 0), 
    CALL_HAS_AUDIO = (1 << 1)
  };

  enum hold_state_e {
    START = 0,
    CALL_HOLD_INIT, 
    CALL_ON_HOLD, 
    CALL_RESUME_INIT, 
    CALL_HOLD_FAILED, 
    END
  };

  int Init(Call *gCall, int zCall);

  // place call on hold 
  int SetNewMediaType(Call *gCall, int zCall, int type, struct sockaddr_in *src, int codec);
  int PlaceCallOnHold(Call *gCall, int zCall);
  int ProcessCallOnHold(Call *gCall, int zCall, osip_message_t *msg);
  int ResumeCall(Call *gCall, int zCall);
  int ProcessResumeCall(Call *gCall, int zCall, osip_message_t *msg);
   // 
  int Free(int zCall);
};

#endif

