#include <stdio.h>
#include <stdlib.h>
#include <eXosip2/eXosip.h>

#include "ArcSipCommon.h"
#include "dynVarLog.h"

extern struct eXosip_t *geXcontext;

#define __MOD__ ((char *)__func__)

//
// plan as of this time is 
// send the application an error 
// and a new uri so it wants to follow the redirection 
// it has the contact str
//

extern Call gCall[MAX_PORTS];
extern char gContactIp[48];
extern  int gSipPort;

extern int arc_fixup_contact_header (osip_message_t * msg, char *ip, int port, int zCall);
extern int arc_insert_global_headers (int zCall, osip_message_t * msg);
extern int setCallState (int zCall, int zState);


/*
  The goal is to handle this 
  here as much as possible and to update 
  the gCall stuff only when the handshake is done 
*/

enum SipRefer_e {
  INBOUND_SIP_REFER_IDLE,
  INBOUND_SIP_REFER_FOLLOW,
  INBOUND_SIP_REFER_REFER_RECEIVED, 
  INBOUND_SIP_REFER_202_SENT, 
  INBOUND_SIP_REFER_NOTIFY_SIP_FRAG_PROCEEDING_SENT, 
  INBOUND_SIP_REFER_NOTIFY_PROCEEDING_200_SENT, 
  INBOUND_SIP_REFER_FAILURE_MESSAGE,
  // 
  OUTBOUND_SIP_REFER_IDLE,
  OUTBOUND_SIP_REFER_START,
  OUTBOUND_SIP_REFER_REFER_SENT, 
  OUTBOUND_SIP_REFER_202_RECEIVED, 
  OUTBOUND_SIP_REFER_NOTIFY_SIP_FRAG_PROCEEDING_RECEIVED, 
  OUTBOUND_SIP_REFER_NOTIFY_SIP_FRAG_200_OK_RECIEVED,
  OUTBOUND_SIP_REFER_FAILURE_MESSAGE,
  OUTBOUND_SIP_REFER_SUCCESS_MESSAGE
};

struct SipRefer_t {
  enum SipRefer_e state;
  int cid;
  int did; 
  char dest_uri[256];
  char from_uri[256];
  char event_id[100];
  int reason_code;
};

static struct SipRefer_t sip_refer_state[MAX_PORTS];
static struct SipRefer_t sip_inrefer_state[MAX_PORTS];

enum InboundReferOps_t {
  INBOUND_REFER_OP_FOLLOW = (1 << 1),
  INBOUND_REFER_OP_SET_IDLE = (1 << 2)
};

int parseURI(char *zFrom, char *zUser, char *zIP, int *zPort)    // MR 5093
{
	char	buf[256];
	char	*ptr;
	
	*zUser = '\0';
	*zIP = '\0';
	*zPort = -1;

	sprintf(buf, "%s", zFrom);

	if ( (ptr = strstr(buf, "sip:")) )
	{
		ptr+=4;
		sprintf(buf, "%s", ptr);
	}

	if ( (ptr = strstr(buf, "<")) )
	{
		ptr+=1;
		sprintf(buf, "%s", ptr);
	}

	if ( (ptr = strstr(buf, ">")) )
	{
		*ptr = '\0';
	}

	if ( (ptr = strstr(buf, "@")) )
	{
		*ptr = '\0';
		sprintf(zUser, "%s", buf);
		ptr++;
		sprintf(buf, "%s", ptr);
	}
	else
	{
		sprintf(zUser, "%s", buf);
		return(0);
	}

	if ( (ptr = strstr(buf, "[")) )
	{
		ptr+=1;
		sprintf(buf, "%s", ptr);

		if ( (ptr = strstr(buf, "]")) )
		{
			*ptr = '\0';
			sprintf(zIP, "%s", buf);
			ptr++;
			sprintf(buf, "%s", ptr);
		}
	}

	if ( (ptr = strstr(buf, ":")) )
	{
		*ptr = '\0';
		if ( *zIP == '\0' )
		{
			sprintf(zIP, "%s", buf);
		}
		ptr++;
		*zPort = atoi(ptr);
	}
	else if ( *zIP == '\0' )
	{
		sprintf(zIP, "%s", buf);
	}
	return(0);

}

int arc_add_callID (osip_message_t *msg, char *zFrom, int zCall)    // MR 5093
{
	int				rc;

	char		fromUser[128];
	char		fromUrl[128];
	int			fromPort;

	if ( ! zFrom )
	{
		dynVarLog(__LINE__, zCall, __MOD__, REPORT_DETAIL, DYN_BASE, WARN, 
			"\"from\" string is empty. Contact to REFER will not be changed.");
		return(-1);
	}

	rc = parseURI(zFrom, fromUser, fromUrl, &fromPort);

	if (msg)
	{
		char *display = NULL;
		
		int	rc;

		display = osip_from_get_displayname (msg->from);

		if ( fromUser )
		{
			if ( msg->from->displayname	)
			{
				osip_free(msg->from->displayname);
			}
			msg->from->displayname = NULL;
			msg->from->displayname = strdup(fromUser);

			dynVarLog(__LINE__, zCall, __MOD__, REPORT_VERBOSE, DYN_BASE, INFO,
					"[%d] Set displayname/callerID to (%s).", __LINE__, fromUser);
		}
	}

	return 0;

}/*END: arc_add_callID */

int 
inboundCallReferHandler(eXosip_event_t *event, int zCall, int op=0){

  int rc = -1;
  osip_message_t *msg = NULL;
  osip_message_t *resp = NULL;
  osip_uri_t *refer_to = NULL;

  /* test for zCall happiness */

  fprintf(stderr, " entering %s(event=%p, zCall=%d, op=%d)\n", __func__, event, zCall, op); 

  if(zCall < 0 || zCall >= MAX_PORTS){
    fprintf(stderr, " %s() Zcall index did not pass entry condition zCall=%d\n", __func__, zCall); 
    return -1;
  }

  /* do ops */ 

  if(op & INBOUND_REFER_OP_FOLLOW){
     sip_inrefer_state[zCall].state =  INBOUND_SIP_REFER_FOLLOW;
	 fprintf(stderr, " %s() set to follow inbound refer \n", __func__);
  }

  if(op & INBOUND_REFER_OP_SET_IDLE){
     sip_inrefer_state[zCall].state = INBOUND_SIP_REFER_IDLE;
  }

  /* follow state */

  if(sip_inrefer_state[zCall].state == INBOUND_SIP_REFER_IDLE){
     fprintf(stderr, " %s() not set to follow the refer returning... \n", __func__);
     // fire a message ? 
     return 0;
  } 

  if(sip_inrefer_state[zCall].state == INBOUND_SIP_REFER_FOLLOW){
    if(event && event->request){ // inbound refer here.. 
      if(MSG_IS_REFER(event->request)){

      }
    }
  }

  fprintf(stderr, " %s() rc=%d\n", __func__, rc);

  return rc;
}

enum OutboundReferOps_t {
  OUTBOUND_REFER_OP_INIT = (1 << 1),
  OUTBOUND_REFER_OP_SET_IDLE = (1 << 2)
};


int 
OutboundCallReferHandler(int zLine, eXosip_event_t *event, int zCall, char *new_uri=NULL, char *from=NULL, int op=0, char *referTo=NULL){

  int rc = -1;
  osip_message_t *ref = NULL;
  osip_message_t *resp = NULL;
  osip_message_t *msg = NULL;
  osip_header_t  *event_hdr = NULL;
  osip_body_t *body = NULL;
  int message_status = 0;
  char *str = NULL;
  char *ptr = NULL;
  size_t len;

  /* test for zCall happiness */

	dynVarLog(__LINE__, zCall, __MOD__, REPORT_VERBOSE, DYN_BASE, INFO,
			"[%d] entering %s(event=%p, zCall=%d, new_uri=%p, from=%p op=%d)",
			zLine, __func__, event, zCall, new_uri, from, op); 

  if(zCall < 0 || zCall >= MAX_PORTS){
    dynVarLog (__LINE__, zCall, __MOD__, REPORT_NORMAL, DYN_BASE, ERR,
         "[%d] Call index did not pass entry conditions! zCall=%d.", zLine, zCall);
    return -1;
  }

  /* perform operations that cause actions */

  if(op & OUTBOUND_REFER_OP_SET_IDLE){
    dynVarLog (__LINE__, zCall, __MOD__, REPORT_VERBOSE, DYN_BASE, INFO,
         "[%d] Setting SIP Refer state to idle, zCall=%d.", zLine, zCall);
    sip_refer_state[zCall].state = OUTBOUND_SIP_REFER_IDLE;
	return 0;
  }

  if(op & OUTBOUND_REFER_OP_INIT){
    memset(&sip_refer_state[zCall], 0, sizeof(SipRefer_t));
    if(new_uri == NULL){
    dynVarLog (__LINE__, zCall, __MOD__, REPORT_VERBOSE, DYN_BASE, INFO,
         "[%d] SIP REFER: No new URI passed in, you need to set a new detination, zCall=%d.", zLine, zCall);
      return -1; 
    }
	snprintf(sip_refer_state[zCall].dest_uri, sizeof(sip_refer_state[zCall].dest_uri), "%s", new_uri);
    if(from){
      snprintf(sip_refer_state[zCall].from_uri, sizeof(sip_refer_state[zCall].from_uri), "%s", from);
    }
    sip_refer_state[zCall].state = OUTBOUND_SIP_REFER_START;
  }

  /* handy debug */

#if 0

dynVarLog(__LINE__, zCall, __MOD__, REPORT_VERBOSE, DYN_BASE, INFO, "[%d] current sip refer state=%d to=%s from=%s\n", 
         zLine, sip_refer_state[zCall].state, 
         sip_refer_state[zCall].dest_uri, 
         sip_refer_state[zCall].from_uri);

#endif 

  /*
    Beginning of the operations
  */

  if(sip_refer_state[zCall].state == OUTBOUND_SIP_REFER_IDLE){
    return 0;
  }

  /* this we get from gCall to start off */

  if(sip_refer_state[zCall].state == OUTBOUND_SIP_REFER_START){

    eXosip_call_build_refer(geXcontext, gCall[zCall].did, sip_refer_state[zCall].dest_uri, &ref);
    if(ref != NULL){
       if(from != NULL){
// MR 4970
          if ( referTo ) 
          {
            osip_header_t *refer_to_hdr = NULL;
            char        tmpReferTo[128];
            rc = osip_message_header_get_byname(ref, "Refer-to", 0, &refer_to_hdr);

            if ( strstr(referTo, "<sip:") == NULL )
            {
                sprintf(tmpReferTo, "<sip:%s>", referTo);
                sprintf(referTo, "%s", tmpReferTo);
            }
            dynVarLog (__LINE__, zCall, __MOD__, REPORT_VERBOSE, DYN_BASE, INFO,
                    "[%d] Resetting Refer-to header to (%s).", zLine, referTo);

            osip_free(refer_to_hdr->hvalue);

            refer_to_hdr->hvalue = strdup(referTo);

          }
// END - MR 4970
       }
	   arc_insert_global_headers (zCall, ref);
		if(from != NULL)
		{
			arc_add_callID (ref, from, zCall);		// MR 5093
		}

#if 0
       osip_message_to_str(ref, &str, &len);
       if(str){
fprintf(stderr, " %s() message=\n[%s]", __func__, str);
		  osip_free(str);
       }
#endif 
	   eXosip_lock(geXcontext);
       rc = eXosip_call_send_request(geXcontext, gCall[zCall].did, ref);
	   eXosip_unlock(geXcontext);
       if(rc == 0){
         sip_refer_state[zCall].state = OUTBOUND_SIP_REFER_REFER_SENT;
         sip_refer_state[zCall].did = gCall[zCall].did;
         setCallState (zCall, CALL_STATE_CALL_TRANSFER_ISSUED);
         dynVarLog (__LINE__, zCall, __MOD__, REPORT_VERBOSE, DYN_BASE, INFO,
         "[%d] Initial SIP REFER sent, destination URI=[%s] rc=%d zCall=%d.", 
		 zLine, sip_refer_state[zCall].from_uri, rc, zCall);
       } else {      
		  dynVarLog (__LINE__, zCall, __MOD__, REPORT_NORMAL, DYN_BASE, ERR,
         "[%d] Failed to send Initial SIP REFER, destination URI=[%s] rc=%d zCall=%d.", 
         zLine, sip_refer_state[zCall].from_uri, rc, zCall);
       }
    }
    // returns the value from eXosip_send 
    
    return rc;
  }

  if(sip_refer_state[zCall].state == OUTBOUND_SIP_REFER_REFER_SENT){
    if(event && event->response){
      // check for response to REFER 
	  if(MSG_IS_RESPONSE_FOR(event->response, "REFER")){
        message_status = osip_message_get_status_code(event->response);
        // message_status = event->response->status_code;
      }
      switch(message_status){
          case 0:
            // unset 
			break;
          case 202:
			// accepted 
            sip_refer_state[zCall].state = OUTBOUND_SIP_REFER_202_RECEIVED;
		    sip_refer_state[zCall].reason_code = message_status;
			setCallState (zCall, CALL_STATE_CALL_TRANSFER_MESSAGE_ACCEPTED);
            dynVarLog (__LINE__, zCall, __MOD__, REPORT_VERBOSE, DYN_BASE, INFO,
            "[%d] Received 202 To SIP REFER, destination URI=[%s] rc=%d zCall=%d.", 
            zLine, sip_refer_state[zCall].from_uri, rc, zCall);

			break;
		  case 415:
            sip_refer_state[zCall].state = OUTBOUND_SIP_REFER_FAILURE_MESSAGE;
		    sip_refer_state[zCall].reason_code = message_status;
			setCallState (zCall, CALL_STATE_CALL_TRANSFERFAILURE);
			dynVarLog (__LINE__, zCall, __MOD__, REPORT_VERBOSE, DYN_BASE, INFO,
            "[%d] Received 415 To SIP REFER, destination URI=[%s] rc=%d zCall=%d.", 
            zLine, sip_refer_state[zCall].from_uri, rc, zCall);

			break;
		  default:
            sip_refer_state[zCall].state = OUTBOUND_SIP_REFER_FAILURE_MESSAGE;
		    sip_refer_state[zCall].reason_code = message_status;
			setCallState (zCall, CALL_STATE_CALL_TRANSFERFAILURE);
            dynVarLog (__LINE__, zCall, __MOD__, REPORT_VERBOSE, DYN_BASE, INFO,
            "[%d] Received %d To SIP REFER, destination URI=[%s] rc=%d zCall=%d.", 
            zLine, message_status, sip_refer_state[zCall].from_uri, rc, zCall);

			break;
      }
    }
  }

  if(
	 sip_refer_state[zCall].state == OUTBOUND_SIP_REFER_202_RECEIVED || 
     sip_refer_state[zCall].state == OUTBOUND_SIP_REFER_NOTIFY_SIP_FRAG_PROCEEDING_RECEIVED ||
     sip_refer_state[zCall].state == OUTBOUND_SIP_REFER_NOTIFY_SIP_FRAG_200_OK_RECIEVED 
    ){

    // these are all notify's with sip frags -- check them 
    // once they go 200 then the sequence is done 
    // fire a 200 for each 

    if(event->request != NULL){
      msg = event->request;
      if(MSG_IS_NOTIFY(msg)){
         // check notify if it is sip frag and the right sip event 
         osip_message_get_body(msg, 0, &body);
		 osip_message_header_get_byname(msg, "Event", 0, &event_hdr);
         if(body->body){
	        char *ptr = NULL;
	        ptr = strstr(body->body, "SIP/2.0 ");
            if(ptr){
			   ptr += strlen("SIP/2.0 ");
			   message_status = atoi(ptr);
               switch(message_status){
                   case 100:
				   case 101:
				   case 180:
				   case 183:
				   case 300:
				   case 301:
				   case 302:
            			dynVarLog (__LINE__, zCall, __MOD__, REPORT_DETAIL, DYN_BASE, INFO,
				            "[%d] Received NOTIFY To SIP REFER, SIP Fragment=[%s]", zLine, body->body);
						sip_refer_state[zCall].state = OUTBOUND_SIP_REFER_NOTIFY_SIP_FRAG_PROCEEDING_RECEIVED;
		                sip_refer_state[zCall].reason_code = message_status;
			            setCallState (zCall, CALL_STATE_CALL_TRANSFER_RINGING);
						break;
				   case 200:
            			dynVarLog (__LINE__, zCall, __MOD__, REPORT_DETAIL, DYN_BASE, INFO,
				            "[%d] Received NOTIFY To SIP REFER, SIP Fragment=[%s]", zLine, body->body);
						sip_refer_state[zCall].state = OUTBOUND_SIP_REFER_NOTIFY_SIP_FRAG_200_OK_RECIEVED;
						sip_refer_state[zCall].state = OUTBOUND_SIP_REFER_SUCCESS_MESSAGE;
		                sip_refer_state[zCall].reason_code = message_status;

						// BT-171
						gCall[zCall].canExitTransferCallThread = 1;
			            setCallState (zCall, CALL_STATE_CALL_TRANSFERCOMPLETE);
            			dynVarLog (__LINE__, zCall, __MOD__, REPORT_DETAIL, DYN_BASE, INFO,
				            "[%d] Set canExitTransferCallThread / CALL_STATE_CALL_TRANSFERCOMPLETE", zLine);
						break;
				   case 404:
				   case 486:
            			dynVarLog (__LINE__, zCall, __MOD__, REPORT_NORMAL, DYN_BASE, INFO,
				            "[%d] Received NOTIFY To SIP REFER, SIP Fragment=[%s]", zLine, body->body);
						sip_refer_state[zCall].state = OUTBOUND_SIP_REFER_FAILURE_MESSAGE;
						sip_refer_state[zCall].reason_code = message_status;
			            setCallState (zCall, CALL_STATE_CALL_TRANSFERFAILURE);
						// very specific ! not found 
						break;
				   case 603:
				   case 500: 
            			dynVarLog (__LINE__, zCall, __MOD__, REPORT_NORMAL, DYN_BASE, INFO,
				            "[%d] Received NOTIFY To SIP REFER, SIP Fragment=[%s]", zLine,  body->body);
						// fire back to the app a message that the bridge failed 
						sip_refer_state[zCall].state = OUTBOUND_SIP_REFER_FAILURE_MESSAGE;
		                sip_refer_state[zCall].reason_code = message_status;
			            setCallState (zCall, CALL_STATE_CALL_TRANSFERFAILURE);
						break;
				   default:
            			dynVarLog (__LINE__, zCall, __MOD__, REPORT_NORMAL, DYN_BASE, INFO,
				            "[%d] Received NOTIFY To SIP REFER, SIP Fragment=[%s]", zLine, body->body);
						sip_refer_state[zCall].state = OUTBOUND_SIP_REFER_FAILURE_MESSAGE;
		                sip_refer_state[zCall].reason_code = message_status;
			            setCallState (zCall, CALL_STATE_CALL_TRANSFERFAILURE);
						break;
               }
               gCall[zCall].eXosipStatusCode = message_status;
           }
		   // build and fire a 200 -- to the NOTIFY

		   eXosip_call_build_answer(geXcontext, event->tid, 200, &resp);
		   if(resp){
			 if(event_hdr && event_hdr->hvalue){
			    osip_message_set_header(resp, "Event", event_hdr->hvalue);
             }
	         arc_insert_global_headers (zCall, resp);
	         eXosip_lock(geXcontext);
			 rc = eXosip_call_send_answer(geXcontext, event->tid, 200, resp);
	         eXosip_unlock(geXcontext);
             if(rc == 0){
                dynVarLog (__LINE__, zCall, __MOD__, REPORT_VERBOSE, DYN_BASE, INFO,
                "[%d] 200 OK To SIP NOTIFY sent, SIP Frag=[%s] rc=%d zCall=%d.", 
                zLine, body->body, rc, zCall);
             } else {
                dynVarLog (__LINE__, zCall, __MOD__, REPORT_VERBOSE, DYN_BASE, INFO,
                "[%d] Failed to Send 200 OK To SIP NOTIFY!, SIP Frag=[%s] rc=%d zCall=%d.", 
                zLine, body->body, rc, zCall);
             }
          }
        }
      }
      else if(MSG_IS_BYE(msg)){ // BT-183
		// BT-171
		gCall[zCall].canExitTransferCallThread = 1;
		setCallState (zCall, CALL_STATE_CALL_TRANSFERCOMPLETE);
		dynVarLog (__LINE__, zCall, __MOD__, REPORT_DETAIL, DYN_BASE, INFO,
			"[%d] Set canExitTransferCallThread / CALL_STATE_CALL_TRANSFERCOMPLETE", zLine);
      }
    }
  }

  if(sip_refer_state[zCall].state == OUTBOUND_SIP_REFER_FAILURE_MESSAGE){
     dynVarLog (__LINE__, zCall, __MOD__, REPORT_VERBOSE, DYN_BASE, INFO,
     "[%d] Call Transfer Failed!  rc=%d zCall=%d.", 
     zLine, rc, zCall);
	 // the app will get notified in the transfer call thread  
  }

  if(sip_refer_state[zCall].state == OUTBOUND_SIP_REFER_SUCCESS_MESSAGE){
     dynVarLog (__LINE__, zCall, __MOD__, REPORT_VERBOSE, DYN_BASE, INFO,
     "[%d] Call Sucessfully Transferred rc=%d zCall=%d", 
      zLine, rc, zCall);
	 // the app will get notified in the transfer call thread  
  }

  return rc;
}



int 
OutboundRedirectionHandler(eXosip_event_t *event, int zCall, char *zMsg){

  int rc = -1;
  int actual_response_code = 0;
  osip_message_t *resp = NULL;
  int i;

 
  osip_contact_t *cptr = NULL; 
  char *contacts[8];

  //fprintf(stderr, " %s() entering %s event=%p zCall=%d\n", __func__, __func__, event, zCall);

  if(!event || !event->response){
     sprintf(zMsg, " %s() empty event of reponse message from eXosip", __func__);
     return -1;
  }

  if(zCall < 0 || zCall >= MAX_PORTS){
      sprintf(zMsg, " %s() zCall index failed sanity check [%d]", __func__, zCall);
      return -1;
  }

  memset(contacts, 0, sizeof(contacts));

  if(event->response && event->response->status_code){
     actual_response_code = osip_message_get_status_code(event->response);
     gCall[zCall].RedirectStatusCode = actual_response_code;

     switch(actual_response_code){
       // simple redirects

#if 1
       case 302:

//printf("Calling eXosip_lock from OutboundCallHandlers %d\n", pthread_self());fflush(stdout);

          eXosip_lock(geXcontext);
          cptr = (osip_contact_t *)osip_list_get(&event->response->contacts, 0);

          if(cptr){
             osip_contact_to_str(cptr, &contacts[0]);
          }

          if(contacts[0]){
             sprintf(zMsg, " %s() redirecting call for contact |%s|", __func__, contacts[0]);
          }

          // gCall[zCall].callState = CALL_STATE_CALL_REDIRECTED;
          eXosip_automatic_action(geXcontext);
		  // eXosip_default_action(event);

          eXosip_unlock(geXcontext);
          break;
       // multiple redirects  
#endif  
#if 0
       case 302:
#endif
       case 301:
       case 300:

          eXosip_lock(geXcontext);
          i = 0;
          while((cptr = (osip_contact_t *)osip_list_get(&event->response->contacts, i)) != NULL){
            if(i == 8){
               //fprintf(stderr, " %s() ran out of slots for redirection\n", __func__);
               break;
            }
            osip_contact_to_str(cptr, &contacts[i]);
            if(contacts[i] != NULL){
              ;; // fprintf(stderr, " %s() adding contact |%s| to multiple redirection\n", __func__, contacts[i]);
            }
            i++;
          }
          // eXosip_call_terminate(event->cid, event->did);
          eXosip_unlock(geXcontext);
			
		  // fprintf(stderr, "Setting call state to CALL_STATE_CALL_REDIRECTED\n");

          gCall[zCall].callState = CALL_STATE_CALL_REDIRECTED;

          break;
       default:
          sprintf(zMsg, " %s() unhandled redirection status code", __func__);
          rc = -1;
          break;
     }
  }
  else {
	
			fprintf(stderr, "NULL event->response or status code\n");
  }

  // create string for global variable 

  int len = 0;
  int tmpLen = 0;
  char *ptr = &gCall[zCall].redirectedContacts[0];

  for(i = 0; i < 8; i++){

    if(contacts[i] && len < sizeof(gCall[zCall].redirectedContacts)){
      if(strlen(contacts[i]) < (sizeof(gCall[zCall].redirectedContacts) - len)){

       //fprintf(stderr, " %s() adding |%s| to global string len=%d\n", __func__, contacts[i], len);

#if 0
       len += snprintf(ptr, sizeof(gCall[zCall].redirectedContacts) - len, "%s|", contacts[i]);
       ptr += len;
#endif

       tmpLen = snprintf(ptr, sizeof(gCall[zCall].redirectedContacts) - len, "%s|", contacts[i]);
		len = len + tmpLen;
       ptr += tmpLen;
      } else {
         break;
      }
    } else {
       break;
    }
  }

  //fprintf(stderr, " %s() global string buff=%s\n", __func__, gCall[zCall].redirectedContacts);

  sprintf(zMsg, " %s() global string buff=%s", __func__, gCall[zCall].redirectedContacts);

  // must be executed at this point 
  
  for(i = 0; i < 8; i++){
     if(contacts[i] != NULL){
        osip_free(contacts[i]);
     }
  }

  return rc;
}

