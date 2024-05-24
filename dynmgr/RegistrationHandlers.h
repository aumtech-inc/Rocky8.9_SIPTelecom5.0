#ifndef REGISTRATION_HANDLERS_DOT_H
#define REGISTRATION_HANDLERS_DOT_H

#include <eXosip2/eXosip.h>

/* RegistrationHandlers.c */

int incomingRegistrationHandler(eXosip_event_t *evnt);

int updateRegistrationHandler(int regid, int interval);

int newRegistrationHandler(char *fromUri, char *proxyUri, char *contactUri, char *supported, char *route, int interval);

int successfulRegistrationHandler(eXosip_event_t *eXosipEvent);

int unsuccessfulRegistrationHandler(eXosip_event_t *eXosipEvent);

int deRegistrationHandler(RegistrationInfoList_t *RegistrationInfoList);


#endif
