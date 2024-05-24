#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <eXosip2/eXosip.h>

#include "RegistrationInfo.h"
#include "RegistrationHandlers.h"
#include "dynVarLog.h"

extern struct eXosip_t *geXcontext;

extern int get_name_value (char *section, char *name, char *defaultVal, char *value,
        int bufSize, char *file, char *err_msg);


#define MAXAUTHINFOS 100

extern RegistrationInfoList_t RegistrationInfoList;


#define FREE(ptr) do { free(ptr); ptr=NULL; } while(0)

static int
RegistrationInfoLoadConfig_internal (char *gHostIp, unsigned int gSipPort, int gDynMgrId, char *gIpCfg, char *section,
		char *gSipUserAgent);

int
RegistrationInfoLoadConfig (char *gHostIp, unsigned int gSipPort, int gDynMgrId, char *gIpCfg, char *gSipUserAgent)
{
	char section[64];

	sprintf(section, "%s", "SIPAuthenticationInfo");	
	RegistrationInfoLoadConfig_internal (gHostIp, gSipPort, gDynMgrId, gIpCfg, section, gSipUserAgent);

	if(gDynMgrId > -1)
	{
		sprintf(section, "CM%d_%s", gDynMgrId, "SIPAuthenticationInfo");	
		RegistrationInfoLoadConfig_internal (gHostIp, gSipPort, gDynMgrId, gIpCfg, section, gSipUserAgent);
	}
	return(0);
}

static int
RegistrationInfoLoadConfig_internal (char *gHostIp, unsigned int gSipPort, int gDynMgrId, char *gIpCfg, char *section,
		char *gSipUserAgent)
{

 int i;
 int rc = 0;
 int j;
 char contacturi[255];
 char fromuri[255];
 char proxyuri[255];

 enum transport_e {
    UDP, 
    TCP, 
    TLS
 };

 enum auth_index_e
 {
  REG_USERNAME = 0,
  REG_ID,
  REG_PASSWORD,
  REG_REALM,
  REG_PROXY_IP,
  REG_PROXY_PORT,
  REG_ROUTE,
  REG_TRANSPORT,
  REG_REGISTRATION_INTERVAL,
  REG_SUPPORTED,
  REG_SUPPORTED_100REL,
  REG_SUPPORTED_PATH, 
  REG_SPAN_ID, 
  // must be at the end do not move
  REG_END
 };

 char auth_info_values[REG_END][256] = { 0 };

 const char *auth_info_names[] = {
  "Username",
  "ID",
  "Password",
  "Realm",
  "ProxyIP",
  "ProxyPort",
  "Route",
  "Transport",
  "RegistrationInterval",
  "Supported",
  "Supported100rel", 
  "SupportedPathHeader",
  "Span",
  NULL
 };

 char ConfigFileContext[256];
 char ConfigErrStr[256];

 for (i = 0; i < MAXAUTHINFOS; i++) {

  memset (auth_info_values, 0, sizeof (auth_info_values));

  snprintf (ConfigFileContext, sizeof (ConfigFileContext), "%s%d", section, i);

  for (j = 0; auth_info_names[j]; j++) {

   if (get_name_value (ConfigFileContext,
					(char *)auth_info_names[j],
					"",
					auth_info_values[j],
					sizeof (auth_info_values[j]),
					gIpCfg, ConfigErrStr)) {

    // fprintf(stderr, " %s: config err string = %s\n", __func__, ConfigErrStr);
   }
   else {
     // fprintf (stderr, " %s: context=<%s> config name=<%s> config value=<%s>\n", __func__, ConfigFileContext, auth_info_names[j], auth_info_values[j]);
     ;;
   }
  }

  if (auth_info_values[REG_USERNAME][0]
      && auth_info_values[REG_PASSWORD][0]
      && auth_info_values[REG_REALM][0]) {

   // add to eXosip's auth info
   eXosip_add_authentication_info (geXcontext, auth_info_values[REG_USERNAME], auth_info_values[REG_ID], auth_info_values[REG_PASSWORD], NULL, auth_info_values[REG_REALM]);
  }

  // if we have a proxy ip and register interval start that --- 
  if (auth_info_values[REG_PROXY_IP][0]
      && auth_info_values[REG_USERNAME][0]
      && auth_info_values[REG_REGISTRATION_INTERVAL][0]
   ) {

   int expires;
   char contacturi[256];
   char fromuri[256];
   char proxyuri[256];
   char *proxyport = "5060";
   char *transport = "udp";
   char supported[256] = "";
   int regid;
   int span = -1;
   unsigned short portno;

   expires = atoi (auth_info_values[REG_REGISTRATION_INTERVAL]);

   if (auth_info_values[REG_PROXY_PORT][0]) {
    proxyport = auth_info_values[REG_PROXY_PORT];
   }

   if (auth_info_values[REG_TRANSPORT][0]) {
    if (!strcasecmp (auth_info_values[REG_TRANSPORT], "udp")
        || !strcasecmp (auth_info_values[REG_TRANSPORT], "tcp")
        || !strcasecmp (auth_info_values[REG_TRANSPORT], "tls")
     ) {
     transport = auth_info_values[REG_TRANSPORT];
    }
   }

   portno = atoi (proxyport);

   //
   // span if span is set then register only if span == gDynMgrId 
   //

   
   if(auth_info_values[REG_SPAN_ID][0] != '\0'){
      span = atoi(auth_info_values[REG_SPAN_ID]);
//fprintf(stderr, " %s() span=%d\n", __func__, span);
   } 


#define STRCAT_CSV(dest, str) \
        if((dest[0] == '\0') || (dest[strlen(dest) - 1] == ',')){ \
           snprintf(&dest[strlen(dest)], sizeof(dest) - strlen(dest), "%s", str); \
        } else { \
           snprintf(&dest[strlen(dest)], sizeof(dest) - strlen(dest), ",%s", str); \
        } \

   // building supported: string from auth info for registrations

   supported[0] = '\0';

   if(auth_info_values[REG_SUPPORTED][0]){
     STRCAT_CSV(supported, auth_info_values[REG_SUPPORTED]);
   } 
   else 
   if (auth_info_values[REG_SUPPORTED_100REL][0] && (strcasecmp(auth_info_values[REG_SUPPORTED_100REL], "yes") == 0)){
     STRCAT_CSV(supported, "100rel");
   } 
   else 
   if (auth_info_values[REG_SUPPORTED_PATH][0] && (strcasecmp(auth_info_values[REG_SUPPORTED_PATH], "yes") == 0)){
      STRCAT_CSV(supported, "path");
   }
   else
   {
		sprintf(supported, "%s", "replaces");
   }


	if ( (gSipUserAgent != NULL ) && (strlen(gSipUserAgent) > 2) )
	{
		eXosip_set_user_agent (geXcontext, ( const char *) gSipUserAgent);
	}

   if (expires > 0) {

    // addign transport to tcp and tls in contact 
    // snprintf (contacturi, sizeof (contacturi), "<sip:%s@%s:%d>", auth_info_values[REG_USERNAME], gHostIp, gSipPort);
    snprintf (contacturi, sizeof (contacturi), "<sip:%s@%s:%d;transport=%s>", auth_info_values[REG_USERNAME], gHostIp, gSipPort, transport);
    snprintf (fromuri, sizeof (fromuri), "<sip:%s@%s>", auth_info_values[REG_USERNAME], auth_info_values[REG_PROXY_IP]
     );
    snprintf (proxyuri, sizeof (proxyuri), "sip:%s:%d", auth_info_values[REG_PROXY_IP], portno);

    // add to registration list

    regid = newRegistrationHandler (fromuri, proxyuri, contacturi, supported, auth_info_values[REG_ROUTE], expires);
    // fprintf(stderr, " %s: regid = %d\n", __func__, regid );

    RegistrationInfoListAdd (&RegistrationInfoList, auth_info_values[REG_USERNAME], auth_info_values[REG_ID], auth_info_values[REG_PASSWORD],
                             auth_info_values[REG_REALM], proxyuri, NULL, expires, transport, regid);

   }
  }
 }                              // end for loop

 return rc;
}

// functions that return the structure should do a deep copy 


static RegistrationInfo_t *
_registration_info_deep_copy (RegistrationInfo_t * elem)
{

 RegistrationInfo_t *rc = NULL;

 if (elem) {

  rc = (RegistrationInfo_t *) calloc (1, sizeof (RegistrationInfo_t));

  if (rc) {
   memcpy (rc, elem, sizeof (RegistrationInfo_t));
   if (elem->username)
    rc->username = strdup (elem->username);
   if (elem->id)
    rc->id = strdup (elem->id);
   if (elem->password)
    rc->password = strdup (elem->password);
   if (elem->realm)
    rc->realm = strdup (elem->realm);
   if (elem->proxyuri)
    rc->proxyuri = strdup (elem->proxyuri);
   if (elem->route)
    rc->route = strdup (elem->route);
   if (elem->transport)
    rc->route = strdup (elem->transport);
  }
 }

 return rc;
}


// added this so that you only have to update this in one spot 
// if the structure changes...
// also frees the deep copy returned to calling code 

void
RegistrationInfoFree (RegistrationInfo_t * elem)
{

 if (elem) {

  if (elem->username)
   FREE(elem->username);
  if (elem->id)
   FREE(elem->id);
  if (elem->password)
   FREE(elem->password);
  if (elem->realm)
   FREE(elem->realm);
  if (elem->proxyuri)
   FREE(elem->proxyuri);
  if (elem->route)
   FREE(elem->route);
  if (elem->transport)
   FREE(elem->transport);

  FREE(elem);
 }
 return;
}


int
RegistrationInfoListInit (RegistrationInfoList_t * RegInfo)
{

 int rc = -1;

 if (RegInfo) {
  memset (RegInfo, 0, sizeof (RegistrationInfoList_t));
  pthread_mutex_init (&RegInfo->mutex, NULL);
  rc = 0;
 }

 return rc;
}


int
RegistrationInfoListAdd (RegistrationInfoList_t * list, char *username, char *id, char *password, char *realm, char *proxyuri, char *route, int expires,
                         char *transport, int regid, int span)
{

 int rc = -1;

 RegistrationInfo_t *elem = NULL;
 RegistrationInfo_t *prev = NULL;

 elem = (RegistrationInfo_t *) calloc (1, sizeof (RegistrationInfo_t));

 pthread_mutex_lock (&list->mutex);

 if (elem) {

  username ? elem->username = strdup (username) : 0;
  id ? elem->id = strdup (id) : 0;
  password ? elem->password = strdup (password) : 0;
  realm ? elem->realm = strdup (realm) : 0;
  proxyuri ? elem->proxyuri = strdup (proxyuri) : 0;
  route ? elem->route = strdup (route) : 0;

  elem->expires = expires;
  elem->regid = regid;
  elem->span = span;

  time (&elem->last);

  if (list->head == NULL) {
   list->head = elem;
   list->tail = elem;
  }
  else {
   prev = list->tail;
   prev->next = elem;
   list->tail = elem;
  }
  list->count++;
  rc = 0;
 }

 pthread_mutex_unlock (&list->mutex);

 return rc;
}

int
RegistrationInfoListDeleteById (RegistrationInfoList_t * list, int regid)
{

 int rc = -1;
 RegistrationInfo_t *elem = NULL;
 RegistrationInfo_t *prev = NULL;

 pthread_mutex_lock (&list->mutex);

 elem = list->head;

 while (elem) {

  if (elem->regid == regid) {

   if (prev) {
    prev->next = elem->next;
   }
   else {
    list->head = elem->next;
   }
   RegistrationInfoFree (elem);
   list->count--;
   rc = 0;
   break;
  }
  prev = elem;
  elem = elem->next;
 }

 pthread_mutex_unlock (&list->mutex);

 return rc;
}

RegistrationInfo_t *
RegistrationInfoListGetById (RegistrationInfoList_t * list, int regid)
{

 RegistrationInfo_t *rc = NULL;
 RegistrationInfo_t *elem = NULL;

 pthread_mutex_lock (&list->mutex);

 elem = list->head;

 while (elem) {
  if (elem->regid == regid) {
   rc = _registration_info_deep_copy (elem);
   break;
  }
  elem = elem->next;
 }

 pthread_mutex_unlock (&list->mutex);

 return rc;
}

int
RegistrationInfoListUpdateExpiresById (RegistrationInfoList_t * list, int regid, int expires)
{

 int rc = -1;

 RegistrationInfo_t *elem = NULL;

 pthread_mutex_lock (&list->mutex);

 elem = list->head;

 while (elem) {

  if (elem->regid == regid) {
   if (expires >= 0) {
    elem->expires = expires;
    rc = 0;
   }
   break;
  }
  elem = elem->next;
 }

 pthread_mutex_unlock (&list->mutex);

 return rc;
}


int
RegistrationInfoListUpdateLastRegister (RegistrationInfoList_t * list, int regid)
{

 int rc = -1;
 RegistrationInfo_t *elem = NULL;

 pthread_mutex_lock (&list->mutex);

 elem = list->head;

 while (elem) {
  if (elem->regid == regid) {
   time (&elem->last);
   rc = 0;
   break;
  }
  elem = elem->next;
 }

 pthread_mutex_unlock (&list->mutex);

 return rc;
}

RegistrationInfo_t *
RegistrationInfoListFindExpires (RegistrationInfoList_t * list)
{

 RegistrationInfo_t *elem = NULL;
 RegistrationInfo_t *rc = NULL;
 time_t now;

 time (&now);

 pthread_mutex_lock (&list->mutex);

 elem = list->head;

 while (elem) {
  if (((elem->last + elem->expires) - 5) <= now) {
   rc = _registration_info_deep_copy (elem);
   break;
  }
  elem = elem->next;
 }

 pthread_mutex_unlock (&list->mutex);

 return rc;
}


void
RegistrationInfoListFree (RegistrationInfoList_t * list)
{

 RegistrationInfo_t *elem = NULL;
 RegistrationInfo_t *next = NULL;

 //  this is here so as to reduce false positives for memory leaks 
 //  most of the time there will be no freeing of the Registration list 
 //  but you may want to unregister all registrations if a signal is sent to 
 //  tear down the app and use this free function .

 pthread_mutex_lock (&list->mutex);

 elem = list->head;

 while (elem) {
  next = elem->next;
  RegistrationInfoFree (elem);
  elem = next;
 }

 list->count = 0;

 list->head = NULL;

 pthread_mutex_unlock (&list->mutex);

 return;
}

#ifdef MAIN
// unit test driver 
// fill the structure delete individual elements 
// fill again -- free all 
// 

struct reginfo_test_t
{
 char *username;
 char *id;
 char *password;
 char *realm;
 char *proxyip;
 int expires;
};

struct reginfo_test_t reginfo_test[] = {
 {"1000", "login", "password", "aumtechinc.com", "10.0.0.92", 200},
 {"1001", "login", "password", "aumtechinc.com", "10.0.0.92", 200},
 {"1002", "login", "password", "aumtechinc.com", "10.0.0.92", 200},
 {"1003", "login", "password", "aumtechinc.com", "10.0.0.92", 200},
 {"1004", "login", "password", "aumtechinc.com", "10.0.0.92", 200},
 {NULL, NULL, NULL, NULL, 0}
};

// 

#include <mcheck.h>

RegistrationInfoList_t RegistrationInfoList;

int
main ()
{

 RegistrationInfoListInit (&RegistrationInfoList);
 int i;

 //mtrace ();

 for (i = 0; reginfo_test[i].username; i++) {
  RegistrationInfoListAdd (&RegistrationInfoList, reginfo_test[i].username, reginfo_test[i].id, reginfo_test[i].password,
                           reginfo_test[i].realm, reginfo_test[i].proxyip, NULL, reginfo_test[i].expires, i);
 }

 for (i = 0; i; i++) {
  if (RegistrationInfoListDeleteById (&RegistrationInfoList, i)) {
   break;
  }
 }

 for (i = 0; reginfo_test[i].username; i++) {
  RegistrationInfoListAdd (&RegistrationInfoList, reginfo_test[i].username, reginfo_test[i].id, reginfo_test[i].password,
                           reginfo_test[i].realm, reginfo_test[i].proxyip, NULL, reginfo_test[i].expires, i);
 }

 RegistrationInfoListFree (&RegistrationInfoList);
 //muntrace ();

 return 0;
}


#endif
