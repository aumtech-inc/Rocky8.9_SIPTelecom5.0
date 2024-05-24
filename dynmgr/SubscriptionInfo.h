#ifndef SUBSCRIPTION_INFO_DOT_H
#define SUBSCRIPTION_INFO_DOT_H

// this is pretty much just dynamic storage 
// much like the list of registrations we will 
// delete subscriptions that fail catostrophically 

struct SubscriptionInfo_t
{
 char *touri;
 char *fromuri;
 char *route;
 char *event;
 int expires;
 time_t last;
 char *callid;
 int zCall;
 int did;                       // exosip's dialog id  
 int state;
 int (*attachment) (void *);    // not used yet
 int (*cb)();  // generic callback routine 
 struct SubscriptionInfo_t *next;
};
typedef struct SubscriptionInfo_t SubscriptionInfo_t;

struct SubscriptionInfoList_t
{
 SubscriptionInfo_t *head;
 int count;
 pthread_mutex_t mutex;
};
typedef struct SubscriptionInfoList_t SubscriptionInfoList_t;

int SubscriptionInfoLoadConfig(char *gHostIp, unsigned int gSipPort, char *gIpCfg);

int SubscriptionInfoInit (SubscriptionInfoList_t * list);

SubscriptionInfo_t *SubscriptionInfoFindExpires (SubscriptionInfoList_t * list);

SubscriptionInfo_t *SubscriptionInfoFindByCallId (SubscriptionInfoList_t * list, char *callid);

int
SubscriptionInfoAdd (SubscriptionInfoList_t * list, char *touri,
                     char *fromuri, char *route, char *event, char *callid, int (*attachment) (void *), int (*cb) (), int zCall=-1);


int SubscriptionInfoUpdateExpiresByCallId (SubscriptionInfoList_t * list, char *callid, int expires);

int SubscriptionInfoUpdateLastSubscription (SubscriptionInfoList_t * list, char *callid);

int SubscriptionInfoUpdateDid (SubscriptionInfoList_t * list, char *callid, int did);

int SubscriptionInfoDeleteByCallId (SubscriptionInfoList_t * list, char *callid);

int SubscriptionInfoDeleteByzCall (SubscriptionInfoList_t * list, int zCall);

void SubscriptionInfoFree (SubscriptionInfo_t * elem);

void SubscriptionInfoListFree (SubscriptionInfoList_t * list);

int SubscriptionInfoUpdateCallId (SubscriptionInfoList_t * list, char *oldcallid, char *newcallid);


#endif /* SUBSCRIPTION_INFO_DOT_H */
