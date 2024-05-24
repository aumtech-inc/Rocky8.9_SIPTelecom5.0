/*------------------------------------------------------------------------------
Program Name:	ArcSipRedirector
File Name:		ArcSipRedirector.c
Purpose:  		SIP/2.0 Call Manager
Author:			Deep Narsay, Aumtech, Inc.
------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
Update: 07/07/2004 DDN Created the file
Update: 10/05/2005 SS Added ability to have x instances of Call Manager
Update: 10/24/2005 SS Changed no DynMgr found code from 404 to 486
Update: 06/30/2011 djb MR 3371.  Updated logging messages.
------------------------------------------------------------------------------*/


/*	Note: Following rules should be followed while coding SIP SDM.
 *
 *	1:  Every routine must include zCall and mod as its arguments. 
 *								(MACROS wont work otherwise)
 */

#define EXOSIP_EVENT_DEBUG(name) (fprintf(stderr, " EXOSIP_EVENT name=%s value=%d\n", #name, name))
#define PID_FILE_PATH "/tmp/ArcSipRedirector.pid"

#include <mcheck.h>
#include <dynVarLog.h> 


#include "RegistrationInfo.h"
#include "RegistrationHandlers.h"
#include "SubscriptionInfo.h"
#include "SubscriptionHandlers.h"
#include "InboundRegistrationInfo.h"
#include "InboundRegistrationHandlers.h"
#include "IncomingCallHandlers.h"
#include "CallRedirection.h"
#include "write_pid_file.h"
#include "dl_open.h"


#include "regex_redirection.h"


#define IMPORT_GLOBALS
#include "ArcSipCallMgr.h"
#include "ArcSipRedirector.h"
#include "SipInit.h"

#define REDIRECTOR_TABLE "RedirectorTab"
#include "jsprintf.h"
#define JS_DEBUG 0


arc_sip_redirection_t sip_redirection_table[SIP_REDIRECTION_TABLE_SIZE];

struct dynMgrStatus gDynMgrList[MAX_REDIRECTS];
	
pthread_attr_t gDMReqReaderThreadAttr;
pthread_t gDMReqReaderThreadId;

char	gLocalIpAddress[256];
char	gRedirectFifo[256];
int		gRedirectFifoFd = -1;
int		gCanReadRedirectFifo = 0;
int		gTotalRegisterCount = 0;

int gCanReadSipEvents = 0;


pthread_mutex_t gActiveRegistrationLock = PTHREAD_MUTEX_INITIALIZER;

#define ACTIVE_REGISTRATION_LOCK pthread_mutex_lock(&gActiveRegistrationLock)
#define ACTIVE_REGISTRATION_UNLOCK pthread_mutex_unlock(&gActiveRegistrationLock)

RegistrationInfoList_t RegistrationInfoList;
SubscriptionInfoList_t SubscriptionInfoList;

InboundRegistrationInfo_t *InboundRegistrationList = NULL;
ActiveInboundRegistrations_t *ActiveRegistrationList = NULL;

#ifdef __DEBUG__
#define __DDNDEBUG__(module, xStr, yStr, zInt) arc_print_port_info(mod, module, __LINE__, zCall, xStr, yStr, zInt)
#else
#define __DDNDEBUG__(module, xStr, yStr, zInt) ;
#endif


//
// fixup contacts for problem with stack 
// should continue to research the problem 
// with the stack to come up with a better fix --joes
//

int
arc_fixup_contact_header (osip_message_t * msg, char *ip, int port, int zCall)
{

	char            uri[512];
	osip_uri_t     *orig = NULL;
	osip_contact_t *contact;
	osip_uri_param_t *param;

	enum transport_mode_e
	{
		UDP,
		TCP,
		TLS
	} mode;

	if (strcmp (gHostTransport, "udp") == 0)
	{
		mode = UDP;
	}
	else if (strcmp (gHostTransport, "tcp") == 0)
	{
		mode = TCP;
	}
	else if (strcmp (gHostTransport, "tls") == 0)
	{
		mode = TLS;
	}

	if (msg)
	{
	char           *tmp = NULL;

	osip_list_t    *list = &msg->contacts;

		// fprintf(stderr, " %s() msg->contacts=%p\n", __func__, &msg->contacts);
		if (list != NULL)
		{
			contact = (osip_contact_t *) osip_list_get (list, 0);
			// fprintf(stderr, " %s() contact=%p\n", __func__, contact);
			if (contact)
			{
				int	rc;

				orig = osip_contact_get_url (contact);
				if (orig)
				{
					rc = osip_uri_to_str (orig, &tmp);
					//fprintf(stderr, " %s() rc = %d\n", __func__, rc);
					switch (mode)
					{
					case UDP:
						if (gEnableIPv6Support != 0)
						{
							if (ip && port)
							{
								snprintf (uri, sizeof (uri),
										  "<sip:%s@[%s]:%d>",
										  gCall[zCall].dnis, ip, port);
							}
							else
							{
								snprintf (uri, sizeof (uri), "<sip:%s@[%s]>",
										  gCall[zCall].dnis, ip);
							}
						}
						else
						{
							if (ip && port)
							{
								snprintf (uri, sizeof (uri), "<sip:%s@%s:%d>",
										  gCall[zCall].dnis, ip, port);
							}
							else
							{
								snprintf (uri, sizeof (uri), "<sip:%s@%s>",
										  gCall[zCall].dnis, ip);
							}
						}
						osip_list_special_free ((osip_list_t *) & msg->
												contacts,
												(void (*)(void *))
												osip_contact_free);
												//osip_contact_free(contact));
// MR 4600 - TLS								(void *(*)(void *))
// MR 4600 - TLS 								osip_contact_free);
						osip_message_set_contact (msg, uri);
						break;

					case TCP:
							if (ip && port)
							{
								snprintf (uri, sizeof (uri),
										  "<sip:%s@[%s]:%d>",
										  gCall[zCall].dnis, ip, port);
							}
							else
							{
								snprintf (uri, sizeof (uri), "<sip:%s@[%s]>",
										  gCall[zCall].dnis, ip);
							}


						osip_list_special_free ((osip_list_t *) & msg->
												contacts,
												(void (*)(void *))
												osip_contact_free);
						osip_message_set_contact (msg, uri);


						osip_uri_uparam_get_byname (orig, "transport", &param);

						if (param)
						{
							if (param->gvalue)
							{
								osip_free (param->gvalue);
								param->gvalue = strdup ("tcp");
							}
						}
						else
						{
							osip_uri_uparam_add (orig,
												 osip_strdup ("transport"),
												 osip_strdup ("tcp"));
						}
						break;
					case TLS:
						osip_uri_uparam_get_byname (orig, "transport",
													&param);
						if (param)
						{
							if (param->gvalue)
							{
								osip_free (param->gvalue);
								param->gvalue = strdup ("tls");
							}
						}
						else
						{
							osip_uri_uparam_add (orig,
												 osip_strdup ("transport"),
												 osip_strdup ("tls"));
						}
						break;
					default:
						dynVarLog (__LINE__, port, (char *)__func__, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
								 " %s() hit default case on line=%d, not good",
								 __func__, __LINE__);
						break;
					}
					if (tmp)
					{
						// fprintf(stderr, " %s() original contact uri = %s rc=%d\n", __func__, tmp, rc);
						free (tmp);
					}
				}
			}
		}
	}

#if 0							// old fixup

	if (gEnableIPv6Support != 0)
	{
		if (ip && port)
		{
			snprintf (uri, sizeof (uri), "<sip:%s@[%s]:%d>",
					  gCall[zCall].dnis, ip, port);
		}
		else
		{
			snprintf (uri, sizeof (uri), "<sip:%s@[%s]>", gCall[zCall].dnis,
					  ip);
		}
	}
	else
	{
		if (ip && port)
		{
			snprintf (uri, sizeof (uri), "<sip:%s@%s:%d>", gCall[zCall].dnis,
					  ip, port);
		}
		else
		{
			snprintf (uri, sizeof (uri), "<sip:%s@%s>", gCall[zCall].dnis,
					  ip);
		}
	}

	if (strcmp (gHostTransport, "tcp") == 0)
	{
	int             len = strlen (uri);

		snprintf (&uri[len - 1], sizeof (uri) - len, ";%s>", "transport=tcp");
	}
	else if (strcmp (gHostTransport, "tls") == 0)
	{
	int             len = strlen (uri);

		snprintf (&uri[len - 1], sizeof (uri) - len, ";%s>", "transport=tls");
	}
	else
	{
		;;
	}

	if (msg && (!strcmp (gHostTransport, "udp")))
	{
		osip_list_special_free ((osip_list_t *) & msg->contacts,
								(void *(*)(void *)) osip_contact_free);
		osip_message_set_contact (msg, uri);
	}

#endif

	return(0);

}/*END: arc_fixup_contact_header */


/*print debug info viz. function name, line number, port num. time*/

///Prints debug info and contains the function that called it, the line, the port, and the time into nohup.out.
void arc_print_port_info ( const char *function, int module, int zLine, 
							int zCall, const char *zData1, const char *zData2, int zData3)
{
	//struct timeb tb;
	struct timespec	tb;
	char t[100];
	char *tmpTime;

	if (!isModuleEnabled (module))
	{
		return;
	}

	//ftime (&tb);
	clock_gettime(CLOCK_REALTIME, &tb);

	tmpTime = ctime (&(tb.tv_sec));

	sprintf (t, "%s", tmpTime);

	t[strlen (t) - 1] = '\0';

	if (zCall < gStartPort || zCall > gEndPort)
	{
		printf ("ARCDEBUG:%d:%s:%d:State_%d:%s:%s:%d:%s:%d:%s:%s:%d\n",
			gDynMgrId,
			gDebugModules[module].label, zCall, -1, __FILE__,
			function, zLine, t, tb.tv_nsec, zData1, zData2, zData3);
	}
	else
	{
		printf ("ARCDEBUG:%d:%s:%d:State_%d:%s:%s:%d:%s:%d:%s:%s:%d\n",
			gDynMgrId,
			gDebugModules[module].label, zCall,
			gCall[zCall].callState, __FILE__, function, zLine, t,
			tb.tv_nsec, zData1, zData2, zData3);
	}

	fflush (stdout);

}/*END: void arc_printf */

///This function prints out log messages into the /home/arc/.ISP/Log/ISP.cur file.

#if 0
int dynVarLog (int zLine, int zCall, char *zpMod, int mode, int msgId, 
	int msgType, char *msgFormat, ...)
{
	va_list ap;
	char m[1024];
	char m1[512];
	char type[32];
	char lPortStr[10];

	switch (msgType)
	{

		case 0:
			strcpy (type, "ERR: ");
			break;

		case 1:
			strcpy (type, "WRN: ");
			break;

		case 2:
			strcpy (type, "INF: ");
			break;

		default:
			strcpy (type, "INF: ");
			break;
	}

	memset (m1, '\0', sizeof (m1));
	va_start (ap, msgFormat);
	vsprintf (m1, msgFormat, ap);
	va_end (ap);

	if(mode == REPORT_CDR)
	{
		sprintf(m, "%s", m1);
	}
	else
	{
		sprintf (m, "%s[%d]%s", type, zLine, m1);
	}

	sprintf (lPortStr, "%d", zCall);

	LogARCMsg (zpMod, mode, lPortStr, "DYN", "ArcSipRedirector", msgId, m);

	return (0);

}/*END: int dynVarLog */
#endif


///This is a callback function that the system uses to let ArcSipRedirector's threads know it has exited.
void sigterm (int x)
{
	const char *mod = "sigterm";
	int zCall = -1;

#ifndef REG_PICKLE
#define REG_PICKLE "/tmp/redirector.pickle"
#endif

	gCanReadRedirectFifo = 0;
	gCanReadSipEvents = 0;

	unlink(REG_PICKLE);

__DDNDEBUG__ (DEBUG_MODULE_ALWAYS, "", "", 0);

	return;
}

void sigsegv (int x)
{
    const char *mod = "sigsegv";
    int zCall = -1;

    gCanReadRedirectFifo = 0;
    gCanReadSipEvents = 0;

__DDNDEBUG__ (DEBUG_MODULE_ALWAYS, "", "", 0);

    return;
}





///This is a callback function that the system uses to let ArcSipRedirector's threads know it has exited.
void sigpipe (int x)
{
	const char *mod = "sigpipe";
	int zCall = -1;

	gCanReadRedirectFifo = 0;
	gCanReadSipEvents = 0;

__DDNDEBUG__ (DEBUG_MODULE_ALWAYS, "", "", 0);

	return;
}



void 
sig_usr(int x){

   gCanReadRedirectFifo = 0;
   gCanReadSipEvents = 0;
   return;
}


///This function returns in zError the integer value for the RFC3261:SIP error passed by the string causeCode.
int mapArcCauseCodeToSipError(char * causeCode, int * zError)
{

#if 0
		case 508:			 /**/
		case 486:			 /*Busy Here*/
		case 480:return (51);/*Temporarily unavailable*/
				break;

		case 408:return (50);/*Request Timed out*/
				break;
	
		case 403: return (51); /*Service forbidden*/
				break;

		case 484:			 /*Address incomplete*/
		case 503:			 /*Service Unavailable*/
		case 404:return (52);/*Not found*/
                break;
		
		case 500:			/*Interval server error*/	
		default: return(21);/*All other*/
#endif

	int yIntCauseCode = 404;

	if(!causeCode) 		return(0);

	if(!causeCode[0]) 	return(0);
	
	if(strcmp(causeCode, "UNASSIGNED_NUMBER" ) == 0)
		yIntCauseCode = 484;
	else if(strcmp(causeCode, "NORMAL_CLEARING") == 0)
		yIntCauseCode = 480;
	else if(strcmp(causeCode, "CHANNEL_UNACCEPTABLE") == 0)
		yIntCauseCode = 403 ;
	else if(strcmp(causeCode, "USER_BUSY") == 0)
		yIntCauseCode = 486;
	else if(strcmp(causeCode, "CALL_REJECTED") == 0)
		yIntCauseCode = 403 ;
	else if(strcmp(causeCode, "DEST_OUT_OF_ORDER") == 0)
		yIntCauseCode = 480 ;
	else if(strcmp(causeCode, "NETWORK_CONGESTION") == 0)
		yIntCauseCode = 503 ;
	else if(strcmp(causeCode, "REQ_CHANNEL_NOT_AVAIL") == 0)
		yIntCauseCode = 503;
	else if(strcmp(causeCode, "REQ_CHANNEL_NOT_AVAILABLE") == 0)
		yIntCauseCode = 503;
	else if(strcmp(causeCode, "SEND_SIT") == 0)
		yIntCauseCode = 404;
	else if(strcmp(causeCode, "GC_UNASSIGNED_NUMBER" ) == 0)
		yIntCauseCode = 484;
	else if(strcmp(causeCode, "GC_NORMAL_CLEARING") == 0)
		yIntCauseCode = 480;
	else if(strcmp(causeCode, "GC_CHANNEL_UNACCEPTABLE") == 0)
		yIntCauseCode = 403 ;
	else if(strcmp(causeCode, "GC_USER_BUSY") == 0)
		yIntCauseCode = 486;
	else if(strcmp(causeCode, "GC_CALL_REJECTED") == 0)
		yIntCauseCode = 403 ;
	else if(strcmp(causeCode, "GC_DEST_OUT_OF_ORDER") == 0)
		yIntCauseCode = 480 ;
	else if(strcmp(causeCode, "GC_NETWORK_CONGESTION") == 0)
		yIntCauseCode = 503 ;
	else if(strcmp(causeCode, "GC_REQ_CHANNEL_NOT_AVAIL") == 0)
		yIntCauseCode = 503;
	else if(strcmp(causeCode, "GC_REQ_CHANNEL_NOT_AVAILABLE") == 0)
		yIntCauseCode = 503;
	else if(strcmp(causeCode, "SEND_SIT") == 0)
		yIntCauseCode = 404;
	else
		yIntCauseCode = 480;

	*zError = yIntCauseCode;

	return(0);
}/*END: int mapArcCauseCodeToSipError*/

///The function is used to translate RFC3261:SIP error codes to Telecom error return codes.
int mapSipErrorToArcError (int zSipError)
{
	switch (zSipError)
	{
		case 508:
		/**/ case 486:		/*Busy Here */
		case 480:
			return (51);	/*Temporarily unavailable */
			break;

		case 408:
			return (50);	/*Request Timed out */
			break;

		case 403:		/*Service forbidden */
		case 503:
			return (52);	/*Service UnAvailable */
			break;

		case 484:		/*Address incomplete */
		case 404:
			return (52);	/*Not found */
			break;

		case 500:		/*Interval server error */
		default:
			return (21);	/*All other */
	}


#if 0
/*
Default Mappings

The following table lists PSTN cause codes and 
the corresponding SIP event mappings that are set by default. 
Any code other than the codes listed are mapped by default to 500 Internal server error.

Table 1   Default PSTN Cause Code to SIP Event Mappings

PSTN Cause Code  	Description  	SIP Event 

1 Unallocated number 404 Not found
2 No route to specified transit network 404 Not found
3 No route to destination 404 Not found
17 User busy 486 Busy here
18 No user response 480 Temporarily unavailable
19 No answer from the user
20 Subscriber absent
21 Call rejected 403 Forbidden
22 Number changed 410 Gone
26 Non-selected user clearing 404 Not found
27 Destination out of order 404 Not found
28 Address incomplete 484 Address incomplete
29 Facility rejected 501 Not implemented
31 Normal, unspecified 404 Not found
34 No circuit available 503 Service unavailable
38 Network out of order 503 Service unavailable
41 Temporary failure 503 Service unavailable
42 Switching equipment congestion 503 Service unavailable
47 Resource unavailable 503 Service unavailable
55 Incoming class barred within Closed User Group (CUG) 403 Forbidden
57 Bearer capability not authorized 403 Forbidden
58 Bearer capability not presently available 501 Not implemented
65 Bearer capability not implemented 501 Not implemented 
79 Service or option not implemented 501 Not implemented 
87 User not member of Closed User Group (CUG) 503 Service Unavailable 
88 Incompatible destination 400 Bad request 
95 Invalid message 400 Bad request 
102 Recover on Expires timeout 408 Request timeout 
111 Protocol error 400 Bad request

Any code other than those listed above: 500 Internal server error
*/

#endif

	return(0);
}/*END: int mapSipErrorToArcError */

///This function mapps the opcode number to a Dynamic Manager Operation.
/**
In the file /home/dev/isp2.2/SIPTelecom/2.0/include/AppMessages.h there are a 
list of DMOP operations.  These operations are called Dynamic Manager 
operations and are sent to Call Manager using a MsgToDM struct on the 
RequestFifo.  All of these structs must have a value MsgToDM.opcode.  This
opcode is the number value corresponding to the DMOP operations defined in
AppMessages.h and the following function returns the string equivalent for
the integer opcode value passed to it.
*/
int getOpcodeString (int zOpcode, char *zOut)
{
	if (!zOut)
	{
		return 0;
	}

	switch (zOpcode)
	{
	case DMOP_TRANSFERCALL:
		sprintf (zOut, "%s", "DMOP_TRANSFERCALL");
		break;

	case DMOP_BRIDGECONNECT:
		sprintf (zOut, "%s", "DMOP_BRIDGECONNECT");
		break;

	case DMOP_RESACT_STATUS:
		sprintf (zOut, "%s", "DMOP_RESACT_STATUS");
		break;

	case DMOP_INITIATECALL:
		sprintf (zOut, "%s", "DMOP_INITIATECALL");
		break;

	case DMOP_SETGLOBAL:
		sprintf (zOut, "%s", "DMOP_SETGLOBAL");
		break;

	case DMOP_GETGLOBAL:
		sprintf (zOut, "%s", "DMOP_GETGLOBAL");
		break;

	case DMOP_SETGLOBALSTRING:
		sprintf (zOut, "%s", "DMOP_SETGLOBALSTRING");
		break;

	case DMOP_GETGLOBALSTRING:
		sprintf (zOut, "%s", "DMOP_GETGLOBALSTRING");
		break;

	case DMOP_DISCONNECT:
		sprintf (zOut, "%s", "DMOP_DISCONNECT");
		break;

	case DMOP_RTPDETAILS:
		sprintf (zOut, "%s", "DMOP_RTPDETAILS");
		break;

	case DMOP_EXITALL:
		sprintf (zOut, "%s", "DMOP_EXITALL");
		break;

	case DMOP_ANSWERCALL:
		sprintf (zOut, "%s", "DMOP_ANSWERCALL");
		break;

	case DMOP_INITTELECOM:
		sprintf (zOut, "%s", "DMOP_INITTELECOM");
		break;

	case DMOP_OUTPUTDTMF:
		sprintf (zOut, "%s", "DMOP_OUTPUTDTMF");
		break;

	case DMOP_RECORD:
		sprintf (zOut, "%s", "DMOP_RECORD");
		break;

	case DMOP_RECVFAX:
		sprintf (zOut, "%s", "DMOP_RECVFAX");
		break;

	case DMOP_SPEAK:
		sprintf (zOut, "%s", "DMOP_SPEAK");
		break;

	case DMOP_SENDFAX:
		sprintf (zOut, "%s", "DMOP_SENDFAX");
		break;

	case DMOP_SRRECOGNIZEFROMCLIENT:
		sprintf (zOut, "%s", "DMOP_SRRECOGNIZEFROMCLIENT");
		break;

	case DMOP_SPEECHDETECTED:
		sprintf (zOut, "%s", "DMOP_SPEECHDETECTED");
		break;

	case DMOP_CANTFIREAPP:
		sprintf (zOut, "%s", "DMOP_CANTFIREAPP");
		break;

	case DMOP_PORTOPERATION:
		sprintf (zOut, "%s", "DMOP_PORTOPEATION");
		break;

	case STOP_ACTIVITY:
		sprintf (zOut, "%s", "STOP_ACTIVITY");
		break;

	case DMOP_REJECTCALL:
      	sprintf(zOut, "%s", "DMOP_REJECTCALL");
      	break;

	case DMOP_DROPCALL:
		sprintf (zOut, "%s", "DMOP_DROPCALL");
		break;

	case DMOP_APPDIED:
		sprintf (zOut, "%s", "DMOP_APPDIED");
		break;

	case RMOP_REGISTER:
		sprintf (zOut, "%s", "RMOP_REGISTER");
		break;

	default:
		sprintf (zOut, "%s", "UNKNWON");
	}

	return (0);

}/*int getOpcodeString */

///This function launchs a thread that reads requests sent to it by Dynamic Manager.
/**
In order for a Dynamic Manager to start receiving calls, ArcSipRedirector has
to redirect the SIP INVITE to it.  The way ArcSipRedirector knows how many
Dynamic Managers it has is by launching a thread to read a FIFO on which Call
Manager will send RMOP_REGISTER messages.  This function launches that thread.
*/
int startDMRequestReaderThread ()
{
	const char *mod = "startDMRequestReaderThread";
	int yRc = 0;
	int zCall = -1;


	yRc = pthread_attr_init (&gDMReqReaderThreadAttr);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
			"Failed to intialize thread attributes. Exiting. rc=%d. [%d, %s]",
			yRc, errno, strerror(errno));

		return (-1);
	}

	yRc = pthread_attr_setdetachstate (&gDMReqReaderThreadAttr,
					   PTHREAD_CREATE_DETACHED);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
			"Failed to set thread detach state. Exiting. rc=%d. [%d, %s]",
			yRc, errno, strerror(errno));

		return (-1);
	}

	yRc = pthread_create (&gDMReqReaderThreadId, &gDMReqReaderThreadAttr,
			      readAndProcessDMRequests, NULL);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
			"Failed to create thread .  Exiting. rc=%d. [%d, %s].",
		   	yRc, errno, strerror(errno));

		return (-1);
	}

	return (0);

}/*END: int startDMRequestReaderThread */

///This function opens the FIFO used for communication between ArcSipRedirector and Dynamic Manager.
/**
In order for the thread readAndProcessDMRequests to read RMOP_REGISTER messages
it has to open a FIFO between ArcSipRedirector and Dynamic Manager.  That is
what the following function does.
*/
int openRedirectFifo ()
{
	const char *mod = "openRedirectFifo";
	int i;
	int	rc;

	sprintf (gRedirectFifo, "%s/%s", gFifoDir, "RedirectorFifo");

	if ((mknod (gRedirectFifo, S_IFIFO | 0777, 0) < 0) && (errno != EEXIST))
	{

		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
			"Unable to create fifo (%s). mknod() system call failed. [%d, %s]", 
			gRedirectFifo, errno, strerror(errno));

		return (-1);
	}

	if ((gRedirectFifoFd = open (gRedirectFifo, O_RDWR)) < 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
			   	"Unable to open redirect fifo (%s). rc=%d. [%d, %s]",
			   	gRedirectFifo, gRedirectFifoFd, errno, strerror(errno));
	
		return (-1);
	}

	gCanReadRedirectFifo = 1;

	return (0);

}/*END: int openRedirectFifo */

///This function sends the DMOP_SHUTDOWN message to Call Manager.
/**
In order to make sure that Call Manager does not use up too much memory, after
it has received 100,000 calls ArcSipRedirector needs to tell it to shutdown.
This function creates a DMOP_SHUTDOWN message and writes it to the
RequestFifo, which cause Dynamic Manager to shutdown and restart once all the
calls it has are finished.
*/
int sendShutdownToDM(int zDmId)
{

	int rc;

	const char *mod = "sendShutdownToDM";

	struct MsgToDM yMsgToDM;

	char yRequestFifo[256];

	int fd;

	sprintf (yRequestFifo, "%s", gDynMgrList[zDmId].requestFifo);

	yMsgToDM.opcode = DMOP_SHUTDOWN;
	yMsgToDM.appCallNum = -1;

	if ((mknod (yRequestFifo, S_IFIFO | 0666, 0) < 0) && (errno != EEXIST))
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
			"Unable to create fifo (%s). mknod() system call failed. [%d, %s]", 
			yRequestFifo, errno, strerror(errno));
		return (-1);
	}

	if ((fd = open (yRequestFifo, O_RDWR)) < 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
			   	"Unable to open request fifo (%s). rc=%d. [%d, %s]",
			   	yRequestFifo, gRedirectFifoFd, errno, strerror(errno));
		return (-1);
	}

	rc = write (fd, &(yMsgToDM), sizeof(struct MsgToDM));
	if (rc == -1)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_BASE, ERR,
			   "Failed to write to Request Fifo (%s). [%d, %s]",
			   yRequestFifo, errno, strerror(errno));

		return (-1);
	}

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"Wrote %d bytes to %s.",
		rc, gDynMgrList[zDmId].requestFifo);

	close(fd);

	return(0);

}/*END: int sendShutdownToDM*/

///This function reads requests sent to ArcSipRedirector from Dynamic Manager.
/**
In order for ArcSipRedirector to know how many Dynamic Managers can take calls
it needs to received RMOP_REGISTER messages from these Dynamic Managers.  It
does so using this function with is actually a thread that runs all the time.
This thread first opens ArcSipRedirector's FIFO and then waits for
registration messages on it.  Once it receives an RMOP_REGISTER message it
adds that Dynamic Manager to its list of available Dynamic Managers to send
calls to.
*/
void * readAndProcessDMRequests (void *z)
{
	const char *mod = "readAndProcessDMRequests";
	int rc, yRc;
	int ts;
	int zCall;
	char yMsg[MAX_LOG_MSG_LENGTH];
	struct Msg_Register yMsgToDM;
	struct Msg_Register *ptrMsgToDM;
	struct Msg_Register *ptrMsg_Register;
	pthread_attr_t thread_attr;
	char yStrOpcode[64];

	gTotalRegisterCount = 0;

	rc = openRedirectFifo();
	if(rc != 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_REDIRECTOR_SHUTDOWN, ERR,
			" Exiting.", rc);

		sleep (EXIT_SLEEP);
		arc_exit(-1, (char *)"");
	}

	while (gCanReadRedirectFifo)
	{
		usleep (100 * 1000);

		ptrMsgToDM = &yMsgToDM;

		/*
		 * Read the next API statement from the fifo 
		 */

		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO, 
			"Waiting for app requests.");

		rc = read (gRedirectFifoFd, ptrMsgToDM, sizeof (struct Msg_Register));
		if (rc == -1)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR, 
				"Failed to read from (%s). [%d, %s].", 
				gRedirectFifo, errno, strerror(errno));
				// so why are we freeing this ? -- 
				// free (ptrMsgToDM);
				ptrMsgToDM = NULL;
				continue;
		}

		getOpcodeString (ptrMsgToDM->opcode, yStrOpcode);

		ptrMsg_Register = (struct Msg_Register *)ptrMsgToDM;

		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO, 
			"Received %d bytes from Dynamic Manager(%d). msg={op:%d(%s),fifo:%s}",
			rc,
			ptrMsg_Register->dmId,
			ptrMsgToDM->opcode,
			yStrOpcode,
			ptrMsg_Register->requestFifo);

		switch (yMsgToDM.opcode)
		{
			case RMOP_DEREGISTER:

            {
				int dynMgrId = -1;
				FILE *yFpReadStatus = NULL;
				int i = -1;

				ptrMsg_Register = (struct Msg_Register *)ptrMsgToDM;

				dynMgrId = ptrMsg_Register->dmId;

				if(dynMgrId >= 0 && dynMgrId < MAX_REDIRECTS)
				{
                    char portno[10];
                    char username[256];
					gDynMgrList[dynMgrId].port = ptrMsg_Register->port;
                    snprintf(portno, sizeof(portno), "%d", ptrMsg_Register->port);
					sprintf(gDynMgrList[dynMgrId].ip, "%s", ptrMsg_Register->ip);
					sprintf(gDynMgrList[dynMgrId].requestFifo, "%s", ptrMsg_Register->requestFifo);
					gDynMgrList[dynMgrId].status = UP;
					gDynMgrList[dynMgrId].sentCalls = 0;
					gTotalRegisterCount++;
                    snprintf(username, sizeof(username), "%s%d", LOCAL_DYNMGR_NAME, dynMgrId);

                    if(atoi(portno)){
                      ACTIVE_REGISTRATION_LOCK;
                      ActiveRegistrationDelete(&ActiveRegistrationList, username, ptrMsg_Register->ip, portno);
				  	  JSPRINTF(" %s() deleted name=%s ip=%s portno=%s\n", __func__, username, ptrMsg_Register->ip, portno);
                      ACTIVE_REGISTRATION_UNLOCK;
			          dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			            "DMOP_DEREGISTER: Deleted DynMgr IPC Registration with name=[%s], IP=[%d] and Port Number=[%s]",
			            username, ptrMsg_Register->ip, portno);
                    }

                    if (atoi(portno)){
					   gDynMgrList[dynMgrId].status = DOWN;
					   gDynMgrList[dynMgrId].sentCalls = 0;
					   JSPRINTF(" %s() REDIRECTOR attempting to remove an active registration for dynmgr id...\n", __func__);
                       arc_sip_redirection_set_active (sip_redirection_table, SIP_REDIRECTION_TABLE_SIZE, 
												gHostIp, gSipPort + dynMgrId, SIP_REDIRECTION_DEACTIVATE);
                    }
                } 
            }
            break;


			case RMOP_REGISTER:
			{
				int dynMgrId = -1;
				FILE *yFpReadStatus = NULL;
				int i = -1;

				ptrMsg_Register = (struct Msg_Register *)ptrMsgToDM;

				dynMgrId = ptrMsg_Register->dmId;

				if(dynMgrId >= 0 && dynMgrId < MAX_REDIRECTS)
				{
                    char portno[10];
                    char username[256];
					gDynMgrList[dynMgrId].port = ptrMsg_Register->port;
                    snprintf(portno, sizeof(portno), "%d", ptrMsg_Register->port);
					sprintf(gDynMgrList[dynMgrId].ip, "%s", ptrMsg_Register->ip);
					sprintf(gDynMgrList[dynMgrId].requestFifo, "%s", ptrMsg_Register->requestFifo);
					gDynMgrList[dynMgrId].status = UP;
					gDynMgrList[dynMgrId].sentCalls = 0;
					gTotalRegisterCount++;
                    snprintf(username, sizeof(username), "%s%d", LOCAL_DYNMGR_NAME, dynMgrId);

                    if(atoi(portno)){
                      ACTIVE_REGISTRATION_LOCK;
                      ActiveRegistrationListAdd(&ActiveRegistrationList, ACTIVE_REGISTRATION_TYPE_LOCAL,
							username, -1, ptrMsg_Register->pid, dynMgrId, ptrMsg_Register->ip, portno);
					  JSPRINTF(" %s() added name=%s ip=%s portno=%s\n", __func__, username, ptrMsg_Register->ip, portno);
                      ACTIVE_REGISTRATION_UNLOCK;
			          dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			            "DMOP_REGISTER: Added DynMgr IPC Registration with name=[%s], IP=[%d] and Port Number=[%s]",
			            username, ptrMsg_Register->ip, portno);
                    }

                    if (atoi(portno)){
					   gDynMgrList[dynMgrId].status = UP;
					   gDynMgrList[dynMgrId].sentCalls = 0;
					   JSPRINTF(" %s() REDIRECTOR attempting to add an active registration for dynmgr id...\n", __func__);
                       arc_sip_redirection_set_active (sip_redirection_table, SIP_REDIRECTION_TABLE_SIZE, 
												gHostIp, gSipPort + dynMgrId, SIP_REDIRECTION_ACTIVATE);
                    } 

				}

#if 0

				unlink(".portStatus");

				yFpReadStatus = fopen(".portStatus", "w+");

				if(yFpReadStatus != NULL)
				{

					for(i = 0; i < MAX_REDIRECTS; i++)
					{
						if(gDynMgrList[i].status == UP)
						{
							fprintf (yFpReadStatus, 
								"port=%d:id=%d:ip=%s\n",
								gDynMgrList[i].port, i, gDynMgrList[i].ip, gDynMgrList[i].port);
						}
					}

					fflush(yFpReadStatus);

					fclose(yFpReadStatus);
				}
#endif

			}

			break;

			default:
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_OPCODE, ERR,
					   "Unknown request opcode (%d) received from pid=%d.",
					   gCall[zCall].msgToDM.opcode,
					   gCall[zCall].msgToDM.appPid);
			}
			break;

		}/* switch */

	}/*END: while(gCanReadRedirectFifo) */

	pthread_detach (pthread_self ());
	pthread_exit (NULL);
	return (NULL);

}/*END: void * readAndProcessAppRequests */

///ArcSipRedirector's main function
/**
ArcSipRedirector's main function is to initialize the eXosip2 stack and wait
for SIP INVITE messages.  Once it receives these messages it sends a 3xx
Redirection message back to the caller to redirect its INVITE's to the next
available Dynamic Manager.  It also keeps track of how many calls each
Dynamic Manager receives and when the Dynamic Manager has received 100,000
calls it sends it a DMOP_SHUTDOWN message.
*/

int gShutdownRegistrations = 0;

#define CALL_MANAGER_STACK_SIZE (32 * 1024 * 1024)


int main(int argc, char *argv[])
{
	int yRc;
	const char *mod = "main";
	char yErrMsg[256];


	int yIntExosipInitCount = 0;
	int zCall = -1;

	int yIntReadyWaitCount = 0;
	int yIntCurrentDynMgr = -1;

    int total_redirections = 0;
	
	eXosip_event_t *eXosipEvent = NULL;

	/*
	 * Set the signal handlers for segv and pipe
	 */

    arc_dbg_open(argv[0]);

    snprintf(gProgramName, sizeof(gProgramName), "%s", argv[0]);


#if 1
    struct rlimit rlim;
    memset(&rlim, 0, sizeof(rlim));

    rlim.rlim_cur = CALL_MANAGER_STACK_SIZE;
    rlim.rlim_max = CALL_MANAGER_STACK_SIZE;

    int rc = setrlimit(RLIMIT_STACK, &rlim);

    if(rc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_DIAGNOSE, TEL_INITIALIZATION_ERROR, WARN,
			"Failed to set rlimit for stack size to %d.", CALL_MANAGER_STACK_SIZE);
    }
	else
	{
		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
      			"Successfuly set rlimit for stack size to %d.", 
				CALL_MANAGER_STACK_SIZE);
    }
#endif

    RegistrationInfoListInit(&RegistrationInfoList);
    SubscriptionInfoInit(&SubscriptionInfoList);

	signal (SIGPIPE, sigpipe);
	signal (SIGTERM, sigterm);
	// signal (SIGSEGV, sigsegv);
	signal (SIGUSR1, sig_usr);
	signal (SIGUSR2, sig_usr);

	/*
	 * END: Set the signal handlers for segv and pipe
	 */

	/*
	 * Print the version info
	 */
	if (argc == 2 && strcmp (argv[1], "-v") == 0)
	{
		fprintf (stdout,
			 "ARC SIP Redirect Server for SIP Telecom Services.(%s).\n"
			 "Version:%s Build:%s\n"
			 "Compiled on %s %s.\n",
			 argv[0], ARCSIP_VERSION, ARCSIP_BUILD, __DATE__,
			 __TIME__);

		exit (-1);
	}
	else if (argc == 2 && strstr (argv[1], "?") != NULL)
	{
		fprintf (stdout,
			 "ARC SIP Redirect Server for SIP Telecom Services.(%s).\n"
			 "Version:%s Build:%s\n"
			 "Compiled on %s %s\n",
			 argv[0], ARCSIP_VERSION, ARCSIP_BUILD, __DATE__,
			 __TIME__);

		fprintf (stdout, "%s",
			 "-v		Product info		\n"
			 "-vv	Version number		\n"
			 "-vb	Build number		\n"
			 "-vp	Protocol info");

		exit (-1);
	} 
    else if (argc == 2 && !strcmp(argv[1], "-d")){
         daemon(1, 0);
         write_pid_file(PID_FILE_PATH);
    }

	/*
	 * Determine base pathname. 
	 */
	ispbase = (char *) getenv ("ISPBASE");
	if (ispbase == NULL)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_INITIALIZATION_ERROR, ERR,
			   "Failed to get ISPBASE. Make sure the ISPBASE environment variable "
				"is properly set and exported, then retry.  Exiting.");
		sleep (EXIT_SLEEP);
		return (-1);
	}
	/*
	 * END: Determine base pathname. 
	 */
	dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_REDIRECTOR_STARTUP, INFO,
		"Starting up Aumtech's Call Redirector for SIP.  pid=%ld.", getpid());

	sprintf (gIspBase, "%s", ispbase);
	sprintf (gGlobalCfg, "%s/Global/.Global.cfg", gIspBase);
	sprintf (gFifoDir, "%s", "\0");

	gSipRedirection = 0;

	gTotalRegisterCount = 0;

	loadConfigFile(basename(argv[0]), NULL);


    // load redirector table 
    char tab_path[1024];
    snprintf (tab_path, sizeof(tab_path), "%s/Telecom/Tables/%s", gIspBase, REDIRECTOR_TABLE);
    arc_sip_redirection_init(sip_redirection_table, SIP_REDIRECTION_TABLE_SIZE);
    arc_sip_redirection_load_table(sip_redirection_table, SIP_REDIRECTION_TABLE_SIZE, tab_path);

	arc_sip_redirection_print_table(sip_redirection_table, SIP_REDIRECTION_TABLE_SIZE);

	if (gSipRedirection == 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_CONFIG_VALUE_NOT_FOUND, WARN,
			"Failed to continue. SIP_Redirection=OFF in .TEL.cfg.");

		while(1)
		{
			sleep(10);
		}
	}

	yRc = get_name_value ((char *)"", FIFO_DIR_NAME_IN_NAME_VALUE_PAIR,
			      DEFAULT_FIFO_DIR, gFifoDir, sizeof (gFifoDir),
			      gGlobalCfg, yErrMsg);

	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_CONFIG_VALUE_NOT_FOUND, WARN,
			"Failed to find '%s' in (%s).  Using (%s).",
			FIFO_DIR_NAME_IN_NAME_VALUE_PAIR, 
			gGlobalCfg, gFifoDir);
	}

	sprintf (gToRespFifo, "%s/%s", gFifoDir, DYNMGR_RESP_PIPE_NAME);

	for(int i = 0; i < MAX_REDIRECTS; i++)
	{
		gDynMgrList[i].dynMgrId 	= -1;
		gDynMgrList[i].status 		= DOWN;
		gDynMgrList[i].ip[0] 		= '\0';
		gDynMgrList[i].sentCalls 	= 0;
		
		gDynMgrList[i].requestFifo[0] = '\0';
	}

	/*Fire readAndProcessDMRequests*/
	yRc = startDMRequestReaderThread ();
	if (yRc != 0)
	{
		// Log message is logged in routine. No need to log it again.
		exit(-1);
	}
	/*END:Fire readAndProcessDMRequests*/

#if  0

	while(gTotalRegisterCount == 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			"Waiting for at least one DM to register with Redirector.");

		sleep(1);
	}

#endif 

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"Successfully Initialized eXosip 2  (%d).", yRc);
	
	yIntExosipInitCount = 0;

//	eXosip_guess_localip (AF_INET, gLocalIpAddress, 255);

    SipLoadConfig();
    sprintf (gLocalIpAddress, "%s", gHostIp);
    dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO, "Set local IP to (%s), [%u, %s]",
            gLocalIpAddress, gHostIp, gHostIp);

	if(access("/tmp/Redirector.debug", F_OK) == 0)
	{
		TRACE_INITIALIZE(END_TRACE_LEVEL, NULL);	
	}

    RegistrationInfoLoadConfig(gHostIp, gSipPort, -1, gIpCfg, gSipUserAgent);
    SubscriptionInfoLoadConfig(gHostIp, gSipPort, gIpCfg);

    
    // pretty harmless to lock at this point 
    // its only here for continuity 
    ACTIVE_REGISTRATION_LOCK;
    InboundRegistrationLoadConfig (&InboundRegistrationList, gIpCfg);
    ActiveRegistrationLoadDialPeers (&ActiveRegistrationList, gIpCfg);
    ACTIVE_REGISTRATION_UNLOCK;

    ACTIVE_REGISTRATION_LOCK;
    ActiveRegistrationReadFromFile(&ActiveRegistrationList);
    ACTIVE_REGISTRATION_UNLOCK;


#define PID_FILE_PATH "/tmp/ArcSipRedirector.pid"

	gCanReadSipEvents = 1;

	yIntCurrentDynMgr = -1;

	while (gCanReadSipEvents)
	// while (gCanReadSipEvents--)
	{

        //if(total_redirections == 500000){
        // gCanReadSipEvents = 0;
        //}

        if(gShutdownRegistrations){
           deRegistrationHandler(&RegistrationInfoList);
        }

		if (eXosipEvent != NULL)
		{
           eXosip_lock(geXcontext);
			eXosip_event_free (eXosipEvent);
           eXosip_unlock(geXcontext);
			eXosipEvent = NULL;
		}

		eXosipEvent = eXosip_event_wait (geXcontext, 1, 1);

        if(eXosipEvent == NULL){
           eXosip_lock(geXcontext);
           eXosip_automatic_action(geXcontext);
           eXosip_unlock(geXcontext);

           ACTIVE_REGISTRATION_LOCK;
           ActiveRegistrationListDeleteExpired (&ActiveRegistrationList);
           if(gSipRedirectionUseTable == 1)
           {
             arc_sip_redirection_reload_table(sip_redirection_table, SIP_REDIRECTION_TABLE_SIZE, tab_path);
           }
           ACTIVE_REGISTRATION_UNLOCK;
        }

		if (eXosipEvent != NULL)
		{

			switch (eXosipEvent->type)
			{

            case EXOSIP_REGISTRATION_NEW:         /**< announce new registration.       */
                EXOSIP_EVENT_DEBUG(EXOSIP_REGISTRATION_NEW);
                inboundRegistrationHandler(eXosipEvent);
                // 
                ACTIVE_REGISTRATION_LOCK;
                ActiveRegistrationDumpToFile(ActiveRegistrationList);
                ACTIVE_REGISTRATION_UNLOCK;
                break;
            case EXOSIP_REGISTRATION_SUCCESS:     /**< user is successfully registred.  */
//                EXOSIP_EVENT_DEBUG(EXOSIP_REGISTRATION_SUCCESS);
                successfulRegistrationHandler(eXosipEvent);
                break;
            case EXOSIP_REGISTRATION_FAILURE:     /**< user is not registred.          */
//                EXOSIP_EVENT_DEBUG(EXOSIP_REGISTRATION_FAILURE);
                unsuccessfulRegistrationHandler(eXosipEvent);
                break;
            case EXOSIP_REGISTRATION_REFRESHED:   /**< registration has been refreshed. */
                EXOSIP_EVENT_DEBUG(EXOSIP_REGISTRATION_REFRESHED);
                break;
            case EXOSIP_REGISTRATION_TERMINATED:  /**< UA is not registred any more.    */
                EXOSIP_EVENT_DEBUG(EXOSIP_REGISTRATION_TERMINATED);
                break;

            // subscription events 

            case EXOSIP_SUBSCRIPTION_UPDATE:       /**< announce incoming SUBSCRIBE.      */
                EXOSIP_EVENT_DEBUG(EXOSIP_SUBSCRIPTION_UPDATE);
                break;
            case EXOSIP_SUBSCRIPTION_CLOSED:       /**< announce end of subscription.     */
                EXOSIP_EVENT_DEBUG(EXOSIP_SUBSCRIPTION_CLOSED);
                break;

            case EXOSIP_SUBSCRIPTION_NOANSWER:        /**< announce no answer              */
                EXOSIP_EVENT_DEBUG(EXOSIP_SUBSCRIPTION_NOANSWER);
                break;
            case EXOSIP_SUBSCRIPTION_PROCEEDING:      /**< announce a 1xx                  */
                EXOSIP_EVENT_DEBUG(EXOSIP_SUBSCRIPTION_PROCEEDING);
                break;
            case EXOSIP_SUBSCRIPTION_ANSWERED:        /**< announce a 200ok                */
                EXOSIP_EVENT_DEBUG(EXOSIP_SUBSCRIPTION_ANSWERED);
                successfulSubscriptionHandler (eXosipEvent);
                break;
            case EXOSIP_SUBSCRIPTION_REDIRECTED:      /**< announce a redirection          */
                break;
            case EXOSIP_SUBSCRIPTION_REQUESTFAILURE:  /**< announce a request failure      */
                unsuccessfulSubscriptionHandler (eXosipEvent);
                break;
            case EXOSIP_SUBSCRIPTION_SERVERFAILURE:   /**< announce a server failure       */
                break;
            case EXOSIP_SUBSCRIPTION_GLOBALFAILURE:   /**< announce a global failure       */
                break;
            case EXOSIP_SUBSCRIPTION_NOTIFY:          /**< announce new NOTIFY request     */
                incomingNotifyHandler (eXosipEvent);
                break;

            case EXOSIP_SUBSCRIPTION_RELEASED:        /**< call context is cleared.        */
                break;

            case EXOSIP_IN_SUBSCRIPTION_NEW:          /**< announce new incoming SUBSCRIBE.*/
                incomingSubscriptionHandler (eXosipEvent);
                break;
            case EXOSIP_IN_SUBSCRIPTION_RELEASED:     /**< announce end of subscription.   */
                break;


            // end subscription events 
            case EXOSIP_MESSAGE_NEW:
                if(eXosipEvent->request && MSG_IS_REGISTER(eXosipEvent->request)){
                  ACTIVE_REGISTRATION_LOCK;
                  inboundRegistrationHandler(eXosipEvent);
                  ACTIVE_REGISTRATION_UNLOCK;
                  ACTIVE_REGISTRATION_LOCK;
                  ActiveRegistrationDumpToFile(ActiveRegistrationList);
                  ACTIVE_REGISTRATION_UNLOCK;
                  }

				if(MSG_IS_OPTIONS(eXosipEvent->request))
				{

					int yRc = 0;

					osip_message_t *optionAnswer = NULL;

					eXosip_lock(geXcontext);

					yRc = eXosip_message_build_answer (geXcontext, eXosipEvent->tid, 200, &optionAnswer);

					if(yRc < 0 || optionAnswer == NULL)
					{
						 dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, DYN_BASE, ERR,
							 "Failed to build answer to options message (tid:%d) rc=%d.", eXosipEvent->tid, yRc);
					}
					else
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
							"Sending SIP Message 200 to OPTIONS.");

						osip_message_set_accept (optionAnswer, "application/sdp");
						yRc = eXosip_options_send_answer (geXcontext, eXosipEvent->tid, 200, optionAnswer);

						//yRc = eXosip_options_send_answer (eXosipEvent->tid, 488, NULL);
						if(yRc < 0)
						{
							dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, DYN_BASE, ERR,
								"Failed to send answer to options message (tid:%d).", eXosipEvent->tid);
						}
						else
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
								"Sent answer to options message (tid:%d).", eXosipEvent->tid);
						}
					}

					eXosip_unlock(geXcontext);
				}

                break;
            //



				case EXOSIP_CALL_CLOSED:
					// dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					//                "Call closed.");
                     eXosip_lock(geXcontext);
                     eXosip_call_terminate(geXcontext, eXosipEvent->cid, eXosipEvent->did);
                     eXosip_unlock(geXcontext);
                     break;

                
                case EXOSIP_CALL_RELEASED:
					// dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					//                "Call released.");
                     break;

#ifndef EXOSIP_VERSION_401
				case EXOSIP_CALL_NEW:
#else
				case EXOSIP_CALL_INVITE:
#endif
				{
					int dynMgrFound = 0;
					int dynMgrId 	= -1;
					char *yStrRedirectPort = NULL;
                    int rc = 0;

                    ACTIVE_REGISTRATION_LOCK;
                    rc = callRedirectionHandler(eXosipEvent);
                    if(rc == 0){
                      total_redirections++;
                    }
                    ACTIVE_REGISTRATION_UNLOCK;
                    break;
				}

				break;

	
				default:
				break;

			}/*switch (eXosipEvent->type) */

			usleep (100);

		}/*END: if (eXosipEvent != NULL) */
		else
		{
			usleep (1000);
		}

	}/*END: while(gCanReadSipEvents) */

    deRegistrationHandler(&RegistrationInfoList);

    // InboundRegistrationListFree(InboundRegistrationList);

    ACTIVE_REGISTRATION_LOCK;
    ActiveRegistrationDumpToFile(ActiveRegistrationList);
    ActiveRegistrationListDelete(ActiveRegistrationList);
    ACTIVE_REGISTRATION_UNLOCK;

    InboundRegistrationListFree(InboundRegistrationList);
    arc_sip_redirection_free(sip_redirection_table, SIP_REDIRECTION_TABLE_SIZE);

	eXosip_quit(geXcontext);

    arc_dbg_close();
	return (0);

}/*END: int main */

/*EOF*/
