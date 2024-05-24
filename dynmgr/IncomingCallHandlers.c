#include <stdio.h>
#include <eXosip2/eXosip.h>


#define IMPORT_GLOBAL_EXTERNS
#include "ArcSipCommon.h"

#include "dynVarLog.h"

#include "IncomingCallHandlers.h"
#include "InboundRegistrationInfo.h"
#include "SubscriptionHandlers.h"
#include "SubscriptionInfo.h"
#include "dtmf_utils.h"
#include "CallOptions.hxx"

extern int parseSdpMessage (int zCall, int zLine, char *zStrMessage, sdp_message_t *);

extern ActiveInboundRegistrations_t *ActiveRegistrationList;
extern SubscriptionInfoList_t *SubscriptionInfoList;
extern struct eXosip_t *geXcontext;
extern CallOptions gCallInfo[];
extern int setSessionTimersResponse(int zCall, int zLine, char *zMod, osip_message_t *response);

// this handles EXOSIP_CALL_MESSAGE_NEW 
// for any number of incoming call messages that 
// are not either part of the invite


int incomingReInviteDispatch(eXosip_event_t *eXosipEvent, int zCall)
{

  //
  // here at first we will just print it out and determine
  // whether or not it is a hold or an actual change in media 
  //

  int rc = -1;
  char new_ip[20];
  unsigned int new_port = 0;
  sdp_message_t new_sdp;
  char *sdp_string = NULL;
  char *message_str = NULL;
  size_t message_len;
  osip_message_t *reinvite;


  if(eXosipEvent && eXosipEvent->request){

      reinvite = eXosipEvent->request;

      osip_message_to_str(reinvite, &message_str, &message_len);

      if(message_str){
           free(message_str);
      }

  }

  return rc;

}


int 
outgoingReInviteDispatch(int did, int zCall, int zCrossPort){

  int rc = -1;
  osip_message_t *re_invite;

  eXosip_lock(geXcontext);

  if(did != -1){
    eXosip_call_build_request(geXcontext, did, "INVITE", &re_invite);
    if(re_invite){
       // pack message and send 
       // will need a refers header with to/from tags
       rc = eXosip_call_send_request(geXcontext, did, re_invite);
    }
  }

  eXosip_unlock(geXcontext);
  
  return rc;
}

int incomingUpdateHandler (eXosip_event_t * eXosipEvent, int zCall)
{
  static char mod[] = "incomingUpdateHandler";
  int rc = 0;
  osip_message_t *msg = NULL;
  osip_message_t *resp = NULL;
  sdp_message_t *remote_sdp = NULL;
  sdp_media_t *audio_media = NULL;
  char dnis[256];
  char ani[256];
  int status = 0;

#if 0	// MR 4896
  if(gCall[zCall].SentOk == 1){

      eXosip_lock (geXcontext);
      eXosip_call_build_answer (geXcontext, eXosipEvent->tid, 488, &resp);

      if (resp) {
          rc = eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 488, resp);
      }
      eXosip_unlock (geXcontext);

      return 0;
  }
#endif

  char *sdp;

  msg = eXosipEvent->request;

  if (msg) {
    remote_sdp = eXosip_get_sdp_info (msg);
  }
  else {
    return -1;
  }

  // update gCall info

  if (remote_sdp) {

    audio_media = eXosip_get_audio_media (remote_sdp);
    sdp_message_to_str (remote_sdp, &sdp);
    snprintf (gCall[zCall].sdp_body_remote,
              sizeof (gCall[zCall].sdp_body_remote), "%s", sdp);
    gCall[zCall].remoteRtpPort = atoi (audio_media->m_port);

    rc = parseSdpMessage (zCall, __LINE__, gCall[zCall].sdp_body_remote, remote_sdp);

    sdp_message_free (remote_sdp);
    free (sdp);
    if (rc) {
      status = 415;
    }
    else {
      status = 200;
    }
  }
  else {
    // should be something else
    //status = 415;
	/*
		Implement various updated params here, e.g.

			Session-Expires: 180;refresher=uac^M
			P-Charging-Vector: icid-value="AAS:852-e271b46ae741b7ff5000d68f8ffab56"^M
			Av-Global-Session-ID: 6ab47124-ffb7-41e7-8fd5-005056abff08^M
	*/
    if ( (rc = gCallInfo[zCall].ProcessSessionTimers (mod, __LINE__, zCall, eXosipEvent)) == -1)
    {
        status = 422;
    }
    else
    {
        status = 200;
    }
  }
  eXosip_lock (geXcontext);
  eXosip_call_build_answer (geXcontext, eXosipEvent->tid, status, &resp);

  if (resp) {

    rc = setSessionTimersResponse(zCall, __LINE__, mod, resp);

    rc = eXosip_call_send_answer (geXcontext, eXosipEvent->tid, status, resp);
  }
  eXosip_unlock (geXcontext);

  if ( status != 200 )
  {
    return(-1);
  }
  else
  {
    return(0);
  }

} // END: incomingUpdateHandler ()

int
incomingCallMessageDispatch (eXosip_event_t * eXosipEvent, int zCall)
{
  //
  // ok this should be changed to only dispatch based on the type of message 
  // filtering for content type should be done in the individual handlers   
  //

  osip_message_t *msg = ( osip_message_t * )NULL;
  osip_message_t *resp = NULL;
  int rc = 0;

  int i;

  char *contents[] = {
    "PlaceHolder",
    "application/dtmf-relay",
    "application/sdp",
    "message/sipfrag",
    NULL
  };

  enum _content_types
  {
    NO_CONTENT_TYPE = 0,
    APPLICATION_DTMF_RELAY,
    APPLICATION_SDP,
	MESSAGE_SIPFRAG,
    //
    CONTENT_TYPE_END
  };

  int msg_type = 0;
  int con_type = 0;
  char *content_type = NULL;

  enum types_e
  {
    INCOMING_MSG_UPDATE = 0,
    INCOMING_MSG_NOTIFY,
    INCOMING_MSG_INFO,
    INCOMING_MSG_BYE
  };

  msg = eXosipEvent->request;

  if (!msg) {
    return -1;
  }

  // determine msg type 

  if (MSG_IS_UPDATE (msg))
    msg_type = INCOMING_MSG_UPDATE;
  else if (MSG_IS_NOTIFY (msg))
    msg_type = INCOMING_MSG_NOTIFY;
  else if (MSG_IS_INFO (msg))
    msg_type = INCOMING_MSG_INFO;
  else if (MSG_IS_BYE (msg))
    msg_type = INCOMING_MSG_BYE;

  // determine content type if any 
  // find first match from table above 

  if (msg->content_type) {
    osip_content_type_to_str (msg->content_type, &content_type);
    if (content_type) {
      for (i = 1; contents[i]; i++) {
        if (!strncasecmp (contents[i], content_type, (strlen(contents[i]) - 1))) {
          con_type = i;
          break;
        }
      }
      free (content_type);
    }
  }

  // this assumes DTMF for now

  if ((msg_type == INCOMING_MSG_INFO)) {
    rc = incomingDTMFHandler (eXosipEvent, zCall);
  }
  else if ((msg_type == INCOMING_MSG_UPDATE)) {
    rc = incomingUpdateHandler (eXosipEvent, zCall);
  } else if ((msg_type == INCOMING_MSG_NOTIFY)){
    rc = incomingNotifyHandler(eXosipEvent, zCall);
  }

  return rc;
}


int 
incomingNotifyHandler(eXosip_event_t *eXosipEvent, int zCall){

  int rc = -1;
  osip_message_t *resp = NULL;

  int status = 200;

  eXosip_lock (geXcontext);
  rc = eXosip_call_build_answer (geXcontext, eXosipEvent->tid, status, &resp);
  rc = eXosip_call_send_answer (geXcontext, eXosipEvent->tid, status, resp);
  eXosip_unlock (geXcontext);

  return rc;
}



int
incomingDTMFHandler (eXosip_event_t * eXosipEvent, int zcall)
{

  int rc = 0;
  int yResponse = 488;
  int dtmfkey = 0;
  char *dtmfkeyptr = NULL;
  int duration = 0;
  char *ptr;

  osip_message_t *infoAnswer = NULL;

  if (eXosipEvent->request) {

    osip_content_type_t *content_typ = NULL;
    osip_content_length *content_len = NULL;
    osip_body_t *body = NULL;

    content_typ = osip_message_get_content_type (eXosipEvent->request);
    content_len = osip_message_get_content_length (eXosipEvent->request);

    if (content_typ && content_len && content_typ->subtype
        && content_len->value) {
      if (!strcmp (content_typ->subtype, "dtmf-relay")) {

        yResponse = 200;

        osip_message_get_body (eXosipEvent->request, 0, &body);

        if (body) {
          ptr = strstr (body->body, "Signal=");
          if (ptr) {
            ptr += strlen ("Signal=");
            dtmfkey = *ptr;
            dtmfkeyptr = ptr;
          }
          ptr = strstr (body->body, "Duration=");
          if (ptr) {
            ptr += strlen ("Duration=");
            duration = atoi (ptr);
          }

          struct MsgToDM msgDtmfDetails;

          if ((zcall >= 0) && duration) {

            struct timeval tb;

            //ftime (&tb);
			gettimeofday(&tb, NULL);
            if (isItFreshDtmf (zcall, tb) && (arc_dtmf_char_to_int (dtmfkeyptr) >= 0)) {
              memcpy (&msgDtmfDetails, &(gCall[zcall].msgToDM),
                      sizeof (struct MsgToDM));
              msgDtmfDetails.opcode = DMOP_SETDTMF;
              msgDtmfDetails.appPid = gCall[zcall].appPid;
              msgDtmfDetails.appPassword = gCall[zcall].appPassword;
              sprintf (msgDtmfDetails.data, "%d", arc_dtmf_char_to_int (dtmfkeyptr));
              gCall[zcall].lastDtmfTime = tb;
              notifyMediaMgr (__LINE__, zcall, (struct MsgToDM *) &(msgDtmfDetails),
                              (char *) __func__);
            }
          }
        }
      }
    }
  }

  eXosip_lock (geXcontext);
  rc = eXosip_call_build_answer (geXcontext, eXosipEvent->tid, yResponse, &infoAnswer);
  rc = eXosip_call_send_answer (geXcontext, eXosipEvent->tid, yResponse, infoAnswer);
  eXosip_unlock (geXcontext);

  if (rc < 0) {
    dynVarLog (__LINE__, -1, (char *) __func__, REPORT_NORMAL, DYN_BASE, ERR,
               "Failed to send %d to INFO message (tid:%d).", yResponse,
               eXosipEvent->tid);
  }
  else {
    dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE, DYN_BASE, 2,
               "Sent %d to SIP INFO DTMF message (tid:%d)(dtmfval=%d).", yResponse,
               eXosipEvent->tid, arc_dtmf_char_to_int (dtmfkeyptr));
  }

  return 0;
}


int 
incomingCallWithSipSignaledDigits(osip_message_t *msg, int zCall){

   int rc;
   char event[256] = "";
   char touri[256] = "";
   char fromuri[256] = "";

   osip_uri_t *uri = NULL;

   if(!msg)
      return -1;

   // sorry but we have to drp the tags from the passed in 
   // invite, maybe there is a better way to do this, maybe not 

   if(msg->to){
      uri = osip_to_get_url(msg->to);
      if(uri){
        if(uri->scheme &&  uri->username && uri->host && uri->port){
          snprintf(touri, sizeof(touri), "%s:%s@%s:%s", uri->scheme, uri->username, uri->host, uri->port);
        } else 
        if(uri->scheme && uri->username && uri->host){
          snprintf(touri, sizeof(touri), "%s:%s@%s", uri->scheme, uri->username, uri->host);
        } else 
        if(uri->scheme && uri->host && uri->port){
          snprintf(touri, sizeof(touri), "%s:%s:%s", uri->scheme, uri->host, uri->port);
        } else 
        if(uri->scheme && uri->host){
          snprintf(touri, sizeof(touri), "%s:%s", uri->scheme, uri->host);
        }
      }
   }

   if(msg->from){
      uri = osip_from_get_url(msg->from);
      if(uri){
        if(uri->scheme &&  uri->username && uri->host && uri->port){
          snprintf(fromuri, sizeof(fromuri), "%s:%s@%s:%s", uri->scheme, uri->username, uri->host, uri->port);
        } else
        if(uri->scheme && uri->username && uri->host){
          snprintf(fromuri, sizeof(fromuri), "%s:%s@%s", uri->scheme, uri->username, uri->host);
        } else
        if(uri->scheme && uri->host && uri->port){
          snprintf(fromuri, sizeof(fromuri), "%s:%s:%s", uri->scheme, uri->host, uri->port);
        } else
        if(uri->scheme && uri->host){
          snprintf(fromuri, sizeof(fromuri), "%s:%s", uri->scheme, uri->host);
        }
      }
   }



   snprintf(event, sizeof(event), "telephone-event;duration=%d", gSipSignaledDigitsMinDuration);
   dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, DYN_BASE, INFO, "Creating Outbound Subscription for SIP Signaled Digits, event=%s touri=%s fromuri=%s\n", "telephone-event", touri, fromuri);

   // note that the two and from are reversed in this case
   rc = newSubscriptionHandler(fromuri, touri,  NULL, "telephone-event", gSipSignaledDigitsEventExpires, zCall);

   return rc;
}

int 
dropSipSignaledDigitsSubscription(int zCall){

  int rc = 0;

  dynVarLog (__LINE__, zCall, (char *) __func__, REPORT_VERBOSE, DYN_BASE, INFO,
               "Dropping Outbound Subscription for SIP Signaled Digits zcall=%d\n", zCall);
  if(zCall >= 0 && zCall <= MAX_PORTS){
    SubscriptionInfoDeleteByzCall(SubscriptionInfoList, zCall);
  }

  return rc;
}

