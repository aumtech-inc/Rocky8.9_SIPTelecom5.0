#ifndef REGISTRATION_INFO_T
#define REGISTRATION_INFO_T

#include <pthread.h>

struct RegistrationInfo_t
{
 char *username;
 char *id;
 char *password;
 char *realm;
 char *proxyuri;
 char *route;
 char *transport;
 int expires;
 int regid;
 int numtries;
 time_t last;
 int span;
 struct RegistrationInfo_t *next;
};
typedef struct RegistrationInfo_t RegistrationInfo_t;

struct RegistrationInfoList_t
{
 RegistrationInfo_t *head;
 RegistrationInfo_t *tail;
 pthread_mutex_t mutex;
 int count;
};
typedef struct RegistrationInfoList_t RegistrationInfoList_t;

int
RegistrationInfoLoadConfig (char *gHostIp, unsigned int gSipPort, int gDynMgrId, char *gIpCfg, char *gSipUserAgent);

int RegistrationInfoListInit (RegistrationInfoList_t * RegInfo);

int RegistrationInfoListAdd (RegistrationInfoList_t * list, char *username, char *id, char *password, char *realm, char *proxyuri, char *route, int expires,
                             char *transport, int regid, int span = -1);

int RegistrationInfoListDeleteById (RegistrationInfoList_t * list, int regid);

RegistrationInfo_t *RegistrationInfoListGetById (RegistrationInfoList_t * list, int regid);

int RegistrationInfoListUpdateExpiresById (RegistrationInfoList_t * list, int regid, int expires);

int RegistrationInfoListUpdateLastRegister (RegistrationInfoList_t * list, int regid);

RegistrationInfo_t *RegistrationInfoListFindExpires (RegistrationInfoList_t * list);

void RegistrationInfoListFree (RegistrationInfoList_t * list);

void RegistrationInfoFree (RegistrationInfo_t * list);

#endif
