#include <stdlib.h>
#include <eXosip2/eXosip.h>

#include "dynVarLog.h"
#include "ArcSipCommon.h"
#include "RegistrationInfo.h"
#include "SubscriptionInfo.h"

extern struct eXosip_t *geXcontext;

extern char gOutboundProxy[];
extern RegistrationInfoList_t RegistrationInfoList;


int 
deRegistrationHandler(RegistrationInfoList_t *RegistrationInfoList){

  int rc = -1;
  int i = 0;

  osip_message_t *msg = NULL;

  for(i = 0; i < 100; i++){

     eXosip_lock(geXcontext);

     eXosip_register_build_register(geXcontext, i, 0, &msg);

     if(msg){
       eXosip_register_send_register(geXcontext, i, msg);
       eXosip_register_remove(geXcontext, i);
     }

     eXosip_unlock(geXcontext);
  }

  return rc;
}



int
updateRegistrationHandler (int regid, int interval)
{

 int rc = 0;

   osip_message_t *reg = NULL;

  eXosip_lock (geXcontext);
  eXosip_register_build_register (geXcontext, regid, interval, &reg);
  rc = eXosip_register_send_register (geXcontext, regid, reg);
  eXosip_unlock (geXcontext);

  return rc;

}


#define MINIMUM_REGISTRATION_INTERVAL  30


int
newRegistrationHandler (char *fromUri, char *proxyUri, char *contactUri, char *supported, char *route, int interval)
{

 int rc = 0;
 int regid;
 osip_message_t *reg = NULL;
 int regInterval = 1800;
 char *msg;


 if (interval >= MINIMUM_REGISTRATION_INTERVAL) {
  regInterval = interval;
 }

 eXosip_lock(geXcontext);
 // regid = eXosip_register_build_initial_register (fromUri, proxyUri, contactUri, regInterval, &reg);
 regid = eXosip_register_build_initial_register (geXcontext, fromUri, proxyUri, NULL, regInterval, &reg);
 eXosip_unlock(geXcontext);

 if (regid < 0) {
  rc = regid;
  dynVarLog (__LINE__, -1, (char *) __func__, REPORT_NORMAL, DYN_BASE, ERR, "Failed to build SIP register. No proxy will be used.");
 }
 else {

  if (supported && supported[0]) {
   osip_message_set_supported (reg, supported);
  }

  if (route && route[0]) {
   osip_message_set_route (reg, osip_strdup (route));
  }

  eXosip_lock (geXcontext);
  rc = eXosip_register_send_register (geXcontext, regid, reg);
  eXosip_unlock (geXcontext);

  dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE, DYN_BASE, INFO, "Sent SIP REGISTER for %s.", fromUri);

  if (rc < 0) {
   dynVarLog (__LINE__, -1, (char *) __func__, REPORT_NORMAL, TEL_REGISTRATION_ERROR, ERR,
			"Failed to send register to proxy. No proxy will be used.");
  }
  else {
   rc = regid;
  }
 }
 // fprintf(stderr, " %s: regid = %d\n", __func__, rc);

 usleep (1000 * 500);
 return rc;
}


int
successfulRegistrationHandler (eXosip_event_t * eXosipEvent)
{

 int rc = 0;
 int exp = 0;
 osip_contact_t *contact = NULL;

 RegistrationInfo_t *reginfo = NULL;
 osip_uri_param_t *expires = NULL;

 // this is mostly here to update state after 
 // a successful registration, pluck out capabilities from the upstream 
 // proxy - also get p-header-associated uri info for a sucessful IMS registration 
 // first update the list of registrations with new timestamp 

 RegistrationInfoListUpdateLastRegister (&RegistrationInfoList, eXosipEvent->rid);

 // end update timestamp 

 if (eXosipEvent->response) {

  //fprintf (stderr, "%s response to successful registration \n", __func__);
  //fprintf (stderr, "%s regid = %d\n", __func__, eXosipEvent->rid);

  // get expires tag from 200 ok to register 
  // we always follow this if not set then just use the original expires 

  osip_message_get_contact (eXosipEvent->response, 0, &contact);

  if (contact) {
   osip_contact_param_get_byname (contact, "expires", &expires);
   //fprintf (stderr, "expires = %s\n", expires->gvalue);
   if (expires && expires->gvalue) {
    exp = atoi (expires->gvalue);
    if (RegistrationInfoListUpdateExpiresById (&RegistrationInfoList, eXosipEvent->rid, exp)) {
    // fprintf (stderr, "could not find register with rid of %d\n", eXosipEvent->rid);
    }
   }
  }

  // look for p-associated-uri headers and do something with them
  // could not find a better way to do this but by hand 

  osip_list_t *hlist = &eXosipEvent->response->headers;
  osip_header_t *gheader = NULL;
  char uris[250];

  int pos = 0;

  while (hlist && !osip_list_eol (hlist, pos)) {

   gheader = (osip_header_t *) osip_list_get (hlist, pos);

   //if (gheader->hname && gheader->hvalue)
    //fprintf (stderr, " generic header name = %s generic value = %s\n", gheader->hname, gheader->hvalue);

   if (gheader && gheader->hname && !strcasecmp (gheader->hname, "p-associated-uri")) {
    if (gheader->hvalue) {
     snprintf (uris, sizeof (uris), "%s", gheader->hvalue);
     //fprintf (stderr, "%s: found pheader uri values = %s\n", __func__, gheader->hvalue);
    }
    break;
   }
   pos++;
  }

  // end p-header

 }                              /* end if exosip->response */

 dynVarLog (__LINE__, -1, (char *) __func__, REPORT_NORMAL, TEL_BASE, INFO, 
	"Successfully registered to proxy server <%s> [%d:%s]", gOutboundProxy,
	osip_message_get_status_code (eXosipEvent->response), eXosipEvent->textinfo);

 return rc;
}

int
unsuccessfulRegistrationHandler (eXosip_event_t * eXosipEvent)
{

 // based on reason remove or retry
 // registration for a particular registration attempt
 // 401 and 407 invoke a similar response 
 // will eventually need to pull reg id from a list of current registrations


 // fprintf(stderr, " %s reason=%s\n", __func__, eXosipEvent->textinfo);


 int rc = 0;
 osip_message_t *reg = NULL;
 int RegistrationStatus = 0;

 if(eXosipEvent && eXosipEvent->response)
  RegistrationStatus = osip_message_get_status_code (eXosipEvent->response);

 dynVarLog (__LINE__, -1, (char *) __func__, REPORT_NORMAL, TEL_REGISTRATION_ERROR, ERR, 
		"Unsuccessful register to proxy server <%s>, [%d:%s]", gOutboundProxy, RegistrationStatus,
		eXosipEvent->textinfo);

 if (RegistrationStatus == 407 || RegistrationStatus == 401) {

  RegistrationInfo_t *reginfo = NULL;

  reginfo = RegistrationInfoListGetById (&RegistrationInfoList, eXosipEvent->rid);

  if (reginfo) {

   dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE, TEL_BASE, INFO, 
		"Adding authentication info (user:%s, pass:%s, id:%s, realm:%s)",
		reginfo->username, reginfo->password, reginfo->id, reginfo->realm);

   RegistrationInfoFree (reginfo);

   eXosip_lock (geXcontext);
   eXosip_automatic_action(geXcontext);
   // eXosip_register_send_register (eXosipEvent->rid, reg);
   eXosip_unlock (geXcontext);

  }
  // fix the above  later .. this used to ne needed 
  // 
  else {
   eXosip_lock(geXcontext);
   eXosip_automatic_action (geXcontext);
   eXosip_unlock(geXcontext);
  }
 }

 // if 404 or some other error remove registration from list 

 if (RegistrationStatus == 404 || RegistrationStatus == 403) {

  RegistrationInfo_t *reginfo = NULL;

  reginfo = RegistrationInfoListGetById (&RegistrationInfoList, eXosipEvent->rid);

  if (reginfo) {
   dynVarLog (__LINE__, -1, (char *) __func__, REPORT_VERBOSE, TEL_BASE, INFO, 
		"Removing Registration after 403/404 error (user:%s, pass:%s, id:%s, realm:%s).",
         reginfo->username, reginfo->password, reginfo->id, reginfo->realm);
  }

  RegistrationInfoFree (reginfo);
  RegistrationInfoListDeleteById (&RegistrationInfoList, eXosipEvent->rid);
  eXosip_lock(geXcontext);
  eXosip_register_remove (geXcontext, eXosipEvent->rid);
  eXosip_unlock(geXcontext);

 }                              // end bad registration

 return rc;
}
