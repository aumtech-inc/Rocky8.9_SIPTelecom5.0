#ifndef INBOUND_REGISTRATIONS_DOT_H
#define INBOUND_REGISTRATIONS_DOT_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define LOCAL_DYNMGR_NAME "ArcIpDynMgr-"

/*
[InboundRegistrationInfo0]
FromIP=
Username=
ID=
Password=
Interval=
*/

#define MAX_INBOUND_REGISTRATIONS 100


enum active_registration_types_e {
  ACTIVE_REGISTRATION_TYPE_REMOTE,
  ACTIVE_REGISTRATION_TYPE_LOCAL
};


struct ActiveInboundRegistrations_t
{
  char Username[80];
  time_t Last;
  int Interval;
//  struct timeb LastUsed;
  struct timespec LastUsed;
  char ipaddress[80];
  char port[80];
  char transport[80];
  pid_t pid;
  size_t NumUsed;
  int type;
  int dynMgrId;
  struct ActiveInboundRegistrations_t *next;
};

typedef struct ActiveInboundRegistrations_t ActiveInboundRegistrations_t;

struct InboundRegistrationInfo_t
{
  char FromIP[40];
  char Username[40];
  char ID[40];
  char Password[40];
  int interval;
  struct InboundRegistrationInfo_t *next;
};

typedef struct InboundRegistrationInfo_t InboundRegistrationInfo_t;

//
// this should only be loaded onece 
// items are read from .TEL.cfg and stored here until the application 
// ends, it probably won't need locks it's fairly static information 
//


int ActiveRegistrationGetCount (ActiveInboundRegistrations_t * list, int type);


void
ActiveRegistrationListDelete(ActiveInboundRegistrations_t *list);


void
dump_list (ActiveInboundRegistrations_t * list);


void
InboundRegistrationListFree(InboundRegistrationInfo_t *list);

int
InboundRegistrationListAdd (InboundRegistrationInfo_t ** list, char *fromip,
                            char *username, char *id, char *passwd,
                            int interval);

int
InboundRegistrationListFind(InboundRegistrationInfo_t *list, char *username,  char *fromip, char *id, char **passwd, int *interval);


int InboundRegistrationLoadConfig (InboundRegistrationInfo_t ** list, char *gIPCfg);

#define MAX_DIAL_PEERS 100

int ActiveRegistrationLoadDialPeers (ActiveInboundRegistrations_t ** list, char *gIPCfg);

int
ActiveRegistrationListAdd (ActiveInboundRegistrations_t ** list, int type,
                           char *username, int interval, pid_t pid, int dynMgrID, char *host, char *port);

int
ActiveRegistrationListGetContact (ActiveInboundRegistrations_t * list, int *type,
                                  char **ipaddress, char **port, size_t *count, int *dynMgrID);


int
ActiveRegistrationListDeleteExpired (ActiveInboundRegistrations_t ** list);


int
ActiveRegistrationDumpToFile(ActiveInboundRegistrations_t * list);

int
ActiveRegistrationReadFromFile(ActiveInboundRegistrations_t **list);


int
ActiveRegistrationDelete(ActiveInboundRegistrations_t ** list, char *name, char *host, char *port);


#endif