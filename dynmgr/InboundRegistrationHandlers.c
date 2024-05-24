#include <stdio.h>
#include <eXosip2/eXosip.h>

#include "dynVarLog.h"
#include "InboundRegistrationInfo.h"
#include "regex_redirection.h"

extern ActiveInboundRegistrations_t *ActiveRegistrationList;
extern InboundRegistrationInfo_t *InboundRegistrationList;
extern struct eXosip_t *geXcontext;

extern arc_sip_redirection_t sip_redirection_table[SIP_REDIRECTION_TABLE_SIZE];

//
// if it is in the list return a 200 and note 
// the contact from the incoming REGISTRATION 
//
// in the future we will do a challenge auth
// but for now just a 200 OK or a 404 are returned 
// the contact address must match from 
// a list of ip's from the config file 
// 
// one downside is that we can't get presently 
// the real socket level ip of the sender from the stack
//
// added a check for unregistration i.e. expires=0
// user for now must supply a valid contact header in all cases even expiring 
// a registration 
//

int
inboundRegistrationHandler (eXosip_event_t * eXosipEvent)
{

  int rc = -1;
  int status = 404;

  osip_message_t *answer = NULL;
  osip_list_t *contact_list = NULL;
  osip_list_t *header_list = NULL;
  osip_header_t *expires = NULL;
  osip_header_t *header = NULL;
  osip_header_t *expires_in = NULL;
  osip_from_t *from = NULL;
  osip_contact_t *contact = NULL;
  osip_uri_t *uri = NULL;

  int interval = 1200;
  char buff[80];
  char *passwd = NULL;
   // for the redirection table 
  int port = 5060;
  int entries;


  if (eXosipEvent->request) {

    from = eXosipEvent->request->from;
    contact_list = &eXosipEvent->request->contacts;
    contact = (osip_contact_t *) osip_list_get (contact_list, 0);
    header_list = &eXosipEvent->request->headers;

    int i = 0;
    while(header = (osip_header_t *)osip_list_get(header_list, i)){
       if(!strcasecmp(header->hname, "Expires")){
          expires_in = header;
       }
      i++;
    }

    if (from && contact) {
      
      int unregister = 0;

      uri = from->url;

      if(!uri || !uri->username || !contact->url){
          dynVarLog (__LINE__, -1,(char *) __func__, REPORT_NORMAL, TEL_BASE,ERR, "", "Either Username or Contact URL is not set, cannont continue to lookup");
          return rc;
      }

      rc = InboundRegistrationListFind(InboundRegistrationList, uri->username, contact->url->host, "", &passwd, &interval);

      snprintf(buff, sizeof(buff), "%d", interval);

      if(expires_in && (atoi(expires_in->hvalue) == 0)){
        unregister++;
      }

      if((rc == 0) && (unregister == 0)){
        osip_header_init (&expires);
        osip_header_set_name (expires, strdup ("Expires"));
        osip_header_set_value (expires, strdup (buff));
        status = 200;
        if(contact->url->port){
          port = atoi(contact->url->port);
        }
        ActiveRegistrationListAdd(&ActiveRegistrationList, ACTIVE_REGISTRATION_TYPE_REMOTE, uri->username, interval, -1, -1, contact->url->host, contact->url->port);
        entries = arc_sip_redirection_set_active (sip_redirection_table, SIP_REDIRECTION_TABLE_SIZE, contact->url->host, port, SIP_REDIRECTION_ACTIVATE);        
        dynVarLog (__LINE__, -1,(char *) __func__, REPORT_VERBOSE, TEL_BASE,INFO, " Adding Active Registration for Username=%s and IP Address=%s Port=%s Redirection Table Entries=%d", uri->username, contact->url->host, contact->url->port, entries);

      } else 

      if((rc == 0) && unregister){
        status = 200;
        if(contact->url->port){
          port = atoi(contact->url->port);
        }
        ActiveRegistrationDelete(&ActiveRegistrationList, uri->username, contact->url->host, contact->url->port);
        entries = arc_sip_redirection_set_active (sip_redirection_table, SIP_REDIRECTION_TABLE_SIZE, contact->url->host, port, SIP_REDIRECTION_DEACTIVATE);        
        dynVarLog (__LINE__, -1,(char *) __func__, REPORT_VERBOSE, TEL_BASE, INFO, 
			" Deleting Active Registration for Username=%s and IP Address=%s Port=%s Redirection Table Entries=%d", 
			uri->username, contact->url->host, contact->url->port, entries);

      }
    }

    eXosip_lock (geXcontext);
    eXosip_message_build_answer (geXcontext, eXosipEvent->tid, status, &answer);

    if (answer) {
      if(expires)
         osip_list_add (&answer->headers, expires, 0);
      eXosip_message_send_answer (geXcontext, eXosipEvent->tid, status, answer);
    }
  }

  eXosip_unlock (geXcontext);

  if(passwd)
     free(passwd);

  return rc;
}


