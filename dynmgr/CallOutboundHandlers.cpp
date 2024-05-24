#include  <iostream>

#include <eXosip2/eXosip.h>

#include "dynVarLog.h"

#define IMPORT_GLOBAL_EXTERNS
#include "ArcSipCommon.h"


#include "CallOutboundHandlers.hxx"

//
//  basically just build and fire messages and 
//  set call state in zCall index 
//
//  pure methods for now 
//  most data will be taken and stored in the zCall index
//
//  these routines handle signalling for platform initiated 
//  call signalling, i.e. placing a call on hold 
//


int 
CallOutboundEventHandlers::Init(Call *gCall, int zCall){

   if(zCall < 0 || zCall > MAX_PORTS){
     return -1;
   }

   hold_state[MAX_PORTS] = START;
   if(media_params[zCall]){
     free(media_params[zCall]);
     media_params[zCall] = NULL;
   }

   dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, 0, 0, "Exiting %s returning %d", __func__, 0);

   return 0;
}

int
CallOutboundEventHandlers::SetNewMediaType(Call *gCall, int zCall, int type, struct sockaddr_in *src, int codec){

  int rc = -1;

  


  return rc;
}

int
CallOutboundEventHandlers::PlaceCallOnHold(Call *gCall, int zCall){

   int rc = -1;
   int status;

   osip_message_t *msg;
   sdp_message_t  *sdp;
   char * sdp_str;
   char * body_str;
   char buff[1024];

   if(zCall < 0 || zCall > MAX_PORTS){
     return -1;
   }

   if(hold_state[zCall] != START){
     return -1;
   }

   eXosip_lock();

   eXosip_call_build_request(gCall[zCall].did, "INVITE", &msg);

   if(msg){
     // attach null sdp 
     sdp_message_init(&sdp);

     if(!sdp){
		eXosip_unlock();	//DDN 20090930
        return -1;
     }

     // sdp message 
     sdp_message_v_version_set(sdp, "0");
     sdp_message_o_origin_set(sdp, "arc", "123456", "test", "test", "test", "test");
     sdp_message_s_name_set(sdp, "session SDP");
     sdp_message_c_connection_add(sdp, 0, "test", "test", "test", "test", "test");
     sdp_message_t_time_descr_add(sdp, "0", "0");
     sdp_message_m_media_add(sdp, "audio", "0", "0", "RTP/AVP");
     sdp_message_a_attribute_add(sdp, 0, "test", "test");
     sdp_message_a_attribute_add(sdp, 1, "test", "sendonly");
     // 
     sdp_message_to_str(sdp, &sdp_str);

     if(sdp_str){
        sdp_message_free(sdp);
        osip_message_set_body(msg, sdp_str, strlen(sdp_str));
        osip_message_set_content_type (msg, "application/sdp");
     }

     status = eXosip_call_send_request(gCall[zCall].did, msg);

     if(status == 0){
       hold_state[zCall] = CALL_HOLD_INIT;
       rc = 0;
      } else {
       hold_state[zCall] = CALL_HOLD_FAILED;
      }
   }

   eXosip_unlock();

   dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, 0, 0, "Exiting %s returning %d", __func__, rc);

   return rc;
}


int 
CallOutboundEventHandlers::ProcessCallOnHold(Call *gCall, int zCall, osip_message_t *msg){

   int rc = -1;

   if(zCall < 0 || zCall > MAX_PORTS){
     return -1;
   }

   if(msg){

     if(MSG_IS_STATUS_2XX(msg) && (hold_state[zCall] == CALL_HOLD_INIT)){
        // check message/sdp and set state 
        hold_state[zCall] = CALL_ON_HOLD;
     } else {
        hold_state[zCall] = CALL_HOLD_FAILED;
     }
   }

   dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, 0, 0, "Exiting %s returning %d", __func__, rc);
   return rc;
}


int 
CallOutboundEventHandlers::ResumeCall(Call *gCall, int zCall){

   int rc = -1;
   int status;

   osip_message_t *msg;
   sdp_message_t  *sdp;
   char * sdp_str;
   char buff[1024];

   if(zCall < 0 || zCall > MAX_PORTS){
     return -1;
   }

   if(hold_state[zCall] != CALL_ON_HOLD){
      return -1;
   }

   eXosip_lock();

   eXosip_call_build_request(gCall[zCall].did, "INVITE", &msg);

   if(msg){
     // attach new sdp for call 
     sdp_message_init(&sdp);

     if(!sdp){
		eXosip_unlock();	//DDN 20090930
       return -1;
     }
     //
     sdp_message_v_version_set(sdp, "0");
     sdp_message_o_origin_set(sdp, "arc", "123456", "test", "test", "test", "test");
     sdp_message_s_name_set(sdp, "session SDP");
     sdp_message_c_connection_add(sdp, 0, "test", "test", "test", "test", "test");
     sdp_message_t_time_descr_add(sdp, "0", "0");
     sdp_message_m_media_add(sdp, "audio", "0", "0", "RTP/AVP");
     sdp_message_a_attribute_add(sdp, 0, "rtpmap:0",  "RTP/AVP");
     // 
     sdp_message_to_str(sdp, &sdp_str);

     if(sdp_str){
         sdp_message_free(sdp);
         osip_message_set_body(msg, sdp_str, strlen(sdp_str));
         osip_message_set_content_type (msg, "application/sdp");
     }

     status = eXosip_call_send_request(gCall[zCall].did, msg);
     if (status == 0){
         hold_state[zCall] = CALL_RESUME_INIT;
     } else {
         hold_state[zCall] = CALL_HOLD_FAILED;
     }
   }

   eXosip_unlock();

   dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, 0, 0, "Exiting %s returning %d", __func__, rc);
   return rc;
}

int 
CallOutboundEventHandlers::ProcessResumeCall(Call *gCall, int zCall, osip_message_t *msg){

   int rc = -1;

   if(zCall < 0 || zCall > MAX_PORTS){
     return -1;
   }

   if(msg){
     if(MSG_IS_STATUS_2XX(msg) && hold_state[zCall] == CALL_RESUME_INIT){
        // check message for sdp and see if it matches previous settings
        hold_state[zCall] = START; // can now re place the call on hold 
        rc = 0;
     } else {
        hold_state[zCall] = CALL_HOLD_FAILED;
     }
   }

   dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, 0, 0, "Exiting %s returning %d", __func__, rc);
   return rc;
}

int 
CallOutboundEventHandlers::Free(int zCall){

  int rc = -1;

  if(zCall > -1 && zCall < MAX_PORTS){
     hold_state[zCall] = START;
     if(media_params[zCall]){
        free(media_params[zCall]);
        media_params[zCall] = NULL;
     }
     rc = 0;
  }

  dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, 0, 0, "Exiting %s returning %d", __func__, rc);
  return rc;

}







