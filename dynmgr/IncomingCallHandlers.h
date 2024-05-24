#ifndef  INCOMING_CALL_HANDLERS_DOT_H
#define INCOMING_CALL_HANDLERS_DOT_H

#include <eXosip2/eXosip.h>

int incomingCallWithSipSignaledDigits(osip_message_t *mesg, int zCall);

int incomingCallRedirectorHandler(eXosip_event_t * eXosipEvent);

int incomingCallMessageDispatch (eXosip_event_t * eXosipEvent, int zCall);

int incomingDTMFHandler (eXosip_event_t * eXosipEvent, int zcall);

int incomingNotifyHandler(eXosip_event_t *eXosipEvent, int zCall);

int dropSipSignaledDigitsSubscription(int zCall);

#endif 

