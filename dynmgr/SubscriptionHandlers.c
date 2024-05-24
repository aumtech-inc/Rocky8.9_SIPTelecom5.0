#include <stdlib.h>
#include <eXosip2/eXosip.h>

#define  IMPORT_GLOBAL_EXTERNS 
#include "ArcSipCommon.h"
#include "RegistrationInfo.h"
#include "SubscriptionInfo.h"
#include "dynVarLog.h"


/* 
Some macros for the extraction of rfc 2833 telephony-event data from RTP payloads
... Joe-S
*/

/* 
   The structure of the audio/telephone-event MIME type is reproduced 
   here from RFC2833 for the convenience of the reader. 
    
   0                   1                   2                   3  
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1  
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
   |     event     |E|R|  volume   |         duration              |  
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
*/

#define RFC_2833_PAYLOAD_DURATION_32BIT(word32) (word32 & 0x0000ffff)
#define RFC_2833_PAYLOAD_EVENT_32BIT(word32) ((word32 & 0xff000000) >> 24)
#define RFC_2833_PAYLOAD_VOLUME_32BIT(word32) ((word32 & 0x003f0000) >> 16)
#define RFC_2833_PAYLOAD_END_32BIT(word32) ((word32 & 0x00800000) >> 23)
#define RFC_2833_PAYLOAD_RESERVED_32BIT(word32) ((word32 & 0x00400000) >> 22)

extern struct eXosip_t eXosip;
extern SubscriptionInfoList_t SubscriptionInfoList;


struct subscription_cb_t {
   char *name;
   int (*cb)();
};


int 
test_cb(void *msg){
   // fprintf(stderr, "******************************\n");
   return 0;
}

//
// SIP signaled digits is basically another form form of SIP 
// INFO dtmf, but it uses the telephony-event encoding from 
// rfc 2833 as the attachment 
//
// all subscription callbacks must return the status code for the 
// outgoing SIP reply
// 


int SipSignaledDigitsCallback(void *mesg, int zCall){

  int rc = 200;
  osip_message_t *msg = (osip_message_t *)mesg;
  osip_body_t *body = NULL;

  unsigned int buff;
  unsigned int event;
  unsigned int duration;
  unsigned int volume;

  if(msg){
    osip_message_get_body(msg, 0, &body);
    if(body && body->body){
      memcpy(&buff, body->body, sizeof(buff));
      event = RFC_2833_PAYLOAD_EVENT_32BIT(buff);
      duration = RFC_2833_PAYLOAD_DURATION_32BIT(buff);
      volume = RFC_2833_PAYLOAD_VOLUME_32BIT(buff);
      //fprintf(stderr, " %s:buff=%Xevent=%dduration=%dvolume=%d\n", __func__, buff, event, duration, volume);

      if(duration >= gSipSignaledDigitsMinDuration){
        // send as DTMF to media manager
        struct timeval tb;
        struct MsgToDM msgDtmfDetails;
       // ftime (&tb);
		gettimeofday(&tb, NULL);

        if (isItFreshDtmf (zCall, tb) && (event >= 0)) {
           memcpy (&msgDtmfDetails, &(gCall[zCall].msgToDM),
                 sizeof (struct MsgToDM));
           msgDtmfDetails.opcode = DMOP_SETDTMF;
           msgDtmfDetails.appPid = gCall[zCall].appPid;
           msgDtmfDetails.appPassword = gCall[zCall].appPassword;
           sprintf (msgDtmfDetails.data, "%d", event);
           gCall[zCall].lastDtmfTime = tb;
           notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msgDtmfDetails), (char *) __func__);
         }
      }
    }
  }
  
  return rc;

}



#define UGLY_CAST (int (*)())

struct subscription_cb_t subscription_cbs[] = {
    {"message-summary", UGLY_CAST test_cb}, 
    {"telephone-event", UGLY_CAST SipSignaledDigitsCallback}, 
    {NULL, NULL}
};


int
newSubscriptionHandler (char *touri, char *fromuri, char *route, char *event, int expires, int zCall)
{

 int rc = -1;
 int i;
 int (*cb) () = NULL;
 osip_message_t *sub = NULL;

 eXosip_lock(geXcontext);
 eXosip_subscribe_build_initial_request (geXcontext, &sub, touri, fromuri, route, event, expires);
 eXosip_unlock(geXcontext);

 // fprintf (stderr, "%s: i = %d\n", __func__, i);
 // lookup callbacks in table based on event name

 for (i = 0; subscription_cbs[i].name; i++) {
  if (!strcmp (subscription_cbs[i].name, event)) {
   // fprintf(stderr, " %s() found callback for event=<%s>\n", __func__, event);
   cb = UGLY_CAST subscription_cbs[i].cb;
   break;
  }
 }

 if (sub) {
  // fprintf (stderr, " %s: new subscription call-id-number = %s event=%s\n", __func__, sub->call_id->number, event);
  SubscriptionInfoAdd(&SubscriptionInfoList, touri, fromuri, route, event, sub->call_id->number, NULL, cb, zCall);

  eXosip_lock(geXcontext);
  rc = eXosip_subscribe_send_initial_request (geXcontext, sub);
  eXosip_unlock(geXcontext);

 }

 return rc;
}


int
successfulSubscriptionHandler (eXosip_event_t * eXosipEvent)
{

 int rc = -1;
 osip_message_t *msg = NULL;
 osip_header_t *expires = NULL;
 int status;
 int exp;
 int i;
 char *callid = NULL;
 SubscriptionInfo_t *subinfo = NULL;

 // 200 level handler for subscriptions 
 // 202 is a non-commital response to subsriptions i.e. may not mean you'll
 // get a notify 
 // we want to update the subscription list with a timestamp and schedule 
 // a refresh taken from the expires tag. 

 if (eXosipEvent->response) {

  callid = eXosipEvent->response->call_id->number;
  int did = eXosipEvent->did;

  status = osip_message_get_status_code (eXosipEvent->response);

  if (status == 200 || status == 202) {

   // i'm lucky that this only takes place once every few hundred seconds 
   // and that this list is short enough

   subinfo = SubscriptionInfoFindByCallId (&SubscriptionInfoList, callid);

   if (callid && subinfo) {

    SubscriptionInfoUpdateLastSubscription (&SubscriptionInfoList, callid);
    SubscriptionInfoUpdateDid (&SubscriptionInfoList, callid, did);
    osip_message_get_expires (eXosipEvent->response, 0, &expires);

    if (expires && expires->hvalue) {
     exp = atoi (expires->hvalue);
     //fprintf (stderr, "%s: new expires = %d\n", __func__, exp);
     if(exp > 0){
       SubscriptionInfoUpdateExpiresByCallId (&SubscriptionInfoList, callid, exp);
     }
    }
    SubscriptionInfoFree (subinfo);
   }
   else {
    // fprintf (stderr, " %s: could not find callid %s in subscription info\n", __func__, callid);
   }
  }
  //fprintf (stderr, "%s: status code = %d\n", __func__, status);
 }

 return rc;
}

int
unsuccessfulSubscriptionHandler (eXosip_event_t * eXosipEvent)
{

 int rc = 0;
 osip_message_t *sub = NULL;
 int status;
 char *callid = NULL;

 // 400 level subscription handler 
 // 401 add auth 
 // 404 invalid subscription 
 // 481 call leg does no exist 

 if (eXosipEvent->response) {

  callid = eXosipEvent->response->call_id->number;
  status = osip_message_get_status_code (eXosipEvent->response);

  // could switch this out for a case statement
  // ok i did

  switch(status){
   case 401:
    eXosip_lock (geXcontext);
    eXosip_automatic_action (geXcontext);
    eXosip_unlock (geXcontext);
    break;

   case 404:
    if (callid) {
     SubscriptionInfoDeleteByCallId (&SubscriptionInfoList, callid);
    }
    break;
   case 405:
    if(callid){
     SubscriptionInfoDeleteByCallId (&SubscriptionInfoList, callid);
    }
    break;
  
   case 481:
      SubscriptionInfo_t *subinfo;
      // retry 
      subinfo = SubscriptionInfoFindByCallId(&SubscriptionInfoList, callid);
      if(subinfo){
        newSubscriptionHandler(subinfo->touri, subinfo->fromuri, subinfo->route, subinfo->event, subinfo->expires, subinfo->zCall);
        SubscriptionInfoDeleteByCallId (&SubscriptionInfoList, callid);
        SubscriptionInfoFree(subinfo);
      }
     break;

   // i guess 500 and 600 level messages will signal a major problem 
   // probably best to just remove them altogether 

   default:
//    fprintf(stderr, " %s Unhandled status code please fix status code=%d\n", __func__, status);
    rc = -1;
    break;
  }
 }

 return rc;
}


int 
deletedSubscriptionHandler(eXosip_event_t *eXosipEvent){

   char *callid = NULL;

   if(!eXosipEvent)
       return -1;

   if (eXosipEvent->request)
     callid = eXosipEvent->request->call_id->number;

   if(callid)
     SubscriptionInfoDeleteByCallId (&SubscriptionInfoList, callid);

    return 0;
}


int
incomingNotifyHandler (eXosip_event_t * eXosipEvent)
{

 int rc = -1;
 char *callid = NULL;
 int status = 200;
 osip_message_t *answer = NULL;
 SubscriptionInfo_t *subinfo = NULL;

 //fprintf (stderr, " %s: incoming notify detected\n", __func__);

 if (eXosipEvent->request) {

  callid = eXosipEvent->request->call_id->number;

  if (callid) {

   subinfo = SubscriptionInfoFindByCallId (&SubscriptionInfoList, callid);

   // Needs a clean up at some point better to use a case statement here 

   if (subinfo) {
     if(subinfo->cb == NULL){
//        fprintf(stderr, " %s: ERROR no Subscription Callback Provided\n", __func__);
        rc = 488;
     }
     else 
     if(subinfo->cb == UGLY_CAST test_cb){
       test_cb(NULL);
       rc = 200;
     }
     else
     if(subinfo->cb == UGLY_CAST SipSignaledDigitsCallback){
       rc = SipSignaledDigitsCallback(NULL, -1);
     } 
     else {
//        fprintf(stderr, " %s: ERROR No Subscription Callback Provided\n", __func__);
        rc = 488;
     } 
    }
    SubscriptionInfoFree (subinfo);
   }
  }


 if (status) {
  eXosip_lock(geXcontext);
  eXosip_message_build_answer (geXcontext, eXosipEvent->tid, status, &answer);
  if (answer) {
   rc = eXosip_message_send_answer (geXcontext, eXosipEvent->tid, status, answer);
  }
  eXosip_unlock(geXcontext);
 }

 return rc;
}

int
updateSubscriptionHandler (char *callid)
{

 int rc = -1;
 int i;

 eXosip_lock (geXcontext);
 eXosip_automatic_action (geXcontext);
 eXosip_unlock (geXcontext);

 return rc;
}



int
incomingSubscriptionHandler (eXosip_event_t * eXosipEvent)
{

 int rc = 0;
 int Response = 488;

 osip_message_t *infoAnswer = NULL;

 eXosip_lock(geXcontext);
 eXosip_call_build_answer (geXcontext, eXosipEvent->tid, Response, &infoAnswer);
 if(infoAnswer){
   rc = eXosip_call_send_answer (geXcontext, eXosipEvent->tid, Response, infoAnswer);
 }
 eXosip_unlock(geXcontext);

 if (rc < 0) {
  dynVarLog (__LINE__, -1, (char *) __func__, REPORT_NORMAL, DYN_BASE, ERR, "Failed to send %d to SUBSCRIBE message (tid:%d).", Response, eXosipEvent->tid);
 }
 else {
  dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE, DYN_BASE, INFO, "Sent %d to SUBSCRIBE message (tid:%d).", Response, eXosipEvent->tid);
 }

 return rc;
}
