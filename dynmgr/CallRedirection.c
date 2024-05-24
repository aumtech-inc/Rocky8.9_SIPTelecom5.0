#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <eXosip2/eXosip.h>
#include <osip2/osip.h>

#include "InboundRegistrationInfo.h"
#include "dynVarLog.h"
#include "regex_redirection.h"

#define IMPORT_GLOBAL_EXTERNS
#include "ArcSipCommon.h"

int sendShutdownToDM (int dynMgr);

extern int gCallMgrRecycleBase;
extern int gCallMgrRecycleDelta;
extern ActiveInboundRegistrations_t *ActiveRegistrationList;
extern char gHostTransport[10];
extern int gSipRedirectionUseTable;
extern int gSipRedirectionDefaultRule;

extern arc_sip_redirection_t sip_redirection_table[SIP_REDIRECTION_TABLE_SIZE];

int
callRedirectionHandler (eXosip_event_t * eXosipEvent)
{

   int rc = -1;

   osip_to_t *to = NULL;

   char contactstr[255];
   int status = 503;
   char *ipaddress = NULL;
   char *port = NULL;
   size_t count = 0;
   int type = -1;
   int dynMgr;
   osip_uri_param_t *netannc = NULL;
   // for redirection table
   char dnis[256] = "arc";
   char dest_ip[256] = "NotFound";
   int dest_port = 5060;
   int dest_idx;
   int dest_found = 0;
   int redirection_status;
   char buff[256];
   int i, j;
   char *routing_rule = (char *)"Not Set";

   osip_message_t *answer = NULL;

   if (eXosipEvent->request == NULL) {
      return -1;
   }

   // sip redirection table 

   if (gSipRedirectionUseTable == 1) {

          if(gDnisSource == DNIS_SOURCE_TO)
    {
        to = eXosipEvent->request->to;
    }
    else
    {
        osip_to_init(&to);
        to->url = osip_message_get_uri(eXosipEvent->request);
    }

      if (to != NULL) {
         if (to->url->username != NULL) {
            snprintf (dnis, sizeof (dnis), "%s", to->url->username);
         }
      }

      switch (gSipRedirectionDefaultRule) {
      case SIP_REDIRECTION_RULE_ROUTE:
         routing_rule = (char *)"Route";

         for (j = 0, i = 0; i < SIP_REDIRECTION_TABLE_SIZE, j < SIP_REDIRECTION_TABLE_SIZE; i++, j++) {
            dest_idx = -1;
            // fprintf (stderr, " %s() following rule route if not found... dnis=%s i=%d j=%d\n", __func__, dnis, i, j);
            dest_idx = arc_sip_redirection_find_next (sip_redirection_table, SIP_REDIRECTION_TABLE_SIZE, j, dnis);
            if ((i == 0) && (dest_idx == -1)) {
               // found nothing on first try  
               // fall back to the active list 
               ActiveRegistrationListGetContact (ActiveRegistrationList, &type, &ipaddress, &port, &count, &dynMgr);
               dest_found++;
               break;
            }
            else 
            if(dest_idx > -1){
               redirection_status =
                  arc_sip_redirection_get_destination (sip_redirection_table, SIP_REDIRECTION_TABLE_SIZE, dest_idx, dest_ip, sizeof (dest_ip), &dest_port);

               if (redirection_status > -1) {
                  dynVarLog (__LINE__, -1,  __func__, REPORT_VERBOSE, TEL_BASE,
                             INFO, " The SIP redirection table returned destination [%s:%d], based on rule indexed at %d", 
                             dest_ip, dest_port, dest_idx);
                  ipaddress = strdup (dest_ip);
                  snprintf (buff, sizeof (buff), "%d", dest_port);
                  port = strdup (buff);
                  dest_found++;
                  break;
               } else 
               if(redirection_status == -1 && dest_idx == SIP_REDIRECTION_DEFAULT_ROUTE){
                 dynVarLog (__LINE__, -1,  __func__, REPORT_VERBOSE, TEL_BASE,
                             INFO, " The SIP redirection Destination Table failed to get destination using rule at default index %d, trying any actively registered host!", dest_idx);
                 ActiveRegistrationListGetContact (ActiveRegistrationList, &type, &ipaddress, &port, &count, &dynMgr);
                 dest_found++;
                 break;
               } else {
                  dynVarLog (__LINE__, -1,  __func__, REPORT_VERBOSE, TEL_BASE,
                             INFO, " The SIP redirection Destination Table [rc=%d] failed to get destination using rule at index %d", redirection_status,
                             dest_idx);
               }
            } 
         }
         if(dest_found == 0){
              dynVarLog (__LINE__, -1,  __func__, REPORT_VERBOSE, TEL_BASE,
                        INFO, " The SIP redirection Destination table failed to get a destination, trying any actively registered host!");
              ActiveRegistrationListGetContact (ActiveRegistrationList, &type, &ipaddress, &port, &count, &dynMgr);
         }

         break;
      case SIP_REDIRECTION_RULE_REJECT:
         routing_rule = (char *)"Reject";
         dest_idx = arc_sip_redirection_find_next (sip_redirection_table, SIP_REDIRECTION_TABLE_SIZE, 0, dnis);
         if (dest_idx > -1) {
            redirection_status =
               arc_sip_redirection_get_destination (sip_redirection_table, SIP_REDIRECTION_TABLE_SIZE, dest_idx, dest_ip, sizeof (dest_ip), &dest_port);

            if (redirection_status > -1) {
               dynVarLog (__LINE__, -1,  __func__, REPORT_VERBOSE, TEL_BASE,
                          INFO, " The SIP redirection table returned destination[%s:%d], based on rule indexed at %d, Routing Rule set to[%s]", 
                          dest_ip, dest_port, dest_idx, routing_rule);
               ipaddress = strdup (dest_ip);
               snprintf (buff, sizeof (buff), "%d", dest_port);
               port = strdup (buff);
            }
         }
         else if (dest_idx == -1) {
            // move this rule check to the top of the loop 
            status = 503;
         }

         break;
      default:
         status = 503;
      }

   }

   // old behavior 

   if (gSipRedirectionUseTable == 0) {
      // fprintf(stderr, " %s() use table = 0\n", __func__);
      ActiveRegistrationListGetContact (ActiveRegistrationList, &type, &ipaddress, &port, &count, &dynMgr);
   }

   if(gDnisSource == DNIS_SOURCE_TO)
    {
        to = eXosipEvent->request->to;
    }
    else
    {
        osip_to_init(&to);
        to->url = osip_message_get_uri(eXosipEvent->request);
   }


   if (to && ipaddress) {

      osip_to_param_get_byname (to, (char*)"play", &netannc);

      if (!to->url->username) {
         to->url->username = osip_strdup ("arc");
      }


      if (netannc && netannc->gvalue) {
         if (port) {
            snprintf (contactstr, sizeof (contactstr), "<%s:%s@%s:%s>;play=%s", to->url->scheme, to->url->username, ipaddress, port, netannc->gvalue);
         }
         else {
            snprintf (contactstr, sizeof (contactstr), "<%s:%s@%s>;play=%s", to->url->scheme, to->url->username, ipaddress, netannc->gvalue);
         }
      }
      else {
         if (port) {
            snprintf (contactstr, sizeof (contactstr), "<%s:%s@%s:%s>", to->url->scheme, to->url->username, ipaddress, port);
         }
         else {
            snprintf (contactstr, sizeof (contactstr), "<%s:%s@%s>", to->url->scheme, to->url->username, ipaddress);
         }
      }

      if (strcmp (gHostTransport, "tcp") == 0) {
         int len = strlen (contactstr);
         snprintf (&contactstr[len - 1], sizeof (contactstr) - len, ";%s>", "transport=tcp");
      }
      else if (strcmp (gHostTransport, "tls") == 0) {
         int len = strlen (contactstr);
         snprintf (&contactstr[len - 1], sizeof (contactstr) - len, ";%s>", "transport=tls");
      }
      else {
         ;;
      }

      eXosip_lock (geXcontext);
      eXosip_call_build_answer (geXcontext, eXosipEvent->tid, 302, &answer);
      eXosip_unlock (geXcontext);

      dynVarLog (__LINE__, -1, __func__, REPORT_VERBOSE, TEL_BASE, INFO, " Redirecting Call: New Destination=%s count=%d", contactstr, count);

      if (answer) {
         osip_message_set_contact (answer, contactstr);
      }

      status = 302;
   }
   else {
      dynVarLog (__LINE__, -1,  __func__, REPORT_VERBOSE, TEL_BASE, INFO, " No Destination Found! Sending 503 for DNIS[%s], Routing Rule set to [%s]", dnis, routing_rule);
      eXosip_lock (geXcontext);
      eXosip_call_build_answer (geXcontext, eXosipEvent->tid, 503, &answer);
      eXosip_unlock (geXcontext);
      status = 503;
   }

   if (answer) {
      eXosip_lock (geXcontext);
      rc = eXosip_call_send_answer (geXcontext, eXosipEvent->tid, status, answer);
      eXosip_unlock (geXcontext);
   }


   if (type == ACTIVE_REGISTRATION_TYPE_LOCAL) {
      if (count >= (gCallMgrRecycleBase + (dynMgr * gCallMgrRecycleDelta))) {

         // fprintf(stderr, " %s deleting dynmgr=%d count=%d\n", __func__, dynMgr, count);

         dynVarLog (__LINE__, -1,  __func__, REPORT_DETAIL, DYN_BASE, INFO,
                    "%d calls have been sent to Call Manager(%d), sending DMOP_SHUTDOWN.", count, dynMgr);

         // gDynMgrList[dynMgrId].status = DOWN;
         // delete from active registration list here
         char username[256];
         snprintf (username, sizeof (username), "%s%d", LOCAL_DYNMGR_NAME, dynMgr);
         ActiveRegistrationDelete (&ActiveRegistrationList, username, ipaddress, port);

         sendShutdownToDM (dynMgr);
      }
   }


   if (ipaddress)
      free (ipaddress);
   if (port)
      free (port);

   if(gDnisSource == DNIS_SOURCE_TO)
   {
        ;
   }
   else
   {
        to->url = NULL;
        osip_to_free(to);
   }

   return rc;
}
