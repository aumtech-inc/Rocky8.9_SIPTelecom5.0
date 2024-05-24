#ifndef OUTBOUND_CALL_HANDLERS_DOT_H
#define OUTBOUND_CALL_HANDLERS_DOT_H

#include <stdio.h>
#include <stdlib.h>
#include <eXosip2/eXosip.h>

#define MAX_REDIRECTION_CONTACTS 8

enum OutboundReferOps_t {
  OUTBOUND_REFER_OP_INIT = (1 << 1),
  OUTBOUND_REFER_OP_SET_IDLE = (1 << 2)
};

int OutboundCallReferHandler(int zLine, eXosip_event_t *event, int zCall, char *newdest=NULL, char *from=NULL, int op=0, char *referTo=NULL);

int InboundCallReferHandler(eXosip_event_t *event, int zCall);

int OutboundRedirectionHandler(eXosip_event_t *event, int zCall, char*);

#endif


