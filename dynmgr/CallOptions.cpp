#include <iostream>
#include <eXosip2/eXosip.h>

// #define IMPORT_GLOBAL_EXTERNS 
// #include "ArcSipCallMgr.h"

#include "Options.hxx"
#include "CallOptions.hxx"


///////////////////////////////////////////////////////
//
// This class processes call options, namely 
// plucking and enabling features based on the
// user agents headers and header params 
//
// Note that you need to use the methods from Options::
// to access the maps used for storage 
//
// Note that if you add data to this structure 
// you will have to lock it: multiple threads will 
// use this class. Since there are only functions 
// the data for now is safe 
//
///////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////
//
// This is largely an attempt t create RFC compliant
// responses outside of the stack 
//
///////////////////////////////////////////////////////

#define __CLASSNAME__ "CallOptions::"

//	#define IMPORT_GLOBAL_EXTERNS 
//	#include "ArcSipCallMgr.h"

#define DYN_BASE        1

#define REPORT_NORMAL   1
#define REPORT_VERBOSE  2
#define REPORT_CDR      16
#define REPORT_DIAGNOSE 64
#define REPORT_DETAIL   128

#define ERR 0
#define WARN 1
#define INFO 2

extern int gSipMinimumSessionExpires;
extern int dynVarLog (int zLine, int zCall, const char *zpMod, int mode, int msgId,
						 int msgType, const char *msgFormat, ...);

extern "C"
{
	#include <stdlib.h>
	#include <strings.h>
	#include <string.h>

    static void parseSessionExpires(char *zValue, int *zExpTime, char *zRefresher);

	extern CallOptions gCallInfo[];
}


CallOptions::CallOptions(){

   return;
}



void
CallOptions::Init(){
   this->Clear();
   return;
}



CallOptions::~CallOptions(){
   return;
}

int 
CallOptions::ProcessInvite(eXosip_event_t *event){

  int rc = 0;

  osip_message_t *inv = NULL;
  sdp_message_t *sdp = NULL;

  if(!event || !event->request){
    return -1;
  } else {
    inv = event->request;
  }

  // process the sip header list
  // from the generic headers

  osip_list_t *list = &inv->headers;
  osip_header_t *hdr = NULL;
  int pos;


  for(pos = 0; !osip_list_eol(list, pos); pos++){

      hdr = (osip_header_t *)osip_list_get(list, pos);

      // fprintf(stderr, "%s%s: header=%s : %s\n", __CLASSNAME__, __func__, hdr->hname, hdr->hvalue);

      if(!strcasecmp(hdr->hname, "session-expires")){
        this->AddOption("session-expires", hdr->hvalue, "", this->OPTION_INT);
      } else 
      if(!strcasecmp(hdr->hname, "requires")){
        // process individual requires tags
        ;;
      } else 
      if(!strcasecmp(hdr->hname, "supported")){ 
        // process individual supported tags 
        ;;
      }
       
  }

  // end sip header list
  // process from params

  return rc;
}



int 
CallOptions::ProcessInviteResponse(osip_message_t *response, int status){

  int rc = 0;
  string value = "";
  int val;

  if(!response)
    return -1;

  switch(status){
    case 200:
      // if session expires header then set refresher param to either UAC or UAS
      value = this->GetStringOption("session-expires");
      val = atoi(value.c_str());
      cerr << __func__ << "::" << val << "value=" << val << endl;
      rc = 200;
      break;
    case 422:
      // if session-expires header add Min-SE header using global minimum and send back
      char min[80];
      snprintf(min, sizeof(min), "%d",  gSipMinimumSessionExpires);
      osip_message_set_header (response, "Min-SE", min);
      rc = 422;
      break;
    default:
      // no status handler defined yet
      rc = -1;
      break;
  }
  return rc;
}

int CallOptions::ProcessSessionTimers(char *zMod, int zLine, int zCall, eXosip_event_t *evnt)
{
	static char mod[]="ProcessSessionTimers";
	int		rc;
	int		timerSupported=0;
	int		minSE = 0;
	int		se = -1;
	char	buf[32];

  char *sip_header_list[] =
  {
    "supported",
    "required",
    "min-se",
    "session-expires",
    "user-agent",
    "p-asserted-identity",
    NULL
  };

  osip_message_t *inv = NULL;
  sdp_message_t *sdp = NULL;
  char name[255];
  char value[255];
  char refresherVal[128];

	 dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,	
			"Called from [%s, %d]", zMod, zLine);
  if(!evnt || !evnt->request){
    return -1;
  } else {
    inv = evnt->request;
  }

  // process the sip header list 
  // from the generic headers 

  osip_list_t *list = &inv->headers;
  osip_header_t *hdr = NULL;
  int pos, i;

  for(pos = 0; !osip_list_eol(list, pos); pos++)
  {
      hdr = (osip_header_t *)osip_list_get(list, pos);

      for(i = 0; sip_header_list[i]; i++)
      {
//		 dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,	
//			"name:(%s)  value:(%s) sip_header_list[%d]:(%s)",
//			hdr->hname, hdr->hvalue, i, sip_header_list[i]);

         if(!strcasecmp(hdr->hname, sip_header_list[i]))
		 {
           snprintf(name, sizeof(name), "%s", sip_header_list[i]);
           snprintf(value, sizeof(value), "%s", hdr->hvalue);

			if ( !strcasecmp(hdr->hname, "session-expires") )
			{

				 dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,	
							"session-expires value:(%s)", value);

				memset((char *)refresherVal, '\0', sizeof(refresherVal));
				parseSessionExpires(value, &se, refresherVal);
//				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,	
//							"se:%d  refresherVal:(%s)", se, refresherVal);

				if ( se < gSipMinimumSessionExpires )
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, WARN, 
						"Session-Expires %d is less than mimimum of %d.  Returning failure.", 
						se, gSipMinimumSessionExpires);
					return(-1);
				}
				else
				{
					// We need to get the current value and add it
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
							"Got of session-expires: %d", se);
				}
			}
			else if ( !strcasecmp(hdr->hname, "min-se") )
			{
				if ( atoi(value) < gSipMinimumSessionExpires )
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, WARN, 
						"Min-Se %d is less than mimimum of %d.  Returning failure.", 
						atoi(value), gSipMinimumSessionExpires);
					return(-1);
				}
				else
				{
					// We need to get the current value and add it
					minSE = atoi(value);
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
							"Current value of min-se: %d", minSE);
				}
			}
			else if ( !strcasecmp(hdr->hname, "supported") )
			{
				if ( ! strcmp(value, "timer") ) 
				{
					timerSupported=1;
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
							"timer is supported:  timerSupported=%d", timerSupported);
				}
			}
         }
      }
  }

	if ( se <= 0 )
	{
		return(0);
	}

	sprintf(buf, "%d", timerSupported);
	this->AddOption("uac-timer-supported", buf, "", this->OPTION_INT);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
			"out of loop; se:%d minSE:%d, timerSupported=%d refresherVal:(%s)",
			se, minSE, timerSupported, refresherVal);
	
	sprintf(buf, "%d", se);
	this->AddOption("session-expires", buf, "", this->OPTION_INT);

	sprintf(buf, "%d", se/2);
	this->AddOption("refresh-time", buf, "", this->OPTION_INT);

	sprintf(buf, "%d", minSE);
	this->AddOption("min-se", buf, "", this->OPTION_INT);

//	rc= gCallInfo[zCall].GetIntOption("min-se");
//	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
//			"%d = gCallInfo[%d].GetIntOption(min-se)", rc, zCall);

	this->AddOption("refresher", refresherVal, "", this->OPTION_STRING);

  // end sip header list 
  // process from params 
  return(0);

} // END: CallOptions::ProcessSessionTimers()

int 
CallOptions::ProcessUpdateResponse(osip_message_t *response, int status){

  int rc = 0;

  // if the response form exosip does not have the 
  // proper headers populated put them in manually 
  // in the response 

  return rc;
}

/*------------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
static void parseSessionExpires(char *zValue, int *zExpTime, char *zRefresher)
{
	char		*p;
	char		name[64];
	char		value[64];
	char		workingVal[64];
	int			i;

	*zExpTime = -1;

	if ( ! zValue )
	{
		return;
	}
		
	if ((p = (char *)strchr(zValue, ';')) == (char *)NULL)
	{
		*zExpTime = atoi(zValue);
		return;
	}

	*p = '\0';
	*zExpTime = atoi(zValue);
	p++;

	memset((char *)workingVal, '\0', sizeof(workingVal));
	i = 0;
	while ( *p != '\0' )
	{
		if ( ! isspace(*p) )
		{
			workingVal[i] = *p;
			i++;
		}
		p++;
	}

	memset((char *)name, '\0', sizeof(name));
	memset((char *)value, '\0', sizeof(value));

	sscanf(workingVal, "%[^=]=%s", name, value);
	if ( strncmp(name, "refresher", 9) )
	{
		sprintf(zRefresher, "%s", "none");
		return;
	}
	
	if ( ( ! strcmp(value, "uac") ) || ( ! strcmp(value, "uas") ) )
	{
		sprintf(zRefresher, "%s", value);
		return;
	}

	sprintf(zRefresher, "%s", "none");
	return;

} // END: parseSessionExpires()



#ifdef MAIN 

int 
main(){

  CallOptions co;
  int i;
  char name[255];
  char value[255];
  char defaults[255] = "";

  co.Clear();

  for(i = 0; i < 255; i++){
     snprintf(name, sizeof(name), "test%d", i);
     snprintf(value, sizeof(value), "%d", i);
     co.AddOption(name, value, defaults, co.OPTION_INT);
  }

  for(i = 0; i < 255; i++){
     snprintf(name, sizeof(name), "test%d", i);
     int rc = co.GetIntOption(name);
     cerr << "value=" << rc << endl;
  }

  co.ProcessInvite(NULL);
  co.ProcessInviteResponse(NULL, 422);
  co.Clear();
  return 0;
}

#endif 

