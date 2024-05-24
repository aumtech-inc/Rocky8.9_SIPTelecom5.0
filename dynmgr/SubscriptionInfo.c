#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "SubscriptionInfo.h"
#include "SubscriptionHandlers.h"

#define MAXSUBSCRIPTIONS 100

extern int get_name_value (char *section, char *name, char *defaultVal, char *value,
        int bufSize, char *file, char *err_msg);
int
SubscriptionInfoLoadConfig (char *gHostIp, unsigned int gSipPort, char *gIpCfg)
{

  int rc = -1;
  int i;

  for (i = 0; i < MAXSUBSCRIPTIONS; i++) {

    int j;
    char ConfigFileContext[256];
    char ConfigErrStr[256];

    enum subscription_info_index
    {
      SUB_USERNAME = 0,
      SUB_TOIP,
      SUB_TOPORT,
      SUB_TRANSPORT,
      SUB_ROUTE,
      SUB_EVENT,
      SUB_INTERVAL,
      // always at the end!
      SUB_END
    };

    char *subscription_info_names[] = {
      "Username",
      "ToIP",
      "ToPort",
      "Transport",
      "Route",
      "Event",
      "SubscriptionInterval",
      NULL
    };

    char subscription_info_values[SUB_END][256];

    snprintf (ConfigFileContext, sizeof (ConfigFileContext), "%s%d", "SIPSubscriptionInfo", i);

    memset (subscription_info_values, 0, sizeof (subscription_info_values));

    for (j = 0; subscription_info_names[j]; j++) {
      if (get_name_value
          (ConfigFileContext, subscription_info_names[j], "", subscription_info_values[j], sizeof (subscription_info_values[j]), gIpCfg, ConfigErrStr)) {
        ;;
      }
      else {
        ;;
      }

      if (subscription_info_values[SUB_USERNAME][0]
          && subscription_info_values[SUB_TOIP][0]
          && subscription_info_values[SUB_EVENT][0]
        ) {

        char touri[256];
        char fromuri[256];
        unsigned short interval = 1800;
        unsigned short portno = 5060;

        if (subscription_info_values[SUB_TOPORT][0]) {
          portno = atoi (subscription_info_values[SUB_TOPORT]);
        }

        if (subscription_info_values[SUB_INTERVAL][0]) {
          interval = atoi (subscription_info_values[SUB_INTERVAL]);
        }

        snprintf (touri, sizeof (touri), "sip:%s@%s:%d", subscription_info_values[SUB_USERNAME], subscription_info_values[SUB_TOIP], portno);

        snprintf (fromuri, sizeof (fromuri), "sip:%s@%s:%d", subscription_info_values[SUB_USERNAME], gHostIp, gSipPort);

        newSubscriptionHandler (touri, fromuri, subscription_info_values[SUB_ROUTE], subscription_info_values[SUB_EVENT], interval);
        break;
      }
    }
  }

  return rc;
}


static SubscriptionInfo_t *_subinfo_deep_copy (SubscriptionInfo_t * elem);


// this is pretty much just dynamic storage 
// much like the list of registrations we will 
// delete subscriptions that fail catostrophically 
// just think of this as an sql row - updates will have to done 
// in user created functions


int
SubscriptionInfoInit (SubscriptionInfoList_t * list)
{

  int rc = -1;

  if (list) {
    memset (list, 0, sizeof (SubscriptionInfoList_t));
    pthread_mutex_init (&list->mutex, NULL);
    rc = 0;
  }

  return rc;
}


// 
// this is a special 
// subscription for inbound calls and signaled digits 
// these will be deleted 
// when the call is complete 
//

int
SubscriptionInfoAdd (SubscriptionInfoList_t * list, char *touri, char *fromuri, char *route, char *event, char *callid, int (*attachment) (void *),
                     int (*cb) (), int zCall)
{

  int rc = -1;

  SubscriptionInfo_t *elem = NULL;
  SubscriptionInfo_t *lptr = NULL;
  SubscriptionInfo_t *prev = NULL;

  pthread_mutex_lock (&list->mutex);

  lptr = list->head;

  elem = (SubscriptionInfo_t *) calloc (1, sizeof (SubscriptionInfo_t));

  if (elem) {

    touri ? elem->touri = strdup (touri) : 0;
    fromuri ? elem->fromuri = strdup (fromuri) : 0;
    route ? elem->route = strdup (route) : 0;
    event ? elem->event = strdup (event) : 0;
    callid ? elem->callid = strdup (callid) : 0;
    elem->zCall = zCall;

    elem->cb = cb;
    elem->attachment = attachment;

    if (lptr) {
      while (lptr) {
        if (lptr->next == NULL) {
          lptr->next = elem;
          break;
        }
        prev = lptr;
        lptr = lptr->next;
      }
    }
    else {
      list->head = elem;
    }
    list->count++;
    rc = 0;
  }

  pthread_mutex_unlock (&list->mutex);

  return rc;
}


// we needed to add this in two steps: once when we 
// initially send the subscribe and then when we get it back 
// we need to store the subscription id used by eXosip 
// but unlike registrations we do not have the id until the 
// 200 ok or failure comes back.


int
SubscriptionInfoUpdateExpiresByCallId (SubscriptionInfoList_t * list, char *callid, int expires)
{

  int rc = -1;

  SubscriptionInfo_t *elem = NULL;

  pthread_mutex_lock (&list->mutex);

  elem = list->head;

  while (elem) {
    if (!strcmp (elem->callid, callid)) {
      elem->expires = expires;
      rc = 0;
      break;
    }
    elem = elem->next;
  }

  pthread_mutex_unlock (&list->mutex);

  return rc;
}

int
SubscriptionInfoUpdateDid (SubscriptionInfoList_t * list, char *callid, int did)
{

  int rc = -1;

  SubscriptionInfo_t *elem = NULL;

  pthread_mutex_lock (&list->mutex);

  elem = list->head;

  while (elem) {
    if (!strcmp (elem->callid, callid)) {
      elem->did = did;
      rc = 0;
      break;
    }
    elem = elem->next;
  }

  pthread_mutex_unlock (&list->mutex);

  return rc;
}

/*@end*/


int
SubscriptionInfoUpdateCallId (SubscriptionInfoList_t * list, char *oldcallid, char *newcallid)
{

  int rc = -1;

  SubscriptionInfo_t *elem = NULL;

  pthread_mutex_lock (&list->mutex);

  elem = list->head;

  while (elem && oldcallid && newcallid) {
    if (!strcmp (elem->callid, oldcallid)) {
      free (elem->callid);
      elem->callid = strdup (newcallid);
      rc = 0;
      break;
    }
    elem = elem->next;
  }

  pthread_mutex_unlock (&list->mutex);

  return rc;
}



int
SubscriptionInfoUpdateLastSubscription (SubscriptionInfoList_t * list, char *callid)
{

  int rc = -1;

  SubscriptionInfo_t *elem = NULL;

  pthread_mutex_lock (&list->mutex);

  elem = list->head;

  while (elem) {
    if (!strcmp (elem->callid, callid)) {
      time (&elem->last);
      rc = 0;
      break;
    }
    elem = elem->next;
  }

  pthread_mutex_unlock (&list->mutex);

  return rc;
}



SubscriptionInfo_t *
SubscriptionInfoFindExpires (SubscriptionInfoList_t * list)
{

  SubscriptionInfo_t *rc = NULL;
  SubscriptionInfo_t *elem = NULL;

  time_t now;

  time (&now);

  pthread_mutex_lock (&list->mutex);

  elem = list->head;

  while (elem) {
    if (((elem->last + elem->expires) - 5) <= now) {
      rc = _subinfo_deep_copy (elem);
      break;
    }
    elem = elem->next;
  }

  pthread_mutex_unlock (&list->mutex);

  return rc;
}


SubscriptionInfo_t *
SubscriptionInfoFindByCallId (SubscriptionInfoList_t * list, char *callid)
{

  SubscriptionInfo_t *rc = NULL;
  SubscriptionInfo_t *elem = NULL;

  pthread_mutex_lock (&list->mutex);

  elem = list->head;

  if (callid) {
    while (elem) {
      if (elem->callid && (!strcmp (elem->callid, callid))) {
        rc = _subinfo_deep_copy (elem);
        break;
      }
      elem = elem->next;
    }
  }

  pthread_mutex_unlock (&list->mutex);
  return rc;
}



int
SubscriptionInfoDeleteByCallId (SubscriptionInfoList_t * list, char *callid)
{

  int rc = -1;

  SubscriptionInfo_t *elem = NULL;
  SubscriptionInfo_t *next = NULL;
  SubscriptionInfo_t *prev = NULL;

  pthread_mutex_lock (&list->mutex);

  elem = list->head;

  while (elem) {

    if (!strcmp (elem->callid, callid)) {

      next = elem->next;

      if (prev) {
        prev->next = elem->next;
      }
      else {
        list->head = elem->next;
      }

      SubscriptionInfoFree (elem);

      elem = next;
      list->count--;
      rc = 0;
      continue;
    }

    prev = elem;
    elem = elem->next;
  }

  pthread_mutex_unlock (&list->mutex);

  return rc;
}


int
SubscriptionInfoDeleteByzCall (SubscriptionInfoList_t * list, int zCall)
{

  int rc = -1;

  SubscriptionInfo_t *elem = NULL;
  SubscriptionInfo_t *next = NULL;
  SubscriptionInfo_t *prev = NULL;


//  fprintf(stderr, " %s() stub please remove\n", __func__);
  return 0;

  if(!list)
    return 0;

  pthread_mutex_lock (&list->mutex);

  elem = list->head;

  while (elem) {

    if (elem->zCall == zCall) {

      next = elem->next;

      if (prev) {
        prev->next = elem->next;
      }
      else {
        list->head = elem->next;
      }

      SubscriptionInfoFree (elem);

      elem = next;
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






static SubscriptionInfo_t *
_subinfo_deep_copy (SubscriptionInfo_t * elem)
{

  SubscriptionInfo_t *rc = NULL;

  if (elem) {

    rc = (SubscriptionInfo_t *) calloc (1, sizeof (SubscriptionInfo_t));

    if (rc) {
      memcpy (rc, elem, sizeof (SubscriptionInfo_t));
      elem->touri ? rc->touri = strdup (elem->touri) : 0;
      elem->fromuri ? rc->fromuri = strdup (elem->fromuri) : 0;
      elem->route ? rc->route = strdup (elem->route) : 0;
      elem->event ? rc->event = strdup (elem->event) : 0;
      elem->callid ? rc->callid = strdup (elem->callid) : 0;
    }
  }

  return rc;
}

void
SubscriptionInfoFree (SubscriptionInfo_t * info)
{

  if (info) {
    if (info->touri)
      free (info->touri);
    if (info->fromuri)
      free (info->fromuri);
    if (info->route)
      free (info->route);
    if (info->event)
      free (info->event);
    if (info->callid)
      free (info->callid);
    free (info);
  }

  return;
}

void
SubscriptionInfoListFree (SubscriptionInfoList_t * list)
{

  SubscriptionInfo_t *elem = NULL;
  SubscriptionInfo_t *next = NULL;

  pthread_mutex_lock (&list->mutex);

  elem = list->head;

  while (elem) {

    next = elem->next;
    SubscriptionInfoFree (elem);
    elem = next;
  }

  list->head = NULL;
  list->count = 0;

  pthread_mutex_unlock (&list->mutex);

  return;
}


#ifdef MAIN

#include <stdio.h>


int
test_attach (char **dest, void *data)
{

  int rc = 0;

  *dest = strdup ("this is a test attachment");

  return rc;
}


int 
test_cb (void *arg)
{

//  fprintf (stderr, "%s: test callback called\n", __func__);
  return 0;

}

struct subinfo_test_t
{
  char *touri;
  char *from;
  char *route;
  char *event;
  int expires;
  int (*cb) (void *);
};

struct subinfo_test_t subscription_info_test[] = {
  {"sip:1000@10.0.0.92", "sip:1000@10.0.0.92", NULL, "message-summary", 600, test_cb},
  {"sip:1001@10.0.0.92", "sip:1001@10.0.0.92", NULL, "message-summary", 600, test_cb},
  {"sip:1002@10.0.0.92", "sip:1002@10.0.0.92", NULL, "message-summary", 600, test_cb},
  {"sip:1003@10.0.0.92", "sip:1003@10.0.0.92", NULL, "message-summary", 600, test_cb},
  {"sip:1004@10.0.0.92", "sip:1004@10.0.0.92", NULL, "message-summary", 600, test_cb},
  {NULL, NULL, NULL, NULL, 0, NULL}
};


#include <mcheck.h>

int
main (int argc, char **argv)
{

  SubscriptionInfoList_t list;
  int i;


  //mtrace ();

  SubscriptionInfoInit (&list);

  for (i = 0; subscription_info_test[i].touri; i++) {
    SubscriptionInfoAdd (&list, subscription_info_test[i].touri,
                         subscription_info_test[i].from,
                         subscription_info_test[i].route, subscription_info_test[i].event, "callid", NULL, subscription_info_test[i].cb);
  }

  SubscriptionInfoListFree (&list);
  //muntrace ();
  return 0;

}


#endif /* MAIN */
