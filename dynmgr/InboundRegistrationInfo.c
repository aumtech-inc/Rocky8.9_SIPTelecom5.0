#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#include "InboundRegistrationInfo.h"

static pthread_mutex_t InboundLock = PTHREAD_MUTEX_INITIALIZER;

#define MAX_INBOUND_REGISTRATIONS 100


#define FREE(ptr) do { free(ptr); ptr = NULL; } while(0)

extern int get_name_value (char *section, char *name, char *defaultVal, char *value,
        int bufSize, char *file, char *err_msg);

static int
test_pid (char *progname, pid_t pid)
{

 int rc = -1;
 char pathname[1024];
 char buf[1024];
 FILE *fp;

 if (progname && pid) {

  snprintf (pathname, sizeof (pathname), "/proc/%d/cmdline", (int) pid);
  // fprintf(stderr, "%s\n", pathname);
  if ((fp = fopen (pathname, "r")) != NULL) {
   fread (buf, sizeof (buf), 1, fp);
   if (strstr (buf, progname)) {
    // fprintf(stderr, "%s\n", buf);
    rc = 0;
   }
   fclose(fp);
  }
 }
 return rc;
}

// added these to resolve a 
// very obscure core dump  


static void 
InboundRegistrationListLock(){
  pthread_mutex_lock(&InboundLock);
}

static void 
InboundRegistrationListUnLock(){
  pthread_mutex_unlock(&InboundLock);
}

// now the list routines lock 
// the list before modification 
//





//
// this should only be loaded once 
// items are read from .TEL.cfg and stored here until the application 
// ends, it probably won't need locks since it's fairly static information 
// and for now the body of the redirector code is single threaded
//


int
InboundRegistrationListAdd (InboundRegistrationInfo_t ** list, char *fromip,
                            char *username, char *id, char *passwd,
                            int interval)
{

  int rc = -1;
  InboundRegistrationInfo_t *elem = NULL;
  InboundRegistrationInfo_t *next = NULL;

  elem =
    (InboundRegistrationInfo_t *) calloc (1, sizeof (InboundRegistrationInfo_t));

  InboundRegistrationListLock();

  if (elem) {
    fromip ? snprintf (elem->FromIP, sizeof (elem->FromIP), "%s", fromip) : 0;
    username ? snprintf (elem->Username, sizeof (elem->Username), "%s",
                         username) : 0;
    id ? snprintf (elem->ID, sizeof (elem->ID), "%s", id) : 0;
    passwd ? snprintf (elem->Password, sizeof (elem->Password), "%s",
                       passwd) : 0;
    elem->interval = interval;
    if (*list == NULL) {
      *list = elem;
    }
    else {
      next = *list;
      elem->next = next;
      *list = elem;
    }
  }

#if 0
  if (*list == NULL) {
    *list = elem;
  }
#endif 

  InboundRegistrationListUnLock();
  return rc;
}

void 
InboundRegistrationListFree(InboundRegistrationInfo_t *list){

  InboundRegistrationInfo_t *elem;
  InboundRegistrationInfo_t *next;

  InboundRegistrationListLock();
  if(list){
    elem = list;

    while(elem){
     next = elem->next;
     FREE(elem);
     elem = next;
   }
  }

  InboundRegistrationListUnLock();
  return;
}


int
InboundRegistrationListFind (InboundRegistrationInfo_t * list, char *username,
                             char *fromip, char *id, char **passwd,
                             int *interval)
{

  int rc = -1;

  if(username == NULL|| fromip == NULL){
    return -1;
  }

  InboundRegistrationInfo_t *elem = NULL;

  InboundRegistrationListLock();
  if (list) {
    while (list) {
      if ((!strcmp (list->Username, username))
          && (!strcmp (list->FromIP, fromip))) {
        if (passwd && list->Password) {
          *passwd = strdup (list->Password);
        }
        if (interval) {
          *interval = list->interval;
        }
        rc = 0;
        break;
      }
      list = list->next;
    }
  }

  InboundRegistrationListUnLock();
  return rc;
}


int
InboundRegistrationLoadConfig (InboundRegistrationInfo_t ** list,
                               char *gIPCfg)
{

  int rc = -1;
  int i, j;

  char section[80];
  char errmsg[100];

  char *config_names[] = {
    "FromIP",
    "Username",
    "ID",
    "Password",
    "Interval",
    NULL
  };

  enum config_indexes
  {
    CONFIG_FROM_IP,
    CONFIG_USERNAME,
    CONFIG_ID,
    CONFIG_PASSWORD,
    CONFIG_INTERVAL,
    //
    CONFIG_END
  };

  char config_values[CONFIG_END][80];

  for (i = 0; i < MAX_INBOUND_REGISTRATIONS; i++) {

    snprintf (section, sizeof (section), "%s%d", "InboundRegistrationInfo",
              i);
    memset (config_values, 0, sizeof (config_values));

    for (j = 0; j < CONFIG_END; j++) {

      get_name_value (section, config_names[j], "", config_values[j],
                      sizeof (config_values[j]), gIPCfg, errmsg);

      if (
		     config_values[CONFIG_FROM_IP][0]
		  && config_values[CONFIG_USERNAME][0]
          && config_values[CONFIG_ID][0]
          && config_values[CONFIG_PASSWORD][0]
          && config_values[CONFIG_INTERVAL][0]
		 ){
        InboundRegistrationListAdd (list, config_values[CONFIG_FROM_IP],
                                    config_values[CONFIG_USERNAME],
                                    config_values[CONFIG_ID],
                                    config_values[CONFIG_PASSWORD],
                                    atoi (config_values[CONFIG_INTERVAL]));
      }
    }
  }

  return rc;
}


//  remember to only load these into the active registration list
//


#define MAX_DIAL_PEERS 100

int
ActiveRegistrationLoadDialPeers (ActiveInboundRegistrations_t ** list,
                                 char *gIPCfg)
{

  char section[80];
  int i, j, rc = -1;

  char errmsg[256];
  char contact[80];


  char *config_names[] = {
    "IPAddress",
    "Port",
    "Transport",
    NULL
  };

  char *config_defaults[] = {
    "",
    "5060",
    "udp",
    NULL
  };

  enum config_indexes_e
  {
    CONFIG_IPADDRESS,
    CONFIG_PORT,
    CONFIG_TRANSPORT,
    // 
    CONFIG_END
  };

  char config_values[CONFIG_END][80];

  for (i = 0; i < MAX_DIAL_PEERS; i++) {

    snprintf (section, sizeof (section), "DialPeer%d", i);
    memset (config_values, 0, sizeof (config_values));

    for (j = 0; config_names[j]; j++) {
      get_name_value (section, config_names[j], config_defaults[j],
                      config_values[j], sizeof (config_values[j]), gIPCfg,
                      errmsg);
    }

    if (config_values[CONFIG_IPADDRESS][0] && config_values[CONFIG_PORT][0]) {
      ActiveRegistrationListAdd (list, ACTIVE_REGISTRATION_TYPE_REMOTE, section, -1, -1, -1,
                                 config_values[CONFIG_IPADDRESS],
                                 config_values[CONFIG_PORT]);
    }
  }

	return(0);
}


//struct ActiveInboundRegistrations_t
//{
//  char Username[80];
//  time_t Last;
//  int Interval;
//  struct timeb LastUsed;
//  char ipaddress[80];
//  char port[80];
//  char transport[80];
//  pid_t pid;
//  struct ActiveInboundRegistrations_t *next;
//};





void 
ActiveRegistrationListDelete(ActiveInboundRegistrations_t *list){

  ActiveInboundRegistrations_t *elem;
  ActiveInboundRegistrations_t *next;

  if(list){
    elem = list;

    while(elem){
      next = elem->next;
      // free
      FREE(elem);
      elem = next;
    }
  }

  return;
}



int
ActiveRegistrationListAdd (ActiveInboundRegistrations_t ** list, int type,
                           char *username, int interval, pid_t pid, int dynMgrID,
                           char *host, char *port)
{

  int rc = -1;

  ActiveInboundRegistrations_t *elem = NULL;
  ActiveInboundRegistrations_t *listptr = NULL;

  elem =
    (ActiveInboundRegistrations_t *) calloc (1,
                                             sizeof
                                             (ActiveInboundRegistrations_t));


  if (elem) {
    time (&elem->Last);
    snprintf (elem->Username, sizeof (elem->Username), "%s", username);
    snprintf (elem->ipaddress, sizeof (elem->ipaddress), "%s", host);
    if (port) {
      snprintf (elem->port, sizeof (elem->port), "%s", port);
    }
    else {
      snprintf (elem->port, sizeof (elem->port), "5060");
    } 
    //ftime (&elem->LastUsed);
	clock_gettime(CLOCK_REALTIME, &elem->LastUsed);
    elem->pid = pid;
    elem->Interval = interval;
    elem->type = type;
    elem->dynMgrId = dynMgrID;
  }
  else {
    return -1;
  }


  if (*list == NULL) {
    *list = elem;
  }
  else {

    listptr = *list;

    while (listptr) {
      // if we find a Username/ip/port match just update the timestamp 
      // i am doing this as nested if's just as a test 
      // just in case I need to modify the tests for a match 
      if (strcmp (listptr->Username, username) == 0) {
        if (strcmp (listptr->ipaddress, host) == 0) {
          if (port && (strcmp (listptr->port, port) == 0)) {
            time (&listptr->Last);
            listptr->pid = pid;
            rc = 0;
            // we don't need the new element 
            FREE (elem);
            break;
          }
        }
      }

      if (listptr->next == NULL) {
        // else add it
        listptr->next = elem;
        rc = 0;
        break;
      }

      listptr = listptr->next;
    }                           // end while 
  }

  return rc;
}

void
ActiveRegistrationsDumpList (ActiveInboundRegistrations_t * list)
{

  int count = 0;
  pid_t pid = getpid ();

  if (list) {
    ActiveInboundRegistrations_t *elem = list;

    while (elem) {
      fprintf (stderr, "\n\nthis=%ppid=%dnum=%d\n", elem, pid, count++);
      fprintf (stderr, "username=%s\n", elem->Username);
      fprintf (stderr, "Last = %d\n", elem->Last);
      fprintf (stderr, "Interval=%d\n", elem->Interval);
      fprintf (stderr, "LastUsed=%d\n", elem->LastUsed);
      fprintf (stderr, "ipaddress=%s\n", elem->ipaddress);
      fprintf (stderr, "port=%s\n", elem->port);
      fprintf (stderr, "next=%p\n\n\n", elem->next);
      elem = elem->next;
    }
  }
  return;
}

#define REG_PICKLE "/tmp/redirector.pickle"

int 
ActiveRegistrationDumpToFile(ActiveInboundRegistrations_t * list){

  int rc = -1;
  char line[1024];
  FILE *pickle = NULL;

  ActiveInboundRegistrations_t *elem = NULL;

  if(!list){
	  unlink(REG_PICKLE);
      return -1;
  }

  pickle = fopen(REG_PICKLE, "w+");

  if(!pickle){
     return -1;
  }

  elem = list;

  while(elem){
     if(elem->type != ACTIVE_REGISTRATION_TYPE_REMOTE){
        elem = elem->next;
        continue;
     }
     snprintf(line, sizeof(line), "%s|%d|%d|%d|%s|%s|%d", elem->Username, 0, elem->Interval, 0, elem->ipaddress, elem->port, elem->type);
     fprintf(pickle, "%s\n", line);
     elem = elem->next;
  }

  fclose(pickle);
  return rc;
}



int 
ActiveRegistrationReadFromFile(ActiveInboundRegistrations_t **list){

   int rc = -1;
   FILE *pickle = NULL;
   char line[1024];
   ActiveInboundRegistrations_t *elem = NULL;
   ActiveInboundRegistrations_t *next = NULL;

   enum state_e {
     USERNAME = 0, 
     LAST_TIME, 
     INTERVAL, 
     LAST_USED, 
     IPADDRESS, 
     PORT,
     TYPE,
     START, 
     STOP, 
     OK,
     ERROR 
   };

   char username[40];
   char last_time[40];
   char interval[40];
   char last_used[40];
   char ip_address[40];
   char port[10];
   char type[4];


   int len;
   char *ptr;
   int state;

   if(access(REG_PICKLE, F_OK) != 0){
      return -1;
   }

   pickle = fopen(REG_PICKLE, "r");

   if(!pickle){
      return -1;
   }

#define GETFIELD(ptr, sep, dest, next) \
do {\
  len = strcspn(ptr, sep); \
  if(len){\
     snprintf(dest, len + 1, "%s", ptr);\
     ptr += len + 1;\
     state = next;\
  } else {\
     state = ERROR;\
  }\
}\
while(0)\
\
    
   while(fgets(line, sizeof(line), pickle)){

       ptr = line;
       state = START;

       // fprintf(stderr, " line=%s", line);

       while(state != STOP){
           switch(state){
              case START:
                  state = USERNAME;
                  break;
              case USERNAME:
                 GETFIELD(ptr, "|", username, LAST_TIME);
                  break;
              case LAST_TIME:
                 GETFIELD(ptr, "|", last_time, INTERVAL);
                 break;
              case INTERVAL:
				 GETFIELD(ptr, "|", interval, LAST_USED);
                 break;
              case LAST_USED:
                 GETFIELD(ptr, "|", last_used, IPADDRESS);
                 break;
              case IPADDRESS:
                 GETFIELD(ptr, "|", ip_address, PORT);
                 break;
              case PORT:
                 GETFIELD(ptr, "|", port, TYPE);
                 break;
              case TYPE:
				 GETFIELD(ptr, "\n", type, OK);
                 break;
              case OK:

                // fprintf(stderr, " %s() read from pickle user=%s ip=%s, port=%s\n", __func__, username, ip_address, port);

                 elem = (ActiveInboundRegistrations_t *)calloc(1, sizeof(ActiveInboundRegistrations_t));
                 if(elem){
                   snprintf(elem->Username, sizeof(elem->Username), "%s", username);
                   elem->Interval = atoi(interval);
                   snprintf(elem->ipaddress, sizeof(elem->ipaddress), "%s", ip_address);
                   snprintf(elem->port, sizeof(elem->port), "%s", port);
                   elem->type = atoi(type);
                   //
                   ActiveRegistrationListAdd (list, ACTIVE_REGISTRATION_TYPE_REMOTE,
                           username, 120, -1, -1,
                           ip_address, port);

#if 0                   
                   if(list){
                     next = *list;
                     *list = elem;
                     elem->next = next;
                   } else {
                     *list = elem;
                   }
#endif
                 }

                 state = STOP;
                 break;
              case ERROR:
                 state = STOP;
                 break;
                 
	          default:
				 fprintf(stderr, " %s() should not hit here !\n", __func__);
                 break;

           }
       }
   }

#undef GETFIELD

   fclose(pickle);
   
   return rc;
}


int
ActiveRegistrationGetCount (ActiveInboundRegistrations_t * list, int type){

   int rc = 0;

  ActiveInboundRegistrations_t *elem = NULL;

  if(list){
     elem = list;
     while (elem) {
      if(elem->type == type){
        rc++;
      }
      elem = elem->next;
    }

   }

   return rc;
}





//
// I am pulling these into an array
// for now, sorting based on last used time.
//
// remember to timestamp the returned contact element
//
// joes
//


int
ActiveRegistrationListGetContact (ActiveInboundRegistrations_t * list, int *type, char **ipaddress, char **port, size_t *refcount, int *dynMgrId)
{

  int rc = -1;
  ActiveInboundRegistrations_t *elem = NULL;
	struct timespec	now;
	struct timespec	sav;
//  struct timeb now;
//	struct timeb sav;
  int index = 0;
  int count = 0;

  ActiveInboundRegistrations_t *ptrs[MAX_INBOUND_REGISTRATIONS + MAX_DIAL_PEERS + 1];
  int i;


  if (list) {

    memset (ptrs, 0, sizeof (ptrs));
    elem = list;

    while (elem) {
      ptrs[count] = elem;
      count++;
      elem = elem->next;
    }

//fprintf(stderr, " %s() active list count=%d\n", __func__, count);

//    ftime (&now);
	clock_gettime(CLOCK_REALTIME, &now);
    memcpy(&sav, &now, sizeof(struct timespec));

    for (i = 0; ptrs[i]; i++) {

      // is it a local dynmgr ?
      // then check if it is running

      if (ptrs[i]->pid != -1){
        if(test_pid("ArcIPDynMgr", ptrs[i]->pid) != 0){
           continue;
        }
      }

      if (ptrs[i]->LastUsed.tv_sec < now.tv_sec) {
        now.tv_sec = ptrs[i]->LastUsed.tv_sec;
        now.tv_nsec = ptrs[i]->LastUsed.tv_nsec;
        index = i;
      }
      else if (ptrs[i]->LastUsed.tv_sec == now.tv_sec) {
        if (ptrs[i]->LastUsed.tv_nsec <= now.tv_nsec) {
          now.tv_nsec = ptrs[i]->LastUsed.tv_nsec;
          index = i;
        }
      }
    }

    if (ipaddress && ptrs[index]->ipaddress[0]) {
      *ipaddress = strdup (ptrs[index]->ipaddress);
       ptrs[index]->NumUsed++;
       refcount ? *refcount = ptrs[index]->NumUsed : 0;
       type ? *type = ptrs[index]->type : 0;
       dynMgrId ? *dynMgrId = ptrs[index]->dynMgrId : 0;
    }

    if (port && ptrs[index]->port[0]) {
      *port = strdup (ptrs[index]->port);
    }

    memcpy(&ptrs[index]->LastUsed, &sav, sizeof(struct timespec));
  }


  return rc;
}


#define FUDGE_FACTOR 10

int
ActiveRegistrationListDeleteExpired (ActiveInboundRegistrations_t **list)
{

  int rc = -1;
  time_t now;

  ActiveInboundRegistrations_t *elem = NULL;
  ActiveInboundRegistrations_t *prev = NULL;
  ActiveInboundRegistrations_t *sav = NULL;

  time (&now);

  if (*list) {


    elem = *list;

    while (elem) {

      if ((elem->Interval != -1) && ((elem->Last + elem->Interval + FUDGE_FACTOR) < now)) {
        sav = elem;
        if (prev) {
          elem = prev->next = elem->next;
        }
        else {
          elem = *list = elem->next;
        }
        FREE(sav);
        continue;
      } else { 
         // 
         // it is a local DynMgr  ? 
         // local dynmgrs always have the format ArcIPDynMgr-%pid so just 
         // check for the string - God forbid someone uses that as a username 
         // 
        if((strstr(elem->Username, LOCAL_DYNMGR_NAME)) && (test_pid("ArcIPDynMgr", elem->pid) != 0)){
           // delete this element 
//fprintf(stderr, " %s() deleting local dynmgr from active registrations\n", __func__);
           sav = elem;
           if(prev){
              prev->next = elem->next;
              elem = prev->next;
           } else {
              *list = elem->next;
              elem = elem->next;
           }
           FREE(sav);
           continue;
        }
      }
      prev = elem;
      elem = elem->next;
    }
  }
  return rc;
}

int
ActiveRegistrationDelete (ActiveInboundRegistrations_t ** list, char *name,
                          char *host, char *port)
{

  int rc = -1;

  ActiveInboundRegistrations_t *elem = NULL;
  ActiveInboundRegistrations_t *prev = NULL;
  ActiveInboundRegistrations_t *sav = NULL;

  if (list && *list) {

    //fprintf(stderr, "line=%d list=%p\n", __LINE__, *list);

    elem = *list;

    while (elem) {
      if (!strcmp (name, elem->Username)) {
        if (!strcmp (host, elem->ipaddress)) {
          if (port && elem->port && (!strcmp (port, elem->port))) {
            sav = elem;
            if (prev) {
              prev->next = elem->next;
              elem = elem->next;
              FREE(sav);
            }
            else {
              *list = elem->next;
              FREE(sav);
              elem = elem->next;
            }
            rc = 0;
            break;
          }
        }
      }
      prev = elem;
      elem = elem->next;
    }

  }

  //fprintf(stderr, "line=%d list=%p\n", __LINE__, *list);

  return rc;
}


#ifdef MAIN

int
main ()
{

  InboundRegistrationInfo_t *list = NULL;
  ActiveInboundRegistrations_t *otherlist = NULL;

  InboundRegistrationLoadConfig (&list);
  ActiveRegistrationLoadDialPeers (&otherlist);

  char contact[265];

  while (1) {
    ActiveRegistrationListGetContact (otherlist, contact, sizeof (contact));
    printf ("contact = %s\n", contact);
    sleep (1);
  }

  return 0;
}

#endif