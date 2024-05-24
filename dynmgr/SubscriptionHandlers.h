#ifndef SUNSCRIPTION_HANDLERS_DOT_H
#define SUNSCRIPTION_HANDLERS_DOT_H

#include <eXosip2/eXosip.h>


/* SubscriptionHandlers.c */


int newSubscriptionHandler(char *touri, char *fromuri, char *route, char *event, int expires, int zCall=-1);
int successfulSubscriptionHandler(eXosip_event_t *eXosipEvent);
int unsuccessfulSubscriptionHandler(eXosip_event_t *eXosipEvent);
int deletedSubscriptionHandler(eXosip_event_t *eXosipEvent);
int incomingNotifyHandler(eXosip_event_t *eXosipEvent);
int updateSubscriptionHandler(char *callid);
int incomingSubscriptionHandler(eXosip_event_t *eXosipEvent);
int incomingInfoHandler(eXosip_event_t *eXosipEvent, int zcall);
int newOptionsHandler(eXosip_event_t *eXosipEvent);
int newOptionRequestHandler(eXosip_event_t *eXosipEvent);

#endif


