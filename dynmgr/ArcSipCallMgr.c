/**
------------------------------------------------------------------------------
Program Name:	ArcSipCallMgr/ArcIPDynMgr
File Name:		ArcSipCallMgr.c
Purpose:  		SIP/2.0 Call Manager
Author:			Aumtech, inc.
------------------------------------------------------------------------------
*/

/**
------------------------------------------------------------------------------
Update: 07/07/2004 DDN Created the file
Update: 10/12/2005 DDN Added ability to run multiple Call Managers
Update: 04/04/2006 DDN Read static port details from file .staticCallInfo.id
Update: 06/26/2006 DDN Added original_ani and original_dnis to process_DMOP_GETGLOBALSTRING
Update: 09/12/2007 djb Added mrcpTTS logic.
Update: 09/28/2007 djb Added logic for $PLATFORM_TYPE
Update: 06/14/2010 RG Added G722 codec.
Update: 06/22/2010 djb Added blastDial logic
Update: 09/10/2013 djb Added network transfer changes
Update: 06/19/2014 djb MR 4325 added logic to CANTFIREAPP - licenses - 503
Update: 06/21/2014 djb MR 4323 moved canDynMgrExit() in dropcall thread.
Update: 08/21/2014 jms MR XXXX fixed age old memory leak in timer thread.
Update: 03/30/2016 djb MR 4558 - removed gWriteBLegCDR
Update: 06/07/2016 djb MR 4605 Updated a-leg's crossPort to -1 on bleg discconnect
Update: 10/18/2016 djb MR 4605/4642 Added logic to not process CLOSED on the
                       same channel
Update: 05/31/2019 djb MS 4906 modified size of referTo, etc to resolve core
------------------------------------------------------------------------------
*/

/**
 *	Note: Following rules should be followed while coding SIP SDM.
 *
 *	1:  Every routine must include zCall and mod as its arguments. 
 *								(MACROS wont work otherwise)
 */

#include <mcheck.h>

#define IMPORT_GLOBALS

#include "ArcSipCallMgr.h"
#include "RegistrationInfo.h"
#include "RegistrationHandlers.h"
#include "SubscriptionInfo.h"
#include "SubscriptionHandlers.h"
#include "IncomingCallHandlers.h"
#include "dynVarLog.h"
#include "dl_open.h"
#include "arc_sdp.h"
#include "osip_utils.h"
#include "SipInit.h"

#include "recycle.h"
#define RECYCLEPIDFILE ".ArcIPDynMgr"

#include "enum.h"
#include "CallOptions.hxx"
#include "CallOutboundHandlers.hxx"
// TODO the below replaces the above
#include "OutboundCallHandlers.h"

#include <eXosip2/eXosipArcCommon.h>

// fprintf debug macro 
#include "jsprintf.h"
#define JS_DEBUG 0

#ifdef ACU_LINUX
#include "arcFifos.h"
#endif

// joes 
// #include <mcheck.h>

// global externs 

#ifdef EXOSIP_EVENT_DBUG
#define EXOSIP_EVENT_DEBUG(eventname, lineno) \
	; //   fprintf(stderr, " EXOSIP_EVENT_DEBUG line=%d event=%s value=%d\n", lineno, #eventname, eventname)
#else
#define EXOSIP_EVENT_DEBUG
#endif

long            ddn_free_count;
long            ddn_malloc_count;

CallOptions     gCallInfo[MAX_PORTS];

///This integer is only true when ArcSipRedirector has sent a shutdown signal to Call Manager.
/**
In order to solve our eXosip memory leak issue we have ArcSipRedirector send
Call Manager a DMOP_SHUTDOWN command.  After receiving this command Call
Manager will wait until all the calls are done and then it will shutdown.
*/
int             gShutdownRegistrations = 0;
int             gShutDownFromRedirector = 0;
int             gShutDownFlag = 0;
int             gCallsStarted = 0;
int             gCallsFinished = 0;
time_t          gShutDownFlagTime = 0;
extern int gShutDownMaxTime;

int             gLastGivenPort = 0;

int             gReceivedHeartBeat = 0;

char            gAppPath[256];
int             gCanKillApp = YES;

pthread_attr_t  gThreadAttr;

pthread_attr_t  gAppReqReaderThreadAttr;
pthread_t       gAppReqReaderThreadId;

pthread_attr_t  gMultiPurposeTimerThreadAttr;
pthread_t       gMultiPurposeTimerThreadId;
pthread_mutex_t gMultiPurposeTimerMutex;
pthread_mutex_t gFreeCallAssignMutex;
pthread_mutex_t gRespShmMutex;
pthread_mutex_t gFreeBLegAssignMutex;

struct MultiPurposeTimerStrcut *gpFirstMultiPurposeTimerStruct = NULL;
struct MultiPurposeTimerStrcut *gpLastMultiPurposeTimerStruct = NULL;
int    MultiPurposeTimerCount = 0;

int             gRequestFifoFd;
char            gRequestFifo[256];

int             gCanReadRequestFifo;
int             gCanReadSipEvents;
int             gAppPassword = 0;

int             gFreeCount = 0;
int             gMallocCount = 0;

char			*gViaHost;
char			*gViaHostForAck;

int             gotStaticPortRequest = 0;
int             isDynMgrReady = 0;
int             writeToRespShm (int zLine, char *mod, void *zpBasePtr, int rec, int ind,
								int val);

int             gVideoLicenseAvailable = 0;
char            gVideoLicenseFeature[] = "VIDEO";

RegistrationInfoList_t RegistrationInfoList;
SubscriptionInfoList_t SubscriptionInfoList;

struct Msg_BlastDial gMsgBlastDial;

int             parseSdpMessage (int zCall, int zLine, char *zStrMessage,
								 sdp_message_t *);
int             parseFaxSdpMsg (int zCall, char *zDestInfo,
								char *zStrMessage);

int             arc_exit (int zCode, char *zMessage);
// int             process_DMOP_PLAYMEDIA (int zCall);

int				load_resource_table ();
int             getCallInfoFromSdp (int zCall, char *zStrMessage);
int             sendPRACKReq (int zCall, eXosip_event_t * eXosipEvent);
int             insertPRACKHeaders (int zCall, osip_message_t * lpMsg);
int             buildPRACKAnswer (int zCall, osip_message_t ** lpMsg);
int             checkINVITEForPRACK (int zCall, eXosip_event_t * eXosipEvent);
int             doDynMgrExit(int zCall, const char *func, int lineno);

static int      createBlastDialResultFile (int zCall, char *zResultFile);
static int		readFromRespShm(char *mod, void *zpBasePtr, int rec, int ind, char *val);

static int		getParmFromFile(int zCall, char *zLabel, char *zFrom, char *zTo);
static int      doSessionExpiry(int zCall, int zLine);
extern int		arc_add_callID (osip_message_t *msg, char *zFrom, int zCall);        // BT-37
static			void removeVideo(char *zSDPMessage);			// BT-160


#ifdef __DEBUG__
#define __DDNDEBUG__(module, xStr, yStr, zInt) arc_print_port_info(mod, module, __LINE__, zCall, xStr, yStr, zInt)
#else
#define __DDNDEBUG__(module, xStr, yStr, zInt) ;
#endif

//struct MsgToDM gDisconnectStruct =    { DMOP_DISCONNECT, -1, -1, -1 };

int             canDynMgrExit ();
int             cb_udp_snd_message_external (int zCall, osip_message_t * sip);
int             cb_udp_snd_message_external (int zCall, osip_message_t * sip,
											 char *message);
int             updateAppName (int line, int shm_index, int port);
int             setCallState (int zCall, int zState);
void            arc_print_port_info (const char *function, int module,
									 int zLine, int zCall, char *zData1,
									 char *zData2, int zData3);
int             setSessionTimersResponse(int zCall, int zLine, char *zMod, osip_message_t *response);

enum fax_tone_e
{
	ARC_FAX_TONE_CNG = 0,
	ARC_FAX_TONE_CED
};

int
arc_fax_playtone (int zCall, int which)
{
	char            mod[] = "arc_fax_playtone";
	int             rc = -1;
	char            path[1024];

	char           *files[] = {
		"fax_cng.wav",			// send
		"fax_ced.wav",			// recv
		NULL
	};

//   if(gSendFaxTone == 0){
//     fprintf(stderr, " %s() gSendFaxTone is not set returning 0\n", __func__);
//    return 0;
//}

	struct Msg_Speak ySpeakFaxTone;

	memset (&ySpeakFaxTone, 0, sizeof (ySpeakFaxTone));

	switch (which)
	{
	case ARC_FAX_TONE_CNG:
		snprintf (path, sizeof (path), "%s", files[which]);
		break;
	case ARC_FAX_TONE_CED:
		snprintf (path, sizeof (path), "%s", files[which]);
		break;
	  deafault:
		dynVarLog (__LINE__, zCall, mod,
				   REPORT_DETAIL, TEL_INVALID_INTERNAL_VALUE, WARN,
				   "Invalid tone type (%d).  Must be either ARC_FAX_TONE_CNG(%d) or "
				   "ARC_FAX_TONE_CED(%d).  Will not send faxtone to the fax machine.",
				   which, ARC_FAX_TONE_CNG, ARC_FAX_TONE_CED);
		return (-1);
		break;
	}

	if (gSendFaxTone == 1)
	{
		if (access (path, F_OK) == 0)
		{
			ySpeakFaxTone.opcode = DMOP_SPEAK;
			ySpeakFaxTone.appPid = gCall[zCall].msgToDM.appPid;
			ySpeakFaxTone.appRef = gCall[zCall].msgToDM.appRef;
			ySpeakFaxTone.appPassword = zCall;
			ySpeakFaxTone.appCallNum = zCall;
			ySpeakFaxTone.list = 0;
			ySpeakFaxTone.synchronous = SYNC;
			ySpeakFaxTone.allParties = FIRST_PARTY;
			ySpeakFaxTone.interruptOption = NONINTERRUPT;
			//
			snprintf (ySpeakFaxTone.file, sizeof (ySpeakFaxTone.file), "%s",
					  path);
			snprintf (ySpeakFaxTone.key, sizeof (ySpeakFaxTone.key), "%s",
					  "faxtone");
			//
			rc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &ySpeakFaxTone,
								 (char *) __func__);
		}
	}

	return rc;

}//arc_fax_playtone

int
arc_fax_stoptone (int zCall)
{

	int             rc = -1;

	struct Msg_Fax_StopTone yMsgFaxStopTone;
	struct Msg_PortOperation yPortOperation;

	if (gSendFaxTone == 1)
	{
		memset (&yPortOperation, 0, sizeof (yPortOperation));

		yPortOperation.opcode = DMOP_PORTOPERATION;
		yPortOperation.appPid = gCall[zCall].msgToDM.appPid;
		yPortOperation.appRef = gCall[zCall].msgToDM.appRef;
		yPortOperation.appPassword = zCall;
		yPortOperation.appCallNum = zCall;
		yPortOperation.operation = STOP_FAX_TONE_ACTIVITY;
		yPortOperation.callNum = zCall;
		rc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &yPortOperation,
							 (char *) __func__);

		memset (&yMsgFaxStopTone, 0, sizeof (yMsgFaxStopTone));
		yMsgFaxStopTone.opcode = DMOP_FAX_STOPTONE;
		yMsgFaxStopTone.appPid = gCall[zCall].msgToDM.appPid;
		yMsgFaxStopTone.appRef = gCall[zCall].msgToDM.appRef;
		yMsgFaxStopTone.appPassword = zCall;
		rc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &yMsgFaxStopTone,
							 (char *) __func__);
	}

	return rc;

}//arc_fax_stoptone




int
arc_fixup_sip_request_line (osip_message_t * msg, char *ip, int port,
							int zCall)
{

	int             rc = -1;
	char            request[256];
	osip_uri_t     *request_uri = NULL;
	osip_uri_param_t *transport = NULL;
	char           *name = "transport";
	char            value[10] = "udp";

	if (msg == NULL)
	{
		return -1;
	}

	request_uri = osip_message_get_uri (msg);

	if (request_uri)
	{
		rc = osip_uri_param_get_byname (&request_uri->url_params, name,
										&transport);
		if (rc == 0)
		{
			// fprintf(stderr, " %s() transport is already set in request line\n", __func__);
			return 0;
		}
		else
		{
			if (strcmp (gHostTransport, "tcp") == 0)
			{
				snprintf (value, sizeof (value), "%s", "tcp");
			}
			else if (strcmp (gHostTransport, "tls") == 0)
			{
				snprintf (value, sizeof (value), "%s", "tls");
			}
			else
			{
				;;
			}

			osip_uri_uparam_add (request_uri, osip_strdup (name),
								 osip_strdup (value));
			//osip_free(transport);
			rc = 0;
		}
	}
	return rc;

}//arc_fixup_sip_request_line

//
// fixup contacts for problem with stack 
// should continue to research the problem 
// with the stack to come up with a better fix --joes
//

// BT-92
int
arc_fixup_contact_header_with_displayname (int zLine, osip_message_t * msg, char *dispName, char *ip, int port, int zCall)
{
	static char mod[]="arc_fixup_contact_header_with_displayname";
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

		// fprintf(stderr, " %s() msg->contacts=%p", __func__, &msg->contacts);
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
					switch (mode)
					{
					case UDP:
						if (gEnableIPv6Support != 0)
						{
							if (ip && port)
							{
								if (dispName)
								{
									snprintf (uri, sizeof (uri),
										  "\"%s\" <sip:%s@[%s]:%d>",
										  dispName, gCall[zCall].dnis, ip, port);
								}
								else
								{
									snprintf (uri, sizeof (uri),
										  "<sip:%s@[%s]:%d>",
										  gCall[zCall].dnis, ip, port);
								}
							}
							else
							{
								if (dispName)
								{
									snprintf (uri, sizeof (uri), "\"%s\" <sip:%s@[%s]>",
										  dispName, gCall[zCall].dnis, ip);
								}
								else
								{
									snprintf (uri, sizeof (uri), "<sip:%s@[%s]>",
										  gCall[zCall].dnis, ip);
								}
							}
						}
						else
						{
							if (ip && port)
							{
								if (dispName)
								{
									snprintf (uri, sizeof (uri), "\"%s\" <sip:%s@%s:%d>",
										  dispName, gCall[zCall].dnis, ip, port);
								}
								else
								{
									snprintf (uri, sizeof (uri), "<sip:%s@%s:%d>",
										  gCall[zCall].dnis, ip, port);
								}
							}
							else
							{
								if (dispName)
								{
									snprintf (uri, sizeof (uri), "\"%s\" <sip:%s@%s:%d>",
										  dispName, gCall[zCall].dnis, ip, port);
								}
								else
								{
									snprintf (uri, sizeof (uri), "<sip:%s@%s>",
										  gCall[zCall].dnis, ip);
								}
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
								if (dispName)
								{
									snprintf (uri, sizeof (uri),
										  "\"%s\" <sip:%s@[%s]:%d>",
										  dispName, gCall[zCall].dnis, ip, port);
								}
								else
								{
									snprintf (uri, sizeof (uri),
										  "<sip:%s@[%s]:%d>",
										  gCall[zCall].dnis, ip, port);
								}
							}
							else
							{
								if (dispName)
								{
									snprintf (uri, sizeof (uri), "\"%s\" <sip:%s@[%s]>",
										  dispName, gCall[zCall].dnis, ip);
								}
								else
								{
									snprintf (uri, sizeof (uri), "<sip:%s@[%s]>",
										  gCall[zCall].dnis, ip);
								}
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

	return 0;

}/*END: arc_fixup_contact_header */



int
arc_fixup_contact_header (osip_message_t * msg, char *ip, int port, int zCall)
{
	static char mod[]="arc_fixup_contact_header";
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

		// fprintf(stderr, " %s() msg->contacts=%p", __func__, &msg->contacts);
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

	return 0;

}/*END: arc_fixup_contact_header */

int
arc_get_port_from_contact_header (char *zData, char *zPort)
{
    char    tmpBuf[512];
	
    char    a1[10];
    int     i;
    char    *p;

	sprintf(tmpBuf, "%s", zData);
	p = index(tmpBuf, ';');
	if ( ! p )
	{
		return(-1);	
	}

	*p = '\0';
	p--;
	while ( isdigit(*p) )
	{
		p--;
	}
	p++;
	sprintf(zPort, "%s", p);

	return(0);
} // END: arc_get_port_from_contact_header()

int
arc_convert_ascii_to_hex (char *to, char *from)
{
	char            yTmpStrHexConversion[56];

	if (!to)
		return -1;

	to[0] = 0;

	for (int i = 0; i < strlen (from); i++)
	{
		sprintf (yTmpStrHexConversion, "%x", from[i]);
		strcat (to, yTmpStrHexConversion);
		sprintf (yTmpStrHexConversion, "\0");
	}

	return 0;

}/*END: int arc_convert_ascii_to_hex */


#include <ctype.h>

int 
arc_percent_convert(char *src, size_t size, char *dest, size_t dstsize){

  int rc = 0;

  const char table[] = {
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1,
   1, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
   0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  if(!src && !dest){
     return 0;
  }

  while(*src && dstsize){
    if(isprint(*src)){
     if(table[(int)*src] && dstsize > 3){
       dstsize -= snprintf(dest, 4, "%%%X", *src);
       src++; dest += 3; rc += 3;
     } else if(dstsize > 0){
       *dest++ = *src++;
       dstsize--; rc++;
     } else {
       src++;
     }
   } else {
      src++;
   }
  }

  dest[rc] = '\0';

  return rc;
}


int 
arc_get_global_headers(int zCall, osip_message_t *msg){

   int rc = 0;
   char buff[1024]; 
   int len = 0;
	static char mod[]="arc_get_global_headers";

   if(zCall < 0 || zCall >= MAX_PORTS){
     return 0;
   }

   if(!msg){
     return 0;
   }

   // drop in for getting and setting global vars from sip message headers 
   // joe S.  Thu Apr 23 13:06:49 EDT 2015

   len = arc_sip_msg_get_usertouser(msg, buff, sizeof(buff), 1);
   if(len > 0 && len < sizeof(gCall[zCall].remote_UserToUser)){
     memcpy(gCall[zCall].remote_UserToUser, buff, len);
     gCall[zCall].remote_UserToUser_len = len;
	 gCall[zCall].remote_UserToUser[len] = '\0';
     //fprintf(stderr, "DDN_DEBUG: %s() SUCCESS, user to user msg=[%s] len=%d\n", __func__, buff, len);
   }
	else
	{
		;//fprintf(stderr, "DDN_DEBUG: %s() FAILED, user to user msg=[%s] len=%d\n", __func__, buff, len);
	}

   // other sip headers may follow 

#if 0
  if(MSG_IS_INVITE(msg)) {	
   len = arc_sip_msg_get_useragent(msg, buff);

//	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "buff:(%s)", buff);

   if(len > 0 && len < sizeof(gCall[zCall].remoteAgentString)){
     memcpy(gCall[zCall].remoteAgentString, buff, len);

	 /* TODO:   Create comprehensive list of standard agents. 
	  *			Refer to: http://www.voip-info.org/wiki/view/SIP+user+agent+identification
	  *			
	 */
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			"gCall[%d].remoteAgentString:(%s)", zCall, gCall[zCall].remoteAgentString);

//	 if(strstr(gCall[zCall].remoteAgentString, "Avaya CM")) {

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			"Setting gCall[%d].remoteAgent:(%d:%x) to %d", zCall, gCall[zCall].remoteAgent, gCall[zCall].remoteAgent, USER_AGENT_AVAYA_CM);

		gCall[zCall].remoteAgent |= USER_AGENT_AVAYA_CM;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			"Sett gCall[%d].remoteAgent:(%d:%x) to %d", zCall, gCall[zCall].remoteAgent, gCall[zCall].remoteAgent, USER_AGENT_AVAYA_CM);

//	 }
//	 else{
//		gCall[zCall].remoteAgent |= USER_AGENT_UNKNOWN;
//	 }
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			"gCall[%d].remoteAgent:(%d:%x)", zCall, gCall[zCall].remoteAgent, gCall[zCall].remoteAgent);
   }
  }
#endif
	gCall[zCall].remoteAgent = gUUIEncoding;
	if(gCall[zCall].remoteAgent & USER_AGENT_AVAYA_CM)
	{ 
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			"gCall[%d].remoteAgent:(%d:%x).  UUI_Encoding = HEX_JETBLUE.",
			zCall, gCall[zCall].remoteAgent, gCall[zCall].remoteAgent);
	}
	if(gCall[zCall].remoteAgent & USER_AGENT_AVAYA_SM)
	{ 
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			"gCall[%d].remoteAgent:(%d:%x).  UUI_Encoding = HEX_SYKES.",
			zCall, gCall[zCall].remoteAgent, gCall[zCall].remoteAgent);
	}
	else if(gCall[zCall].remoteAgent & USER_AGENT_GENERIC)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			"gCall[%d].remoteAgent:(%d:%x).  UUI_Encoding = GENERIC.",
			zCall, gCall[zCall].remoteAgent, gCall[zCall].remoteAgent);
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			"gCall[%d].remoteAgent:(%d:%x).  UUI_Encoding = HEX (default).",
			zCall, gCall[zCall].remoteAgent, gCall[zCall].remoteAgent);
	}

   return rc;
}

int
arc_save_sip_from(int zCall, osip_from_t *from)
{
	if(!from) return -1;

	if(from->url && from->url->username)
	{
		sprintf(gCall[zCall].sipFromUsername, "%s", from->url->username);
	}

	if(from->url && from->url->host)
	{
		sprintf(gCall[zCall].sipFromHost, "%s", from->url->host);
	}

	if(from->url && from->url->port)
	{
		sprintf(gCall[zCall].sipFromPort, "%s", from->url->port);
	}

	return (0);

}//arc_save_sip_from

int
arc_save_sip_to(int zCall, osip_to_t * to)
{
	if(!to) return -1;

	if(to->url && to->url->username)
	{
		sprintf(gCall[zCall].sipToUsername, "%s", to->url->username);
	}

	if(to->url && to->url->host)
	{
		sprintf(gCall[zCall].sipToHost, "%s", to->url->host);
	}

	if(to->url && to->url->port)
	{
		sprintf(gCall[zCall].sipToPort, "%s", to->url->port);
	}

	return(0);
}//arc_save_sip_to


int
arc_insert_global_headers (int zCall, osip_message_t * msg)
{

	char            yTmpTransferAAI[1024] = "\0";
	char            yTmpStrHexConversion[56];
	char            xmUUI[256];
    char buff[1024];
    int rc;



   // added to set pai in refer only JOES: 
   if(MSG_IS_REFER(msg)){

		//BT-123

	    if( gCall[zCall].sipFromUsername[0] && 
			gCall[zCall].sipFromHost[0])
		{

			if(gCall[zCall].sipFromPort)
			{
				sprintf (buff, "<sip:%s@%s:%s>",
				 	gCall[zCall].sipFromUsername,
				 	gCall[zCall].sipFromHost,
					gCall[zCall].sipFromPort);
			}
			else
			{
				sprintf (buff, "<sip:%s@%s>",
				 	gCall[zCall].sipFromUsername,
				 	gCall[zCall].sipFromHost);
			}

            osip_message_set_header (msg, (char *) "Referred-By", buff);


        buff[0]='\0';
      }

        osip_message_set_header (msg, (char *) "Supported", "replaces");
      //END: BT-123

      if (gCall[zCall].GV_PAssertedIdentity[0] != '\0')
     {
        dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
                   "Setting P-Asserted-Identity = %s",
                   (char *)gCall[zCall].GV_PAssertedIdentity);
        osip_message_set_header (msg, (char *)"P-Asserted-Identity",
                                 gCall[zCall].GV_PAssertedIdentity);
      }
    }


	/*Set AAI as URL param */

	if (gCall[zCall].transferAAI && gCall[zCall].transferAAI[0] != '\0')
	{

		if (strcasecmp (gTransferAAIEncoding, "hex") == 0)
		{
			for (int i = 0; i < strlen (gCall[zCall].transferAAI); i++)
			{
				sprintf (yTmpStrHexConversion, "%x", gCall[zCall].transferAAI[i]);
				strcat (yTmpTransferAAI, yTmpStrHexConversion);
				sprintf (yTmpStrHexConversion, "\0");
			}
		}
		else					//encoding = none
		{
			sprintf (yTmpTransferAAI, "%s", gCall[zCall].transferAAI);
		}

		// DDN:20111212
		/* Set User-To-User as header */
	
		// BT-120 - 2-8-21
		if(gCall[zCall].remoteAgent & USER_AGENT_AVAYA_CM)
		{
			dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
				"Skipping header User-To-User %s for AVAYA CM. ", yTmpTransferAAI);
		}
		if(gCall[zCall].remoteAgent & USER_AGENT_AVAYA_SM)
		{
        	dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
                   "BT-26 Temporarily adding header User-To-User %s for AVAYA SM. ", yTmpTransferAAI);
			osip_message_set_header (msg, "User-To-User", yTmpTransferAAI);
		}
		if(gCall[zCall].remoteAgent & USER_AGENT_AVAYA_XM)
		{
        	dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
                   "BT-120 Temporarily adding header User-To-User %s AVAYA XM. ", yTmpTransferAAI);
			sprintf(xmUUI, "%s%s;encoding=hex", gTransferAAIPrefix, yTmpTransferAAI);
			osip_message_set_header (msg, "User-To-User", xmUUI);
		}
		else
		{
        	dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
                   "BT-26 Temporarily adding header User-To-User %s - default. ", yTmpTransferAAI);
			osip_message_set_header (msg, "User-To-User", yTmpTransferAAI);
		}
/*

	    osip_uri_t     *sipMainUri = NULL;
		sipMainUri = osip_message_get_uri (msg);

		if (sipMainUri)
		{
			osip_uri_uparam_add (sipMainUri,
								 osip_strdup (gTransferAAIParamName),
								 osip_strdup (yTmpTransferAAI));
			if (strcasecmp (gTransferAAIEncoding, "hex") == 0)
			{
				osip_uri_uparam_add (sipMainUri, osip_strdup ("encoding"),
									 osip_strdup ("hex"));
			}
		}

*/
	}

#if 0 // cleanup 

    // set aai as url param to refer-to header 

    if(MSG_IS_REFER(msg)){
	  if (gCall[zCall].transferAAI && gCall[zCall].transferAAI[0] != '\0'){

        char *uri_str = NULL;
    	osip_uri_t *refer_to_uri =  NULL;
    	osip_header_t *refer_to_hdr = NULL;

    	rc = osip_message_header_get_byname(msg, "Refer-To", 0, &refer_to_hdr);

        if(rc == -1){
          JSPRINTF(" %s() refer-to not found\n");
        }

        if(refer_to_hdr && refer_to_hdr->hvalue){
         osip_uri_init(&refer_to_uri);
         if(refer_to_uri == NULL){
           JSPRINTF(" %s()  failed to init uri !\n", __func__);
           return -1;
         }
         osip_uri_parse(refer_to_uri, refer_to_hdr->hvalue);
         osip_uri_uparam_add(refer_to_uri, osip_strdup("User-To-User"), osip_strdup(yTmpTransferAAI));
	     if (strcasecmp (gTransferAAIEncoding, "hex") == 0) {
				osip_uri_uparam_add (refer_to_uri, osip_strdup ("encoding"),osip_strdup ("hex"));
		 }

         arc_percent_convert(yTmpTransferAAI, sizeof(yTmpTransferAAI), buff, sizeof(buff));
         //fprintf(stderr, " %s() buff now=[%s]\n", __func__, buff);


         osip_uri_to_str(refer_to_uri, &uri_str);
         JSPRINTF(" %s() uri str=[%s]\n", __func__, uri_str);
         osip_free(refer_to_hdr->hvalue);
         refer_to_hdr->hvalue = uri_str;
        }
      }
    }
#endif 

	return (0);

}/*END: int arc_insert_global_headers */

/**
 * Quick converter for telephony type to DTMF char for RFC2833 events
 */

static int      dtmf_tab[16] =
	{ '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '*', '#', 'A', 'B',
'C', 'D' };

struct AuthenticateInfo gAuthenticateInfoStruct[100];

int
SendMediaMgrPortSwitch (int appCallNum, int deadleg, int aleg, int bleg,
						int disconnect)
{
	static char     mod[] = "SendMediaMgrPortSwitch";
	int             rc = -1;

	Msg_BridgeThirdLeg msg;

	memset (&msg, 0, sizeof (msg));
	msg.opcode = DMOP_BRIDGE_THIRD_LEG;
	msg.appRef = gCall[appCallNum].msgToDM.appRef;
	msg.appPid = gCall[appCallNum].msgToDM.appPid;
	msg.appCallNum = appCallNum;
	msg.appCallNum1 = deadleg;
	msg.appCallNum2 = aleg;
	msg.appCallNum3 = bleg;

	/*B or C leg was disconnected */
	if (disconnect)
	{
		msg.whichOne = 99;
	}

	dynVarLog (__LINE__, appCallNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "appPid=%d appRef=%d dead=%d a=%d b=%d",
			   gCall[appCallNum].msgToDM.appPid,
			   gCall[appCallNum].msgToDM.appRef, deadleg, aleg, bleg);

	rc = notifyMediaMgr (__LINE__, appCallNum, (struct MsgToDM *) &msg,
						 (char *) __func__);

	return rc;

}//SendMediaMgrPortSwitch



int 
SendApplicationPortDisconnect (int portno){

    int rc = -1;
	struct MsgToApp  AM;

	memset (&AM, 0, sizeof (AM));

	AM.opcode = DMOP_DISCONNECT;
	AM.appCallNum = portno;
	AM.appPid = gCall[portno].appPid;
	AM.appPassword = gCall[portno].appPassword;
	AM.appRef = gCall[portno].msgToDM.appRef;
    AM.returnCode = -1;

	snprintf (AM.message, sizeof (AM.message), "%s", " DynMgr recieved Disconnect, trying to notify Application");

    rc = writeGenericResponseToApp (__LINE__, portno, &AM, (char *) __func__);
    return rc;
}


int
SendMediaManagerPortDisconnect (int portno, int notifyApp)
{

	struct MsgToDM  DM;
	int             rc;

	memset (&DM, 0, sizeof (DM));

	DM.appCallNum = portno;
	DM.opcode = DMOP_DISCONNECT;
	DM.appPid = gCall[portno].appPid;
	DM.appPassword = gCall[portno].appPassword;
	DM.appRef = gCall[portno].msgToDM.appRef;

	if (notifyApp)
	{
		snprintf (DM.data, sizeof (DM.data), "%s", "true");
	}
	else
	{
		snprintf (DM.data, sizeof (DM.data), "%s", "false");
	}

	rc = notifyMediaMgr (__LINE__, portno, (struct MsgToDM *) &DM, (char *) __func__);

	return(0);
}//SendMediaManagerPortDisconnect

int
arc_add_authentication_info (char zUser[512],
							 char zId[512],
							 char zPassword[512],
							 char zAlgorithm[100], char zRealm[512])
{
	int             yIntRealmFound = 0;
	int             yIntCounter = 0;

	static int      yTotalAuthenticationsAdded = 0;

	if (zUser[0] == 0)
	{
		return (0);
	}

	if (zRealm[0] == 0)
	{
		return (0);
	}

	if (yTotalAuthenticationsAdded == 0)
	{
		for (yIntCounter = 0; yIntCounter < 100; yIntCounter++)
		{
			memset (&(gAuthenticateInfoStruct[yIntCounter]), '\0',
					sizeof (struct AuthenticateInfo));
		}
	}

	for (yIntCounter = 0; yIntCounter <= yTotalAuthenticationsAdded;
		 yIntCounter++)
	{
		if (strcmp (zRealm, gAuthenticateInfoStruct[yIntCounter].realm) == 0)
		{
			yIntRealmFound = 1;
			break;
		}
	}

#ifdef DJB_TEST
	if (yIntRealmFound)
	{
		/*No locks.    eXosip_lock is already called */
		eXosip_modify_authentication_info (zUser, zId, zPassword, zAlgorithm,
										   zRealm);
	}
	else
	{
#endif
		if (yTotalAuthenticationsAdded >= 100)
		{
			return (-1);
		}

		/*No locks.    eXosip_lock is already called */
		eXosip_add_authentication_info (geXcontext, zUser, zId, zPassword, zAlgorithm,
										zRealm);

		sprintf (gAuthenticateInfoStruct[yTotalAuthenticationsAdded].user,
				 "%s", zUser);
		sprintf (gAuthenticateInfoStruct[yTotalAuthenticationsAdded].id, "%s",
				 zId);
		sprintf (gAuthenticateInfoStruct[yTotalAuthenticationsAdded].password,
				 "%s", zPassword);
		sprintf (gAuthenticateInfoStruct[yTotalAuthenticationsAdded].realm,
				 "%s", zRealm);

		yTotalAuthenticationsAdded++;
#ifdef DJB_TEST
	}
#endif

	return 0;

}								/*END: arc_add_authentication_info */

///Prints debug info and contains the function that called it, the line, the port, and the time into nohup.out.
void
arc_print_port_info (const char *function, int module, int zLine,
					 int zCall, char *zData1, char *zData2, int zData3)
{
	struct timeval	tb;
	char            t[100];
	char           *tmpTime;
	char            yStrCTime[256];

	if (!isModuleEnabled (module))
	{
		return;
	}

	//ftime (&tb);
	gettimeofday(&tb, NULL);

//	tmpTime = ctime_r (&(tb.tv_sec), yStrCTime);
	tmpTime = ctime (&(tb.tv_sec));

	sprintf (t, "%s", tmpTime);

	t[strlen (t) - 1] = '\0';

	if (zCall < gStartPort || zCall > gEndPort)
	{
		printf ("ARCDEBUG:%d:%s:%d:State_%d:%s:%s:%d:%s:%d:%s:%s:%d\n",
				gDynMgrId,
				gDebugModules[module].label, zCall, -1, __FILE__,
				function, zLine, t, tb.tv_usec, zData1, zData2, zData3);
	}
	else
	{
		printf ("ARCDEBUG:%d:%s:%d:State_%d:%s:%s:%d:%s:%d:%s:%s:%d\n",
				gDynMgrId,
				gDebugModules[module].label, zCall,
				gCall[zCall].callState, __FILE__, function, zLine, t,
				tb.tv_usec, zData1, zData2, zData3);
	}

	fflush (stdout);

}								/*END: void arc_printf */

///This function frees the memory pointed to by zPts and writes a debug message in nohup.out.
void
arc_free (int zCall, char *zMod, int zLine, void *zPtr)
{
	time_t          yTm;

	time (&yTm);

	gFreeCount++;

	arc_print_port_info (zMod, DEBUG_MODULE_MEMORY, zLine, zCall,
						 "Free count", "", gFreeCount);

	osip_free (zPtr);

}								/*END: void arc_free */

///This function mallocs a specific amount of memory and writes a debug message in nohup.out.
void           *
arc_malloc (int zCall, char *zMod, int zLine, int zSize)
{
	void           *lpData = NULL;

	time_t          yTm;

	time (&yTm);

	lpData = osip_malloc (zSize);

	gMallocCount++;

	arc_print_port_info (zMod, DEBUG_MODULE_MEMORY, zLine, zCall,
						 "Malloc count", "", gMallocCount);

	return (lpData);

}								/*END: void * arc_malloc */

///This function will unlink the file pointed to by zFile.
int
arc_unlink (int zCall, char *mod, char *zFile)
{
	__DDNDEBUG__ (DEBUG_MODULE_FILE, "Unlinking", zFile, zCall);

	return (unlink (zFile));

}								/*END: int arc_unlink */

///This function is used to write to Telecom Monitor's shared memory.
int
writeToRespShm (int zLine, char *mod, void *zpBasePtr, int rec, int ind, int val)
{
	char            tmp[30];
	char           *ptr = (char *)zpBasePtr;
	int             zCall = rec;

//__DDNDEBUG__(DEBUG_MODULE_CALL, mod, mod, rec);

	if (rec < 0 || ind < 0 || val < 0)
	{
		dynVarLog (__LINE__, rec, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "WriteToRespShm(base_ptr, %d, %d, %d) failed. Invalid "
				   "value/not supported.", rec, ind, val);
		return (0);
	}

	//pthread_mutex_lock (&gRespShmMutex);
//	if ( ind == APPL_PID )
//	{
//		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//			"[%d] DJB: ind=APPL_PID=%d, val=%d",  zLine, ind, val);
//	}
			

	switch (ind)
	{
	case APPL_API:				/* current api # */
	case APPL_START_TIME:		/* application start time */
	case APPL_NAME:			/* application name */
	case APPL_STATUS:			/* status of the application */
	case APPL_PID:				/* pid */
	case APPL_FIELD5:			/* turnkey handshake */
	case APPL_SIGNAL:			/* signal to the application */
		ptr += (rec * SHMEM_REC_LENGTH);
		/* position the pointer to the field */
		ptr += vec[ind - 1];
		sprintf (tmp, "%*d", fld_siz[ind - 1], val);
		(void) memcpy (ptr, tmp, strlen (tmp));
		break;

	default:
		dynVarLog (__LINE__, rec, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "WriteToRespShm(base_ptr, %d, %d, %d) failed: index (%d) not supported.",
				   rec, ind, val, ind);
		break;
	}

	//pthread_mutex_unlock (&gRespShmMutex);

	return (0);

}/*END: int writeToRespShm */

int
checkIfVXIPresent ()
{
static char     mod[] = "checkIfVXIPresent";
char            yStrCheckCommand[256];
FILE           *fin = NULL;
char            buf[BUFSIZE];

	sprintf (yStrCheckCommand, "%s", "./arcVXML2 -v");

	if (access ("arcVXML2", F_OK) != 0)
	{
		return 0;
	}

	if ((fin = popen (yStrCheckCommand, "r")) == NULL)
	{
		return 0;
	}

	fgets (buf, sizeof buf, fin);

	(void) pclose (fin);
	fin = NULL;

	if (buf[0] && strstr (buf, "VXI") != NULL)
	{
		return (1);
	}
	else
	{
		return (0);
	}

}/*END: int checkIfVXIPresent */

int
parseOcnRdn (int zCall, char *sip_div, char *zResult)
{

	char            mod[] = "parseOcnRdn";
	char           *start_point = NULL;
	char           *end_point = NULL;
	int             start_len = 0;
	int             end_len = 0;
	int             isSip = 0;

	start_point = strstr (sip_div, "<tel:");
	if (start_point == NULL)
	{
		start_point = strstr (sip_div, "<sip:");
		isSip = 1;
	}

	if (start_point == NULL)
	{
		__DDNDEBUG__ (DEBUG_MODULE_CALL, "No tel or sip format in", sip_div,
					  zCall);
		return -1;
	}

	if (isSip == 1)
	{
		end_point = strstr (start_point, "@");
	}
	else
	{
		end_point = strstr (start_point, ">");
	}

	if (end_point == NULL)
	{
		__DDNDEBUG__ (DEBUG_MODULE_CALL, "No tel or sip format in", sip_div,
					  zCall);
		return -1;
	}
	start_len = strlen (start_point);
	end_len = strlen (end_point);

	memcpy (zResult, start_point + 5, start_len - end_len - 5);

	return 0;

}/*END: int parseOcnRdn */

int
setOcnRdn (int zCall, osip_message_t * zInvite)
{
	char            mod[] = { "setOcnRdn" };

	int             yCurrentPos = 0;
	int             yLoopCounter = 0;

	char            yStrOcn[256];
	char            yStrRdn[256];
	char            yStrLast[256];

	memset (yStrOcn, 0, 256);
	memset (yStrRdn, 0, 256);
	memset (yStrLast, 0, 256);

	osip_header_t  *lpHeader = NULL;

	while (1)
	{
		yCurrentPos =
			osip_message_header_get_byname (zInvite, "Diversion", yCurrentPos,
											&lpHeader);

		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Pos", "", yCurrentPos);

		if (lpHeader == NULL)
		{
			break;
		}

		if (yLoopCounter == 0)
		{
			/*Set RDN */
			sprintf (yStrRdn, "%s", osip_header_get_value (lpHeader));
		}

		/*Set Last */
		sprintf (yStrLast, "%s", osip_header_get_value (lpHeader));

		yLoopCounter++;
		yCurrentPos++;

		if (yLoopCounter > 10)
		{
			break;
		}
	}

	if (yStrLast[0])
	{
		/*Set OCN */
		sprintf (yStrOcn, "%s", yStrLast);
	}

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "OCN", yStrOcn, zCall);
	__DDNDEBUG__ (DEBUG_MODULE_CALL, "RDN", yStrRdn, zCall);

	parseOcnRdn (zCall, yStrOcn, gCall[zCall].ocn);
	parseOcnRdn (zCall, yStrRdn, gCall[zCall].rdn);

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "OCN", gCall[zCall].ocn, zCall);
	__DDNDEBUG__ (DEBUG_MODULE_CALL, "RDN", gCall[zCall].rdn, zCall);

#if 0
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "OCN=%s, RDN=%s", gCall[zCall].ocn, gCall[zCall].rdn);
#endif

	return (0);

}/*END: setOcnRdn */

///This function creates the sdp body for outgoing SIP INVITEs.
/**
Starting with eXosip2 the creation and population of an SDP body is not done
by the stack.  We therefore need to build a valid sdp body and attach this
body to any outgoing messages.  This is because eXosip2 is completely media
independent.  This function only needs to know what port SIPIVR will be
receving audio on (zStrAudioPort), what DTMF payload type will be used 
(zTelephonyPayload), and what voice codec this call will be using 
(zVoiceCodec).
*/
int
createInitiateSdpBody (int zCall, char zStrAudioPort[15],
					   int zVoiceCodec, int zVideoCodec,
					   int zTelephonyPayload)
{
	char            mod[] = "createInitiateSdpBody";
	char            tmp[4096];
	char            localip[128];
	char            yStrVoiceCodec[100];
	char            yStrTelephonyPayload[100];
	int             yIntVersionNumber = 0;
	char           *ip_version = "IP4";
    struct arc_sdp_t   arc_sdp;

	yIntVersionNumber = zCall + 1997;

	//eXosip_guess_localip (AF_INET, localip, 128);

	sprintf (localip, "%s", gHostIp);

	if (gEnableIPv6Support != 0)
	{
		ip_version = "IP6";
	}

	switch (zVoiceCodec)
	{
	case 111:
		sprintf (yStrVoiceCodec, "speex/8000");
		break;

	case 110:
		sprintf (yStrVoiceCodec, "speex/16000");
		break;

	case 18:
		sprintf (yStrVoiceCodec, "G729/8000");
		break;

	case 8:
		sprintf (yStrVoiceCodec, "8 PCMA/8000");
		break;

	case 9:
		sprintf (yStrVoiceCodec, "G722/8000");
		break;

	case 3:
		sprintf (yStrVoiceCodec, "8 PCMA/8000");
		break;

	case 0:

	default:
		sprintf (yStrVoiceCodec, "0 PCMU/8000");
		break;
	}

	if (zVoiceCodec == gCall[zCall].amrIntCodec[0])
	{
		sprintf (yStrVoiceCodec, "%d AMR/8000", gCall[zCall].amrIntCodec[0]);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "gPreferredCodec=%s, GV_PreferredCodec=%s",
			   gPreferredCodec, gCall[zCall].GV_PreferredCodec);

	if (gCall[zCall].GV_PreferredCodec != NULL &&
		gCall[zCall].GV_PreferredCodec[0] != '\0')
	{
		if (strstr (gCall[zCall].GV_PreferredCodec, "G729") != NULL)
		{
			zVoiceCodec = 18;
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "PCMU") != NULL)
		{
			zVoiceCodec = 0;
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "PCMA") != NULL)
		{
			zVoiceCodec = 8;
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "GSM") != NULL)
		{
			zVoiceCodec = 3;
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "AMR") != NULL ||
				 strstr (gCall[zCall].GV_PreferredCodec, "amr") != NULL)
		{
			zVoiceCodec = gCall[zCall].amrIntCodec[0];
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "SPEEX") != NULL)
		{
			if (strstr (gCall[zCall].GV_PreferredCodec, "8000"))
			{
				zVoiceCodec = 110;
			}
			else
			{
				zVoiceCodec = 111;
			}
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "G722") != NULL)
		{
			zVoiceCodec = 9;
		}
	}
	else if (gPreferredCodec != NULL && gPreferredCodec[0] != '\0')
	{
		char	yRhs[8] = "";
		char	yLhs[8] = "";

		sscanf (gPreferredCodec, "%[^,]=%[^=]", yLhs, yRhs);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "yLhs=%s, yRhs=%s", yLhs, yRhs);

		if (strstr (yLhs, "G729") != NULL)
		{
			zVoiceCodec = 18;
		}
		else if (strstr (yLhs, "PCMU") != NULL)
		{
			zVoiceCodec = 0;
		}
		else if (strstr (yLhs, "PCMA") != NULL)
		{
			zVoiceCodec = 8;
		}
		else if (strstr (yLhs, "GSM") != NULL)
		{
			zVoiceCodec = 3;
		}
		else if (strstr (yLhs, "AMR") != NULL || strstr (yLhs, "amr") != NULL)
		{
			zVoiceCodec = gCall[zCall].amrIntCodec[0];
		}
		else if (strstr (yLhs, "SPEEX") != NULL)
		{
			if (strstr (yLhs, "8000"))
			{
				zVoiceCodec = 110;
			}
			else
			{
				zVoiceCodec = 111;
			}
		}
		else if (strstr (yLhs, "G722") != NULL)
		{
			zVoiceCodec = 0;
		}
	}

	sprintf (yStrTelephonyPayload, "%d telephone-event/8000", zTelephonyPayload);

	if (zVoiceCodec == -1)
	{
		arc_sdp_init(&arc_sdp);
	    arc_sdp_push(&arc_sdp, "v=0\r\n");
		arc_sdp_push(&arc_sdp, "o=ArcSipIvr 2005 %d IN %s %s\r\n", yIntVersionNumber, ip_version, localip);
		arc_sdp_push(&arc_sdp, "s=SIP Call\r\n"); 
        arc_sdp_push(&arc_sdp, "c=IN %s %s\r\n", ip_version, localip);
        arc_sdp_push(&arc_sdp, "t=0 0\r\n");
        if(zTelephonyPayload <= 0){
		  arc_sdp_push(&arc_sdp, "m=audio %s RTP/AVP 18 0 8 3 110 111\r\n", zStrAudioPort);
        } else {
          arc_sdp_push(&arc_sdp, "m=audio %s RTP/AVP 18 0 8 3 110 111 %d %d\r\n", 
                     zStrAudioPort, 
                     gCall[zCall].telephonePayloadType, 
                     gCall[zCall].telephonePayloadType_2);
        }
		arc_sdp_push(&arc_sdp, "a=rtpmap:18 G729/8000\r\n");
		arc_sdp_push(&arc_sdp, "a=rtpmap:0 PCMU/8000\r\n");
		arc_sdp_push(&arc_sdp, "a=rtpmap:8 PCMA/8000\r\n");
		arc_sdp_push(&arc_sdp, "a=rtpmap:3 GSM/8000\r\n");
		arc_sdp_push(&arc_sdp, "a=rtpmap:110 speex/8000\r\n");
		arc_sdp_push(&arc_sdp, "a=rtpmap:111 speex/16000\r\n");
		arc_sdp_push(&arc_sdp, "a=rtpmap:9 G722/8000\r\n");
        if(zTelephonyPayload > 0){
		  arc_sdp_push(&arc_sdp, "a=rtpmap:96 telephone-event/8000\r\n");
		  arc_sdp_push(&arc_sdp, "a=rtpmap:101 telephone-event/8000\r\n");
        }
		arc_sdp_finalize(&arc_sdp, tmp, sizeof(tmp));

/**************************************************************************************

until we are sure we have working code this is the old stuff 

		sprintf (tmp,
				 "v=0\r\n"
				 "o=ArcSipIvr 2005 %d IN %s %s\r\n"
				 "s=SIP Call\r\n" "c=IN %s %s\r\n" "t=0 0\r\n"
				 //"m=audio %s RTP/AVP 18 0 8 3 110 111 101 96\r\n"
				 "m=audio %s RTP/AVP 18 0 8 3 110 111 %d %d\r\n"
				 "a=rtpmap:18 G729/8000\r\n"
				 "a=rtpmap:0 PCMU/8000\r\n"
				 "a=rtpmap:8 PCMA/8000\r\n"
				 "a=rtpmap:3 GSM/8000\r\n"
				 "a=rtpmap:110 speex/8000\r\n"
				 "a=rtpmap:111 speex/16000\r\n"
				 "a=rtpmap:9 G722/8000\r\n"
				 "a=rtpmap:96 telephone-event/8000\r\n"
				 "a=rtpmap:101 telephone-event/8000\r\n",
				 yIntVersionNumber,
				 ip_version,
				 localip,
				 ip_version,
				 localip,
				 zStrAudioPort,
				 gCall[zCall].telephonePayloadType,
				 gCall[zCall].telephonePayloadType_2);

***************************************************************************************/

	}
	else
	{
		switch (zVoiceCodec)
		{

		case 4:
			sprintf (tmp, 
                     "v=0\r\n" 
                     "o=ArcSipIvr 2005 %d IN %s %s\r\n" 
                     "s=SIP Call\r\n" 
                     "c=IN %s %s\r\n" 
                     "t=0 0\r\n" 
                     "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
					 "a=rtpmap:%s\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 zVoiceCodec, zTelephonyPayload, yStrTelephonyPayload);

			break;

		case 18:

			sprintf (tmp, 
                     "v=0\r\n" 
                     "o=ArcSipIvr 2005 %d IN %s %s\r\n" 
                     "s=SIP Call\r\n" "c=IN %s %s\r\n"  
                     "t=0 0\r\n" 
                     "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
					 "a=rtpmap:18 G729/8000\r\n"
					 "a=rtpmap:%s\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 zVoiceCodec, zTelephonyPayload, yStrTelephonyPayload);

			break;

		case 9:

			sprintf (tmp, 
                     "v=0\r\n" 
                     "o=ArcSipIvr 2005 %d IN IP4 %s\r\n" 
                     "s=SIP Call\r\n" "c=IN IP4 %s\r\n" 
                     "t=0 0\r\n"  
                     "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
					 "a=rtpmap:9 G722/8000\r\n"
					 "a=rtpmap:%s\r\n",
					 yIntVersionNumber,
					 localip,
					 localip,
					 zStrAudioPort,
					 zVoiceCodec, zTelephonyPayload, yStrTelephonyPayload);


			break;

		default:

           /*
             Let's eventually switch to using this routine as it allows a dynamic 
             check for features turned on and off as needed like inband DTMF. 
           */

            int size = 0;

			arc_sdp_init(&arc_sdp);
			arc_sdp_push(&arc_sdp, "v=0\r\n");
			arc_sdp_push(&arc_sdp, "o=ArcSipIvr 2005 %d IN %s %s\r\n", yIntVersionNumber, ip_version, localip);
			arc_sdp_push(&arc_sdp, "s=SIP Call\r\n");
		    arc_sdp_push(&arc_sdp, "c=IN %s %s\r\n", ip_version, localip);
		    arc_sdp_push(&arc_sdp, "t=0 0\r\n");
            // if not set then advertise inband --  
            if(zTelephonyPayload > 0){
		       arc_sdp_push(&arc_sdp, "m=audio %s RTP/AVP %d %d\r\n", zStrAudioPort, zVoiceCodec, zTelephonyPayload); 
			   arc_sdp_push(&arc_sdp, "a=rtpmap:%s\r\n", yStrVoiceCodec);
			   arc_sdp_push(&arc_sdp, "a=rtpmap:%s\r\n", yStrTelephonyPayload);
            } else {
		       arc_sdp_push(&arc_sdp, "m=audio %s RTP/AVP %d\r\n", zStrAudioPort, zVoiceCodec); 
			   arc_sdp_push(&arc_sdp, "a=rtpmap:%s\r\n", yStrVoiceCodec);
            }
            size = arc_sdp_finalize(&arc_sdp, tmp, sizeof(tmp));


/***************************************************************************************

until  we are sure that this is workeing well, this is the old code 

			sprintf (tmp,
					 "v=0\r\n"
					 "o=ArcSipIvr 2005 %d IN %s %s\r\n"
					 "s=SIP Call\r\n"
					 "c=IN %s %s\r\n"
					 "t=0 0\r\n"
					 "m=audio %s RTP/AVP %d %d\r\n"
					 "a=rtpmap:%s\r\n"
					 "a=rtpmap:%s\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 zVoiceCodec,
					 zTelephonyPayload, yStrVoiceCodec, yStrTelephonyPayload);

****************************************************************************************/

			break;

		}
	}

	sprintf (gCall[zCall].sdp_body_local, "%s", tmp);

	tmp[0] = 0;

	return 0;

}/*END: int createInitiateSdpBody */

///This function creates the sdp body for outgoing SIP INVITEs.
/**
Starting with eXosip2 the creation and population of an SDP body is not done
by the stack.  We therefore need to build a valid sdp body and attach this
body to any outgoing messages.  This is because eXosip2 is completely media
independent.  This function only needs to know what port SIPIVR will be
receving audio on (zStrAudioPort), what DTMF payload type will be used 
(zTelephonyPayload), and what voice codec this call will be using 
(zVoiceCodec).
*/
int
createListenSdpBody (int zCall, char zStrAudioPort[15],
					 int zVoiceCodec, int zVideoCodec, int zTelephonyPayload)
{
	char            mod[] = "createInitiateSdpBody";
	char            tmp[4096];
	char            localip[128];
	char            yStrVoiceCodec[100];
	char            yStrTelephonyPayload[100];
	int             yIntVersionNumber = 0;
	char           *ip_version = "IP4";
    struct arc_sdp_t  arc_sdp;

	yIntVersionNumber = zCall + 1997;

	//eXosip_guess_localip (AF_INET, localip, 128);

	if (gEnableIPv6Support != 0)
	{
		ip_version = "IP6";
	}

	sprintf (localip, "%s", gHostIp);

	switch (zVoiceCodec)
	{
	case 111:
		sprintf (yStrVoiceCodec, "speex/8000");
		break;

	case 110:
		sprintf (yStrVoiceCodec, "speex/16000");
		break;

	case 18:
		sprintf (yStrVoiceCodec, "G729/8000");
		break;

	case 8:
		sprintf (yStrVoiceCodec, "8 PCMA/8000");
		break;

	case 9:
		sprintf (yStrVoiceCodec, "G722/8000");
		break;

	case 3:
		sprintf (yStrVoiceCodec, "8 PCMA/8000");
		break;

	case 0:

	default:
		sprintf (yStrVoiceCodec, "0 PCMU/8000");
		break;
	}

	if (zVoiceCodec == gCall[zCall].amrIntCodec[0])
	{
		sprintf (yStrVoiceCodec, "%d AMR/8000", gCall[zCall].amrIntCodec[0]);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "gPreferredCodec=%s, GV_PreferredCodec=%s",
			   gPreferredCodec, gCall[zCall].GV_PreferredCodec);

	if (gCall[zCall].GV_PreferredCodec != NULL &&
		gCall[zCall].GV_PreferredCodec[0] != '\0')
	{
		if (strstr (gCall[zCall].GV_PreferredCodec, "G729") != NULL)
		{
			zVoiceCodec = 18;
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "PCMU") != NULL)
		{
			zVoiceCodec = 0;
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "PCMA") != NULL)
		{
			zVoiceCodec = 8;
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "GSM") != NULL)
		{
			zVoiceCodec = 3;
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "AMR") != NULL ||
				 strstr (gCall[zCall].GV_PreferredCodec, "amr") != NULL)
		{
			zVoiceCodec = gCall[zCall].amrIntCodec[0];
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "SPEEX") != NULL)
		{
			if (strstr (gCall[zCall].GV_PreferredCodec, "8000"))
			{
				zVoiceCodec = 110;
			}
			else
			{
				zVoiceCodec = 111;
			}
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "G722") != NULL)
		{
			zVoiceCodec = 9;
		}
	}
	else if (gPreferredCodec != NULL && gPreferredCodec[0] != '\0')
	{
	char            yRhs[8] = "";
	char            yLhs[8] = "";

		sscanf (gPreferredCodec, "%[^,]=%[^=]", yLhs, yRhs);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "yLhs=%s, yRhs=%s", yLhs, yRhs);

		if (strstr (yLhs, "G729") != NULL)
		{
			zVoiceCodec = 18;
		}
		else if (strstr (yLhs, "PCMU") != NULL)
		{
			zVoiceCodec = 0;
		}
		else if (strstr (yLhs, "PCMA") != NULL)
		{
			zVoiceCodec = 8;
		}
		else if (strstr (yLhs, "GSM") != NULL)
		{
			zVoiceCodec = 3;
		}
		else if (strstr (yLhs, "AMR") != NULL || strstr (yLhs, "amr") != NULL)
		{
			zVoiceCodec = gCall[zCall].amrIntCodec[0];
		}
		else if (strstr (yLhs, "SPEEX") != NULL)
		{
			if (strstr (yLhs, "8000"))
			{
				zVoiceCodec = 110;
			}
			else
			{
				zVoiceCodec = 111;
			}
		}
		else if (strstr (yLhs, "G722") != NULL)
		{
			zVoiceCodec = 9;
		}
	}

	sprintf (yStrTelephonyPayload, "%d telephone-event/8000", zTelephonyPayload);

	if (zVoiceCodec == -1)
	{
		sprintf (tmp,
				 "v=0\r\n"
				 "o=ArcSipIvr 2005 %d IN %s %s\r\n"
				 "s=SIP Call\r\n" "c=IN %s %s\r\n" "t=0 0\r\n"
				 //"m=audio %s RTP/AVP 18 0 8 3 110 111 101 96\r\n"
				 "m=audio %s RTP/AVP 18 0 8 3 110 111 %d %d\r\n"
				 "a=rtpmap:18 G729/8000\r\n"
				 "a=rtpmap:0 PCMU/8000\r\n"
				 "a=rtpmap:8 PCMA/8000\r\n"
				 "a=rtpmap:9 G722/8000\r\n"
				 "a=rtpmap:3 GSM/8000\r\n"
				 "a=rtpmap:110 speex/8000\r\n"
				 "a=rtpmap:111 speex/16000\r\n"
				 "a=rtpmap:96 telephone-event/8000\r\n"
				 "a=rtpmap:101 telephone-event/8000\r\n"
				 "a=sendonly\r\n",
				 yIntVersionNumber,
				 ip_version,
				 localip,
				 ip_version,
				 localip,
				 zStrAudioPort,
				 gCall[zCall].telephonePayloadType,
				 gCall[zCall].telephonePayloadType_2);
	}
	else
	{
		switch (zVoiceCodec)
		{

		case 4:
			sprintf (tmp, 
                     "v=0\r\n" 
                     "o=ArcSipIvr 2005 %d IN %s %s\r\n" 
                     "s=SIP Call\r\n" "c=IN %s %s\r\n" 
                     "t=0 0\r\n" 
                     "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
					 "a=rtpmap:%s\r\n"
					 "a=sendonly\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 zVoiceCodec, zTelephonyPayload, yStrTelephonyPayload);

			break;

		case 18:

			sprintf (tmp, 
                     "v=0\r\n" 
                     "o=ArcSipIvr 2005 %d IN %s %s\r\n" 
                     "s=SIP Call\r\n" 
                     "c=IN %s %s\r\n" 
                     "t=0 0\r\n" 
                     "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
					 "a=rtpmap:18 G729/8000\r\n"
					 "a=rtpmap:%s\r\n"
					 "a=sendonly\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 zVoiceCodec, zTelephonyPayload, yStrTelephonyPayload);

			break;

		case 9:

			sprintf (tmp, 
                     "v=0\r\n" 
                     "o=ArcSipIvr 2005 %d IN IP4 %s\r\n" 
                     "s=SIP Call\r\n" "c=IN IP4 %s\r\n" 
                     "t=0 0\r\n" 
                     "m=audio %s RTP/AVP %d %d\r\n"	// <<------------ +5
					 "a=rtpmap:9 G722/8000\r\n"
					 "a=rtpmap:%s\r\n"
					 "a=sendonly\r\n",
					 yIntVersionNumber,
					 localip,
					 localip,
					 zStrAudioPort,
					 zVoiceCodec, zTelephonyPayload, yStrTelephonyPayload);

			break;

		default:

			arc_sdp_init(&arc_sdp);
		    arc_sdp_push(&arc_sdp, "v=0\r\n");
			arc_sdp_push(&arc_sdp, "o=ArcSipIvr 2005 %d IN %s %s\r\n", yIntVersionNumber, ip_version, localip);
		    arc_sdp_push(&arc_sdp, "s=SIP Call\r\n");
		    arc_sdp_push(&arc_sdp, "c=IN %s %s\r\n", ip_version, localip);
			arc_sdp_push(&arc_sdp, "t=0 0\r\n");
		    arc_sdp_push(&arc_sdp, "a=rtpmap:%s\r\n", yStrVoiceCodec);
            if(zTelephonyPayload > 0){
		      arc_sdp_push(&arc_sdp, "m=audio %s RTP/AVP %d %d\r\n", zStrAudioPort, zVoiceCodec, zTelephonyPayload);
			  arc_sdp_push(&arc_sdp, "a=rtpmap:%s\r\n", yStrTelephonyPayload);
            } else {
		      arc_sdp_push(&arc_sdp, "m=audio %s RTP/AVP %d\r\n", zStrAudioPort, zVoiceCodec);
            }
		    arc_sdp_push(&arc_sdp, "a=sendonly\r\n",yStrVoiceCodec);
            arc_sdp_finalize(&arc_sdp, tmp, sizeof(tmp));


#if 0
			sprintf (tmp,
					 "v=0\r\n"
					 "o=ArcSipIvr 2005 %d IN %s %s\r\n"
					 "s=SIP Call\r\n"
					 "c=IN %s %s\r\n"
					 "t=0 0\r\n"
					 "m=audio %s RTP/AVP %d %d\r\n"
					 "a=rtpmap:%s\r\n"
					 "a=rtpmap:%s\r\n"
					 "a=sendonly\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 zVoiceCodec,
					 zTelephonyPayload, yStrVoiceCodec, yStrTelephonyPayload);
#endif 

			break;

		}
	}

	sprintf (gCall[zCall].sdp_body_local, "%s", tmp);

	return 0;

}/*END: int createListenSdpBody */

// begin old aculab def #ifdef ACU_LINUX

/*---------------------------------------------------------------------------
  ---------------------------------------------------------------------------*/
int
createFaxSdpBody (int zCall,
				  char zStrAudioPort[15],
				  int zVoiceCodec, int zVideoCodec, int zTelephonyPayload)
{
	char            mod[] = "createFaxSdpBody";
	char            tmp[4096];
	char            localip[128];
	char            yStrVoiceCodec[100];
	char            yStrTelephonyPayload[100];
	int             yIntVersionNumber = 0;

	char            tcfMode[25] = "localTCF";
	char            udptlMode[25] = "t38UDPFEC";

	switch (gT38ErrorCorrectionMode)
	{
	case 0:
		snprintf (udptlMode, sizeof (udptlMode), "%s", "None");
		break;
	case 1:
		snprintf (udptlMode, sizeof (udptlMode), "%s", "t38UDPRedundancy");
		break;
	case 2:
		snprintf (udptlMode, sizeof (udptlMode), "%s", "t38UDPFEC");
		break;
	default:
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
				   TEL_INVALID_INTERNAL_VALUE, ERR,
				 "Hit default case in determining gT38ErrorCorrectionMode value=%d\n",
				 gT38ErrorCorrectionMode);
	}

	switch (gT38FaxRateManagement)
	{
	case 1:
		snprintf (tcfMode, sizeof (tcfMode), "localTCF");
		break;
	case 2:
		snprintf (tcfMode, sizeof (tcfMode), "transferredTCF");
		break;
	default:
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
				   TEL_INVALID_INTERNAL_VALUE, ERR,
				 "hit default case in determining gT38FaxRateManagement value=%d",
				 gT38FaxRateManagement);
	}

	if (gCall[zCall].faxRtpPort <= 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
				   TEL_INVALID_INTERNAL_VALUE, ERR,
				   "Invalid faxRtpPort(%d), not creating SDP.",
				   gCall[zCall].faxRtpPort);
		return (-1);
	}

	yIntVersionNumber = zCall + 1997;

    if(gFaxSdpIp[0] != '\0'){
	   sprintf (localip, "%s", gFaxSdpIp);
    } else {
	   sprintf (localip, "%s", gHostIp);
    }

    // joes Wed Dec 17 11:54:48 EST 2014

    if(gCall[zCall].faxData.version == -1){
      gCall[zCall].faxData.version = 0;
    }

	sprintf (tmp,
			 "v=0\r\n"
			 "o=ArcSipIvr 2005 %d IN IP4 %s\r\n"
			 "s=SIP Call\r\n"
			 "c=IN IP4 %s\r\n"
			 "t=0 0\r\n"
			 "m=image %d udptl t38\r\n"
			 "c=IN IP4 %s\r\n"
			 "a=T38FaxVersion:%d\r\n"
			 "a=T38MaxBitRate:%d\r\n"
			 "a=T38FaxRateManagement:%s\r\n" "a=T38FaxUdpEC:%s\r\n"
			 //"a=T38FaxMaxBufferSize:%d\r\n"
			 "a=T38FaxMaxBuffer:%d\r\n" "a=T38FaxMaxDatagram:%d\r\n"
			 //"a=T38MaxDatagram:%d\r\n"
			 "a=T38FaxFillBitRemoval:0\r\n"
			 "a=T38FaxTranscodingMMR:%d\r\n"
			 "a=T38FaxTranscodingJBIG:0\r\n",
			 yIntVersionNumber, localip, localip,
			 //zStrAudioPort,
			 gCall[zCall].faxRtpPort,
			 localip,
			 gCall[zCall].faxData.version,
			 gT38MaxBitRate,
			 tcfMode,
			 udptlMode,
			 gT38FaxMaxBuffer,
			 gT38FaxMaxDataGram, gCall[zCall].faxData.transcodingMMR);

	sprintf (gCall[zCall].sdp_body_fax_local, "%s", tmp);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
			   "Built fax sdp (%s)", tmp);
	return (0);

}/*END: int createFaxSdpBody */

int
fnSend200OKForFax (int zCall)
{

	char            mod[] = "fnSend200OKForFax";
	int             yResponse = 200;
	int             yRc = 0;
	char           *message = NULL;
	int             i;
	size_t          length = 0;
	int             tid;

	osip_message_t *reInviteAnswer = NULL;

	eXosip_lock (geXcontext);

	if (gCall[zCall].reinvite_tid != -1)
	{
		tid = gCall[zCall].reinvite_tid;
	}
	else
	{
		tid = gCall[zCall].tid;
	}
	yRc = eXosip_call_build_answer (geXcontext, tid, 200, &reInviteAnswer);

	if (yRc < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_SIP_SIGNALING,
				   ERR,
				   "eXosip_call_build_answer() failed.  rc=%d.  Unable to build %d to "
				   "re-INVITE message (tid:%d).  No 200 OK will be sent.",
				   yRc, yResponse, tid);
	}
	else
	{
		if (gCall[zCall].faxData.isFaxCall == 1)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
					   "Attaching fax sdp to %d OK of REINVITE.", yResponse);

			yRc = createFaxSdpBody (zCall, "", 0, 0, 101);

			osip_message_set_body (reInviteAnswer,
								   gCall[zCall].sdp_body_fax_local,
								   strlen (gCall[zCall].sdp_body_fax_local));
			osip_message_set_content_type (reInviteAnswer, "application/sdp");
		}
		/*END: DDN_FAX: 02/25/2010 */
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
					   "Sending SIP Message %d.", yResponse);

			/*Send the old SDM info. TBD: Check of changes in the media */
			osip_message_set_body (reInviteAnswer,
								   gCall[zCall].sdp_body_local,
								   strlen (gCall[zCall].sdp_body_local));
			osip_message_set_content_type (reInviteAnswer, "application/sdp");
		}

	    arc_fixup_contact_header (reInviteAnswer, gContactIp, gSipPort, zCall);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
					   "Sending SIP Message %d.", yResponse);
		yRc = eXosip_call_send_answer (geXcontext, tid, yResponse, reInviteAnswer);

		if (yRc < 0)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_SIP_SIGNALING,
					   ERR,
					   "eXosip_call_send_answer() failed. rc=%d.  "
					   "Unable to send %d to re-INVITE message (tid:%d).",
					   yRc, yResponse, tid);
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
					   "Sent %d to re-INVITE (tid:%d).", yResponse, tid);
		}
	}

	eXosip_unlock (geXcontext);
	gCall[zCall].send200OKForFax = 0;

	return (0);

}//fnSend200OKForFax

/*---------------------------------------------------------------------------
 ---------------------------------------------------------------------------*/
int
fnSendFaxReInvite (int zCall)
{
	char            mod[] = "sendFaxReInvite";
	char            yStrTmpSdpPort[15] = "";
	int             yRc = -1;
	osip_message_t *invite = NULL;

	sprintf (yStrTmpSdpPort, "%d", gCall[zCall].localSdpPort);

	gCall[zCall].faxData.version = gT38FaxVersion;
	gCall[zCall].faxData.bitrate = gT38MaxBitRate;
	gCall[zCall].faxData.transcodingMMR = gT38FaxTranscodingMMR;

	eXosip_lock (geXcontext);
	yRc = eXosip_call_build_request (geXcontext, gCall[zCall].did, "INVITE", &invite);

	if (yRc < 0)
	{
		eXosip_unlock (geXcontext);

		return (-1);
	}
	eXosip_unlock (geXcontext);

	yRc = createFaxSdpBody (zCall, yStrTmpSdpPort, 0, 0, 101);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
			   "FaxMesaageBody set to (%s)", gCall[zCall].sdp_body_fax_local);

	arc_fixup_contact_header (invite, gContactIp, gSipPort, zCall);
	osip_message_set_body (invite, gCall[zCall].sdp_body_fax_local,
						   strlen (gCall[zCall].sdp_body_fax_local));
	osip_message_set_content_type (invite, "application/sdp");

#if 0
	osip_list_remove (&invite->vias, 0);
	osip_message_set_via (invite, gCall[zCall].viaInfo);
	osip_cseq_set_number (invite->cseq, "2");
	osip_cseq_set_method (invite->cseq, "INVITE");

	if (gCall[zCall].callIdHeader != NULL)
	{
		osip_call_id_free (invite->call_id);
		invite->call_id = NULL;

		yRc =
			osip_message_set_call_id (invite,
									  (const char *) &(gCall[zCall].
													   callIdHeader));

		if (yRc < 0)
		{
			return (-1);
		}
	}
#endif

	eXosip_lock (geXcontext);
	yRc = eXosip_call_send_request (geXcontext, gCall[zCall].did, invite);
	eXosip_unlock (geXcontext);

	if (yRc < 0)
	{
		return (-1);
	}
	return 0;

}//fnSendFaxReInvite

//  end old aculab #endif

///This function creates the sdp body for outgoing SIP INVITEs.
/**
Starting with eXosip2 the creation and population of an SDP body is not done
by the stack.  We therefore need to build a valid sdp body and attach this
body to any outgoing messages.  This is because eXosip2 is completely media
independent.  This function only needs to know what port SIPIVR will be
receving audio on (zStrAudioPort), what DTMF payload type will be used 
(zTelephonyPayload), and what voice codec this call will be using 
(zVoiceCodec).
*/
int
createSdpBody (int zCall, char zStrAudioPort[15],
			   int zVoiceCodec, int zVideoCodec, int zTelephonyPayload)
{
	char            mod[] = "createSdpBody";

#if 0
	char           *tmp = gCall[zCall].data_buffer;
#endif
	char            tmp[4096];

	char           *ip_version = "IP4";

	char            localip[128];
	char            yStrVoiceCodec[100];
	char            yStrTelephonyPayload[100];
	int             yIntVersionNumber = 0;
    struct arc_sdp_t arc_sdp;

	yIntVersionNumber = zCall + 1997;

#if 0
	if (gCall[zCall].data_buffer == NULL)
	{
		gCall[zCall].data_buffer = (char *) malloc (4096);

		tmp = gCall[zCall].data_buffer;

		if (!tmp)
		{
			return -1;
		}
	}
#endif

	//eXosip_guess_localip (AF_INET, localip, 128);

	if (gEnableIPv6Support != 0)
	{
		ip_version = "IP6 ";
	}

	//sprintf(localip, "%s", gHostIp);
	sprintf (localip, "%s", gSdpIp);

	switch (zVoiceCodec)
	{
	case 111:
		sprintf (yStrVoiceCodec, "speex/8000");
		break;

	case 110:
		sprintf (yStrVoiceCodec, "speex/16000");
		break;

	case 18:
		sprintf (yStrVoiceCodec, "G729/8000");
		break;

	case 8:
		sprintf (yStrVoiceCodec, "8 PCMA/8000");
		break;

	case 9:
		sprintf (yStrVoiceCodec, "G722/8000");
		break;

	case 3:
		sprintf (yStrVoiceCodec, "8 PCMA/8000");
		break;

	case 0:

	default:
		sprintf (yStrVoiceCodec, "0 PCMU/8000");
		break;
	}

	if (zVoiceCodec == gCall[zCall].amrIntCodec[0])
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "amrFmtpParams=(%s).", gCall[zCall].amrFmtpParams);

		if (strlen (gCall[zCall].amrFmtpParams) == 0)
		{
			sprintf (gCall[zCall].amrFmtpParams, "\r\n");
		}

		if (gMaxPTime == 0)
		{
			sprintf (yStrVoiceCodec,
					 "a=rtpmap:%d AMR/8000\r\na=fmtp: %d mode-set=%d; octet-align=1;%s",
					 gCall[zCall].amrIntCodec[0], gCall[zCall].amrIntCodec[0],
					 gCall[zCall].amrIntIndex, gCall[zCall].amrFmtpParams);

		}
		else
		{
			sprintf (yStrVoiceCodec,
					 "a=rtpmap:%d AMR/8000\r\na=fmtp: %d mode-set=%d; octet-align=1;%sa=maxptime:%d\r\n",
					 gCall[zCall].amrIntCodec[0], gCall[zCall].amrIntCodec[0],
					 gCall[zCall].amrIntIndex, gCall[zCall].amrFmtpParams,
					 gMaxPTime);
		}

	}

	if (zVoiceCodec == -1)
	{
		if (gCall[zCall].GV_PreferredCodec != NULL &&
			gCall[zCall].GV_PreferredCodec[0] != '\0')
		{
			if (strstr (gCall[zCall].GV_PreferredCodec, "G729") != NULL)
			{
				zVoiceCodec = 18;
			}
			else if (strstr (gCall[zCall].GV_PreferredCodec, "PCMU") != NULL)
			{
				zVoiceCodec = 0;
			}
			else if (strstr (gCall[zCall].GV_PreferredCodec, "PCMA") != NULL)
			{
				zVoiceCodec = 8;
			}
			else if (strstr (gCall[zCall].GV_PreferredCodec, "GSM") != NULL)
			{
				zVoiceCodec = 3;
			}
			else if (strstr (gCall[zCall].GV_PreferredCodec, "AMR") != NULL ||
					 strstr (gCall[zCall].GV_PreferredCodec, "amr") != NULL)
			{
				zVoiceCodec = gCall[zCall].amrIntCodec[0];
			}
			else if (strstr (gCall[zCall].GV_PreferredCodec, "SPEEX") != NULL)
			{
				if (strstr (gCall[zCall].GV_PreferredCodec, "8000"))
				{
					zVoiceCodec = 110;
				}
				else
				{
					zVoiceCodec = 111;
				}
			}
			else if (strstr (gCall[zCall].GV_PreferredCodec, "G722") != NULL)
			{
				zVoiceCodec = 9;
			}
		}
		else if (gPreferredCodec != NULL && gPreferredCodec[0] != '\0')
		{
			if (strstr (gPreferredCodec, "G729") != NULL)
			{
				zVoiceCodec = 18;
			}
			else if (strstr (gPreferredCodec, "PCMU") != NULL)
			{
				zVoiceCodec = 0;
			}
			else if (strstr (gPreferredCodec, "PCMA") != NULL)
			{
				zVoiceCodec = 8;
			}
			else if (strstr (gPreferredCodec, "GSM") != NULL)
			{
				zVoiceCodec = 3;
			}
			else if (strstr (gPreferredCodec, "AMR") != NULL ||
					 strstr (gPreferredCodec, "amr") != NULL)
			{
				zVoiceCodec = gCall[zCall].amrIntCodec[0];
			}
			else if (strstr (gPreferredCodec, "SPEEX") != NULL)
			{
				if (strstr (gPreferredCodec, "8000"))
				{
					zVoiceCodec = 110;
				}
				else
				{
					zVoiceCodec = 111;
				}
			}
			else if (strstr (gPreferredCodec, "G722") != NULL)
			{
				zVoiceCodec = 9;
			}
		}
	}
	sprintf (yStrTelephonyPayload, "%d telephone-event/8000",
			 zTelephonyPayload);

	char            sendrecv[32] = "";

#if 1
	switch (gCall[zCall].sendRecvStatus)
	{
	case SENDONLY:
		sprintf (sendrecv, "%s", "a=recvonly");
		break;
	case RECVONLY:
		sprintf (sendrecv, "%s", "a=sendonly");
		break;
	case INACTIVE:
		sprintf (sendrecv, "%s", "a=inactive");
		break;
	case SENDRECV:
	default:
		break;
	}
#endif

	if (zVoiceCodec == -1)
	{
		sprintf (tmp,
				 "v=0\r\n"
				 "o=ArcSipIvr 2005 %d IN %s %s\r\n"
				 "s=SIP Call\r\n" "c=IN %s %s\r\n" "t=0 0\r\n"
				 //"m=audio %s RTP/AVP 18 0 8 3 110 111 101 96\r\n"
				 "m=audio %s RTP/AVP 18 0 8 3 110 111 %d %d\r\n"
				 "a=rtpmap:18 G729/8000\r\n"
				 "a=rtpmap:0 PCMU/8000\r\n"
				 "a=rtpmap:8 PCMA/8000\r\n"
				 "a=rtpmap:3 GSM/8000\r\n"
				 "a=rtpmap:9 G722/8000\r\n"
				 "a=rtpmap:110 speex/8000\r\n"
				 "a=rtpmap:111 speex/16000\r\n"
				 "a=rtpmap:96 telephone-event/8000\r\n"
				 "a=rtpmap:101 telephone-event/8000\r\n",
				 yIntVersionNumber,
				 ip_version,
				 localip,
				 ip_version,
				 localip,
				 zStrAudioPort,
				 gCall[zCall].telephonePayloadType,
				 gCall[zCall].telephonePayloadType_2);
	}
	else
	{
		if (sendrecv != NULL && sendrecv[0] != '\0')
		{
			switch (zVoiceCodec)
			{

			case 4:
				sprintf (tmp, 
                         "v=0\r\n" 
                         "o=ArcSipIvr 2005 %d IN %s %s\r\n" 
                         "s=SIP Call\r\n" 
                         "c=IN %s %s\r\n" 
                         "t=0 0\r\n" 
                         "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
						 "a=rtpmap:%s\r\n"
						 "%s\r\n",
						 yIntVersionNumber,
						 ip_version,
						 localip,
						 ip_version,
						 localip,
						 zStrAudioPort,
						 zVoiceCodec,
						 zTelephonyPayload, yStrTelephonyPayload, sendrecv);

				break;

			case 18:

				if (gCall[zCall].isG729AnnexB == NO)
				{
					sprintf (tmp, 
							 "v=0\r\n" 
			                 "o=ArcSipIvr 2005 %d IN %s %s\r\n" 
                             "s=SIP Call\r\n" 
                             "c=IN %s %s\r\n" 
                             "t=0 0\r\n" 
                             "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
							 "a=rtpmap:18 G729/8000\r\n"
							 "a=fmtp:18 annexb=no\r\n"
							 "a=rtpmap:%s\r\n"
							 "a=ptime:%d\r\n"
							 "%s\r\n",
							 yIntVersionNumber,
							 ip_version,
							 localip,
							 ip_version,
							 localip,
							 zStrAudioPort,
							 zVoiceCodec,
							 zTelephonyPayload,
							 yStrTelephonyPayload, gPreferredPTime, sendrecv);
				}
				else
				{
					sprintf (tmp, 
                             "v=0\r\n" 
                             "o=ArcSipIvr 2005 %d IN %s %s\r\n" 
                             "s=SIP Call\r\n" 
                             "c=IN %s %s\r\n" 
                             "t=0 0\r\n" 
                             "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
							 "a=rtpmap:18 G729/8000\r\n"
							 "a=fmtp:18 annexb=yes\r\n"
							 "a=rtpmap:%s\r\n"
							 "a=ptime:%d\r\n"
							 "%s\r\n",
							 yIntVersionNumber,
							 ip_version,
							 localip,
							 ip_version,
							 localip,
							 zStrAudioPort,
							 zVoiceCodec,
							 zTelephonyPayload,
							 yStrTelephonyPayload, gPreferredPTime, sendrecv);
				}
				break;

			case 9:

				sprintf (tmp, 
                         "v=0\r\n" 
                         "o=ArcSipIvr 2005 %d IN IP4 %s\r\n" 
                         "s=SIP Call\r\n" 
                         "c=IN IP4 %s\r\n" 
                         "t=0 0\r\n" 
                         "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
						 "a=rtpmap:9 G722/8000\r\n"
						 "a=rtpmap:%s\r\n"
						 "a=ptime:%d\r\n"
						 "%s\r\n",
						 yIntVersionNumber,
						 localip,
						 localip,
						 zStrAudioPort,
						 zVoiceCodec,
						 zTelephonyPayload,
						 yStrTelephonyPayload, gPreferredPTime, sendrecv);
				break;

			default:
				sprintf (tmp,
						 "v=0\r\n"
						 "o=ArcSipIvr 2005 %d IN %s %s\r\n"
						 "s=SIP Call\r\n"
						 "c=IN %s %s\r\n"
						 "t=0 0\r\n"
						 "m=audio %s RTP/AVP %d %d\r\n"
						 "a=rtpmap:%s\r\n"
						 "a=rtpmap:%s\r\n"
						 "a=ptime:%d\r\n"
						 "%s\r\n",
						 yIntVersionNumber,
						 ip_version,
						 localip,
						 ip_version,
						 localip,
						 zStrAudioPort,
						 zVoiceCodec,
						 zTelephonyPayload,
						 yStrVoiceCodec,
						 yStrTelephonyPayload, gPreferredPTime, sendrecv);

				break;

			}
			if (zVoiceCodec == gCall[zCall].amrIntCodec[0])
			{
				sprintf (tmp, 
                         "v=0\r\n" 
                         "o=ArcSipIvr 2005 %d IN %s %s\r\n" 
                         "s=SIP Call\r\n" 
                         "c=IN %s %s\r\n" 
                         "t=0 0\r\n" 
                         "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
						 "%s"
						 "a=rtpmap:%s\r\n"
						 "%s\r\n",
						 yIntVersionNumber,
						 ip_version,
						 localip,
						 ip_version,
						 localip,
						 zStrAudioPort,
						 zVoiceCodec,
						 zTelephonyPayload,
						 yStrVoiceCodec, yStrTelephonyPayload, sendrecv);
			}
		}
		else
		{
			switch (zVoiceCodec)
			{

			case 4:
				sprintf (tmp, 
                         "v=0\r\n" 
                         "o=ArcSipIvr 2005 %d IN %s %s\r\n" 
                         "s=SIP Call\r\n" 
                         "c=IN %s %s\r\n" 
                         "t=0 0\r\n" 
                         "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
						 "a=rtpmap:%s\r\n"
						 "a=ptime:%d\r\n",
						 yIntVersionNumber,
						 ip_version,
						 localip,
						 ip_version,
						 localip,
						 zStrAudioPort,
						 zVoiceCodec,
						 zTelephonyPayload,
						 yStrTelephonyPayload, gPreferredPTime);

				break;

			case 18:

				if (gCall[zCall].isG729AnnexB == NO)
				{
					sprintf (tmp, 
                             "v=0\r\n" 
                             "o=ArcSipIvr 2005 %d IN %s %s\r\n" 
                             "s=SIP Call\r\n" 
                             "c=IN %s %s\r\n" 
                             "t=0 0\r\n" 
                             "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
							 "a=rtpmap:18 G729/8000\r\n"
							 "a=fmtp:18 annexb=no\r\n"
							 "a=rtpmap:%s\r\n"
							 "a=ptime:%d\r\n",
							 yIntVersionNumber,
							 ip_version,
							 localip,
							 ip_version,
							 localip,
							 zStrAudioPort,
							 zVoiceCodec,
							 zTelephonyPayload,
							 yStrTelephonyPayload, gPreferredPTime);
				}
				else
				{
					sprintf (tmp, 
                             "v=0\r\n" 
                             "o=ArcSipIvr 2005 %d IN %s %s\r\n" 
                             "s=SIP Call\r\n" 
                             "c=IN %s %s\r\n" 
                             "t=0 0\r\n" 
                             "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
							 "a=rtpmap:18 G729/8000\r\n"
							 "a=fmtp:18 annexb=yes\r\n"
							 "a=rtpmap:%s\r\n"
							 "a=ptime:%d\r\n",
							 yIntVersionNumber,
							 ip_version,
							 localip,
							 ip_version,
							 localip,
							 zStrAudioPort,
							 zVoiceCodec,
							 zTelephonyPayload,
							 yStrTelephonyPayload, gPreferredPTime);
				}

				break;

			case 9:

				sprintf (tmp, 
                         "v=0\r\n" 
                         "o=ArcSipIvr 2005 %d IN IP4 %s\r\n" 
                         "s=SIP Call\r\n" 
                         "c=IN IP4 %s\r\n" 
                         "t=0 0\r\n" 
                         "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
						 "a=rtpmap:9 G722/8000\r\n"
						 "a=rtpmap:%s\r\n"
						 "a=ptime:%d\r\n"
						 "%s\r\n",
						 yIntVersionNumber,
						 localip,
						 localip,
						 zStrAudioPort,
						 zVoiceCodec,
						 zTelephonyPayload,
						 yStrTelephonyPayload, gPreferredPTime, sendrecv);
				break;

			default:
				sprintf (tmp,
						 "v=0\r\n"
						 "o=ArcSipIvr 2005 %d IN %s %s\r\n"
						 "s=SIP Call\r\n"
						 "c=IN %s %s\r\n"
						 "t=0 0\r\n"
						 "m=audio %s RTP/AVP %d %d\r\n"
						 "a=rtpmap:%s\r\n"
						 "a=rtpmap:%s\r\n"
						 "a=ptime:%d\r\n",
						 yIntVersionNumber,
						 ip_version,
						 localip,
						 ip_version,
						 localip,
						 zStrAudioPort,
						 zVoiceCodec,
						 zTelephonyPayload,
						 yStrVoiceCodec,
						 yStrTelephonyPayload, gPreferredPTime);

				break;

			}
			if (zVoiceCodec == gCall[zCall].amrIntCodec[0])
			{
				sprintf (tmp, 
                         "v=0\r\n" 
                         "o=ArcSipIvr 2005 %d IN %s%s\r\n" 
                         "s=SIP Call\r\n" 
                         "c=IN %s%s\r\n" 
                         "t=0 0\r\n" 
                         "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
						 "%s"
						 "a=rtpmap:%s\r\n",
						 yIntVersionNumber,
						 ip_version,
						 localip,
						 ip_version,
						 localip,
						 zStrAudioPort,
						 zVoiceCodec,
						 zTelephonyPayload,
						 yStrVoiceCodec, yStrTelephonyPayload);
			}
		}

	}

	sprintf (gCall[zCall].sdp_body_local, "%s", tmp);

	tmp[0] = 0;

	return 0;

}/*END: int createSdpBody */

int
getAudioCodec (int zCall)
{
	char            mod[] = "getAudioCodec";

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "audioCodecString=(%s)", gCall[zCall].audioCodecString);

	if (strstr (gCall[zCall].audioCodecString, "G729") != NULL)
	{
		return 18;
	}
	else if (strstr (gCall[zCall].audioCodecString, "PCMU") != NULL)
	{
		return 0;
	}
	else if (strstr (gCall[zCall].audioCodecString, "PCMA") != NULL)
	{
		return 8;
	}
	else if (strstr (gCall[zCall].audioCodecString, "GSM") != NULL)
	{
		return 3;
	}
	else if (strstr (gCall[zCall].audioCodecString, "AMR") != NULL ||
			 strstr (gCall[zCall].audioCodecString, "amr") != NULL)
	{
		return gCall[zCall].amrIntCodec[0];
	}
	else if (strstr (gCall[zCall].audioCodecString, "SPEEX") != NULL)
	{
		if (strstr (gCall[zCall].audioCodecString, "8000"))
		{
			return 110;
		}
		else
		{
			return 111;
		}
	}
	else if (strstr (gCall[zCall].audioCodecString, "G722") != NULL)
	{
		return 9;
	}
	return 0;

}//getAudioCodec

///This function creates the sdp body for outgoing SIP INVITEs.
/**
Starting with eXosip2 the creation and population of an SDP body is not done
by the stack.  We therefore need to build a valid sdp body and attach this
body to any outgoing messages.  This is because eXosip2 is completely media
independent.  This function only needs to know what port SIPIVR will be
receving audio on (zStrAudioPort), what DTMF payload type will be used
(zTelephonyPayload), and what voice codec this call will be using
(zVoiceCodec).
*/
int
createBridgeSdpBody (int zCall, char zStrAudioPort[15],
					 int zVoiceCodec, int zTelephonyPayload, int zCrossPort)
{
	char            tmp[4096];
	char            localip[128];
	char            yStrVoiceCodec[100];
	char            yStrTelephonyPayload[100];
	int             yIntVersionNumber = 0;
	char            mod[] = "createBridgeSdpBody";
	char           *ip_version = "IP4";

	yIntVersionNumber = zCall + 1997;

	//eXosip_guess_localip (AF_INET, localip, 128);

	if (gEnableIPv6Support != 0)
	{
		ip_version = "IP6";
	}

	sprintf (localip, "%s", gHostIp);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "zVoiceCodec=(%d).", zVoiceCodec);
	switch (zVoiceCodec)
	{
	case 111:
		sprintf (yStrVoiceCodec, "speex/8000");
		break;

	case 110:
		sprintf (yStrVoiceCodec, "speex/16000");
		break;

	case 18:
		sprintf (yStrVoiceCodec, "G729/8000");
		break;

	case 8:
		sprintf (yStrVoiceCodec, "8 PCMA/8000");
		break;

	case 9:
		sprintf (yStrVoiceCodec, "G722/8000");
		break;

	case 3:
		sprintf (yStrVoiceCodec, "8 PCMA/8000");
		break;

	case 0:

	default:
		sprintf (yStrVoiceCodec, "0 PCMU/8000");
		break;
	}

	sprintf (yStrTelephonyPayload, "%d telephone-event/8000",
			 zTelephonyPayload);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
			   "CreateBridgeSdpBody zVoiceCodec=%d.", zVoiceCodec);

	if (zVoiceCodec == -1)
	{

	int             aLegAudioCodec = getAudioCodec (zCrossPort);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "aLegAudioCodec=%d.", aLegAudioCodec);

		switch (aLegAudioCodec)
		{

		case 4:

			sprintf (tmp,
					 "v=0\r\n"
					 "o=ArcSipIvr 2005 %d IN %s %s\r\n"
					 "s=SIP Call\r\n"
					 "c=IN %s %s\r\n"
					 "t=0 0\r\n"
					 "m=audio %s RTP/AVP %d %d\r\n"
					 "a=rtpmap:34 h263/90000\r\n"
					 "a=rtpmap:%s\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 aLegAudioCodec,
					 zTelephonyPayload,
					yStrTelephonyPayload);

			break;

		case 18:

			sprintf (tmp,
					 "v=0\r\n"
					 "o=ArcSipIvr 2005 %d IN %s %s\r\n"
					 "s=SIP Call\r\n"
					 "c=IN %s %s\r\n"
					 "t=0 0\r\n"
					 "m=audio %s RTP/AVP %d %d\r\n"
					 "a=rtpmap:18 G729/8000\r\n"
					 "a=rtpmap:34 h263/90000\r\n"
					 "a=rtpmap:%s\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 aLegAudioCodec,
					 zTelephonyPayload,
					yStrTelephonyPayload);

			break;

		case 9:

			sprintf (tmp,
					 "v=0\r\n"
					 "o=ArcSipIvr 2005 %d IN IP4 %s\r\n"
					 "s=SIP Call\r\n"
					 "c=IN IP4 %s\r\n"
					 "t=0 0\r\n"
					 "m=audio %s RTP/AVP %d %d\r\n"
					 "a=rtpmap:9 G722/8000\r\n"
					 "a=rtpmap:34 h263/90000\r\n"
					 "a=rtpmap:%s\r\n",
					 yIntVersionNumber,
					 localip,
					 localip,
					 zStrAudioPort,
					 aLegAudioCodec,
					 zTelephonyPayload,
					yStrTelephonyPayload);

			break;

		case 0:
			sprintf (tmp,
					 "v=0\r\n"
					 "o=ArcSipIvr 2005 %d IN %s %s\r\n"
					 "s=SIP Call\r\n"
					 "c=IN %s %s\r\n"
					 "t=0 0\r\n"
					 "m=audio %s RTP/AVP %d %d\r\n"
					 "a=rtpmap:0 PCMU/8000\r\n"
					 "a=rtpmap:34 h263/90000\r\n"
					 "a=rtpmap:%s\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 aLegAudioCodec,
					 zTelephonyPayload,
					yStrTelephonyPayload);
			break;

		case 8:
			sprintf (tmp,
					 "v=0\r\n"
					 "o=ArcSipIvr 2005 %d IN %s %s\r\n"
					 "s=SIP Call\r\n"
					 "c=IN %s %s\r\n"
					 "t=0 0\r\n"
					 "m=audio %s RTP/AVP %d %d\r\n"
					 "a=rtpmap:8 PCMA/8000\r\n"
					 "a=rtpmap:34 h263/90000\r\n"
					 "a=rtpmap:101 telephone-event/8000\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 aLegAudioCodec,
					 zTelephonyPayload,
					yStrTelephonyPayload);
			break;

		case 3:
			sprintf (tmp,
					 "v=0\r\n"
					 "o=ArcSipIvr 2005 %d IN %s %s\r\n"
					 "s=SIP Call\r\n"
					 "c=IN %s %s\r\n"
					 "t=0 0\r\n"
					 "m=audio %s RTP/AVP %d %d\r\n"
					 "a=rtpmap:3 GSM/8000\r\n"
					 "a=rtpmap:34 h263/90000\r\n"
					 "a=rtpmap:101 telephone-event/8000\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 aLegAudioCodec,
					 zTelephonyPayload,
					yStrTelephonyPayload);
			break;

		default:
			sprintf (tmp,
					 "v=0\r\n"
					 "o=ArcSipIvr 2005 %d IN %s %s\r\n"
					 "s=SIP Call\r\n" "c=IN %s %s\r\n" "t=0 0\r\n"
					 //"m=audio %s RTP/AVP 18 0 8 3 110 111 101 96\r\n"
					 "m=audio %s RTP/AVP 18 0 8 3 110 111 %d %d\r\n"
					 "a=rtpmap:18 G729/8000\r\n"
					 "a=rtpmap:0 PCMU/8000\r\n"
					 "a=rtpmap:8 PCMA/8000\r\n"
					 "a=rtpmap:3 GSM/8000\r\n"
					 "a=rtpmap:9 G722/8000\r\n"
					 "a=rtpmap:110 speex/8000\r\n"
					 "a=rtpmap:111 speex/16000\r\n"
					 "a=rtpmap:96 telephone-event/8000\r\n"
					 "a=rtpmap:34 h263/90000\r\n"
					 "a=rtpmap:101 telephone-event/8000\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 gCall[zCall].telephonePayloadType,
					 gCall[zCall].telephonePayloadType_2);

			break;
		}

	}
	else
	{
		switch (zVoiceCodec)
		{

		case 4:
			sprintf (tmp, "v=0\r\n" "o=ArcSipIvr 2005 %d IN %s %s\r\n" "s=SIP Call\r\n" "c=IN %s %s\r\n" "t=0 0\r\n" "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
					 "a=rtpmap:%s\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 zVoiceCodec, zTelephonyPayload, yStrTelephonyPayload);

			break;

		case 18:

			sprintf (tmp, "v=0\r\n" "o=ArcSipIvr 2005 %d IN %s %s\r\n" "s=SIP Call\r\n" "c=IN %s %s\r\n" "t=0 0\r\n" "m=audio %s RTP/AVP %d %d\r\n"	//<<------------ +5
					 "a=rtpmap:%s\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 zVoiceCodec, zTelephonyPayload, yStrTelephonyPayload);

			break;

		default:
			sprintf (tmp,
					 "v=0\r\n"
					 "o=ArcSipIvr 2005 %d IN %s %s\r\n"
					 "s=SIP Call\r\n"
					 "c=IN %s %s\r\n"
					 "t=0 0\r\n"
					 "m=audio %s RTP/AVP %d %d\r\n"
					 "a=rtpmap:%s\r\n"
					 "a=rtpmap:%s\r\n",
					 yIntVersionNumber,
					 ip_version,
					 localip,
					 ip_version,
					 localip,
					 zStrAudioPort,
					 zVoiceCodec,
					 zTelephonyPayload, yStrVoiceCodec, yStrTelephonyPayload);

			break;

		}
	}

	sprintf (gCall[zCall].sdp_body_local, "%s", tmp);

	return 0;

}/*END: int createSdpBody */

///This function is simply a sleep for doing a nanosecond sleep
int
nano_sleep (int Seconds, int nanoSeconds)
{
	char            mod[] = { "nano_sleep" };
	struct timespec lTimeSpec1, lTimeSpec2;

	lTimeSpec1.tv_sec = Seconds;
	lTimeSpec1.tv_nsec = nanoSeconds;
	nanosleep (&lTimeSpec1, &lTimeSpec2);

	return (0);

}/*END: int nano_sleep */

///This function prints out log messages into the /home/arc/.ISP/Log/ISP.cur file.
#if 0

int
dynVarLog (int zLine, int zCall, char *zpMod, int mode, int msgId,
		   int msgType, char *msgFormat, ...)
{
	char            mod[] = "dynVarLog";
	va_list         ap;
	char            m[1024];
	char            m1[512];
	char            type[32];
	int             rc;
	char            lPortStr[10];

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

	if (mode == REPORT_CDR)
	{
		sprintf (m, "%s", m1);
	}
	else
	{
		sprintf (m, "%s[%d]%s", type, zLine, m1);
	}

	sprintf (lPortStr, "%d", zCall);

	LogARCMsg (zpMod, mode, lPortStr, "DYN", "ArcIPDynMgr", msgId, m);

	return (0);

}								/*END: int dynVarLog */

#endif

///This function writes a CDR message to the ISP.cur log file.
/**
To keep track when a call is presented, started, and ended we need to write a 
message to ISP.cur.  This function is called to write that message.  If this
is a bridge call then just set zBlegPort to gCall[zCall].crossPort.
*/
int
writeCDR (char *mod, int zCall, char *zType, int zMsgId, int zBLegPort)
{
	char            yCDRKey[64];
	char            yMsg[255];
	char            yHost[255];
	char            yCurrentTime[32];
	time_t          tics;
	char           *pChar;

	memset (yHost, 0, sizeof (yHost));

	gethostname (yHost, 254);

	if ((pChar = strchr (yHost, '.')) != NULL)
	{
		*pChar = '\0';
	}
	yHost[31] = '\0';

	setTime (yCurrentTime, &tics);

	sprintf (yCDRKey, "%s-PORT%d-%s", yHost, zCall, gCall[zCall].lastEventTime);

	sprintf (yMsg, "%s:%s:%s:%s:%s:%s:%s:%s",
			 yCDRKey,
			 zType, gProgramName,
			 gCall[zCall].ani,
			 gCall[zCall].dnis,
			 yCurrentTime,
			 gCall[zCall].GV_CustData1, gCall[zCall].GV_CustData2);

	if (zBLegPort >= 0)
	{
		dynVarLog (__LINE__, zBLegPort, mod, REPORT_CDR, zMsgId, INFO, "%s",
				   yMsg);
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_CDR, zMsgId, INFO, "%s",
				   yMsg);
	}

	return (0);

}/*END: int writeCDR */

///This function is used to create Call Manager's shared memory.
/**
In order for Call Manager to communicate with Media Manager we use shared
memory.  This function creates the shared memory used to communicate between
Call Manager and Media Manager.  We remove the memory before creating it.
*/
int
createSharedMem (int zCall)
{
	char            mod[] = "createSharedMem";
	int             yrc = -1;

	//gShmKey = SHMKEY_SIP;
	gShmKey = SHMKEY_SIP + ((key_t) gDynMgrId);

	//create shared memory segment 

	if ((gShmId =
		 shmget (gShmKey, SHMSIZE_SIP, PERMS | IPC_CREAT | IPC_EXCL)) < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "shmget() failed. rc=%d.  Unable to get shared memory segment %ld. [%d, %s] "
				   "gDynMgrId is (%d).",
				   gShmId, gShmKey, errno, strerror (errno), gDynMgrId);
		return (-1);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Successfully created shared memory key; id for SIP is [%ld:%d].",
			   gShmKey, gShmId);

	return (ISP_SUCCESS);

}/*END: int createSharedMem */

///This function is used to attach Call Manager's shared memory.
/**
In order for Call Manager to communicate with Media Manager we use shared
memory.  This function attaches the shared memory used to communicate between
Call Manager and Media Manager.  We attach the shared memory only after
removing it and creating it.
*/
int
attachSharedMem (int zCall)
{
	char            mod[] = "attachSharedMem";
	int             yRc = -1;
	int             i = 0;

	if ((gShmBase = shmat (gShmId, 0, 0)) == (char *) -1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "shmat() failed.  Unable to attach to shared memory key %d. [%d, %s]",
				   gShmId, errno, strerror (errno));
		return (-1);
	}

	gEncodedShmBase = (struct EncodedShmBase *) gShmBase;

	for (i = 0; i < (MAX_PORTS * 4); i++)
	{
		gEncodedShmBase->msgCounter[i].count = 0;
	}

	return (ISP_SUCCESS);

}/*END: int attachSharedMem */

int
removeFromTimerList (int zCall, int zOpcode)
{
	struct MultiPurposeTimerStrcut *ypTmpMultiPurposeTimerStruct = NULL;

	pthread_mutex_lock (&gMultiPurposeTimerMutex);

	ypTmpMultiPurposeTimerStruct = gpFirstMultiPurposeTimerStruct;
	while (ypTmpMultiPurposeTimerStruct != NULL)
	{
		if (ypTmpMultiPurposeTimerStruct->opcode == zOpcode &&
			ypTmpMultiPurposeTimerStruct->port == zCall)
		{
			ypTmpMultiPurposeTimerStruct->deleted = 1;
		}

		ypTmpMultiPurposeTimerStruct = ypTmpMultiPurposeTimerStruct->nextp;
	}

	pthread_mutex_unlock (&gMultiPurposeTimerMutex);

	return(0);
}/*END: removeFromTimerList */

#ifdef DJB_CAN_GET_RID_OF_THIS
int modifyExistingSessionExpiresTerminate(int zLine, int zCall, struct timeb *zTimeb, char *zTimeStr, int zSeconds)
{
	static char	mod[]="modifyExistingSessExpTerm";

	int		opcode = MP_OPCODE_SESSION_EXPIRES_TERMINATE_APP;
	int		found = 0;

	struct MultiPurposeTimerStrcut *ypTmpMultiPurposeTimerStruct = NULL;

	pthread_mutex_lock (&gMultiPurposeTimerMutex);

	ypTmpMultiPurposeTimerStruct = gpFirstMultiPurposeTimerStruct;
	while (ypTmpMultiPurposeTimerStruct != NULL)
	{
		if (ypTmpMultiPurposeTimerStruct->opcode == opcode &&
			ypTmpMultiPurposeTimerStruct->port == zCall)
		{
			// ypTmpMultiPurposeTimerStruct->deleted = 1;
	  		memcpy((void *)(ypTmpMultiPurposeTimerStruct->expires), (void *)zTimeb, sizeof(struct timeb));
			found = 1;
			break;
		}

		ypTmpMultiPurposeTimerStruct = ypTmpMultiPurposeTimerStruct->nextp;
	}

	pthread_mutex_unlock (&gMultiPurposeTimerMutex);
	if ( found )
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				"[%d] Modified existing session to expire at %s (%d seconds) [ MP_OPCODE_SESSION_EXPIRES_TERMINATE_APP, cid=%d  ]. %d",
				zLine, zTimeStr, zSeconds,  gCall[zCall].cid, zTimeb->time);
		return(0);
	}
	return(-1);

}/*END: modifyExistingSessionExpiresTerminate */
#endif // DJB_CAN_GET_RID_OF_THIS

int
getTimerListCount(){

  int rc = 0;

  pthread_mutex_lock(&gMultiPurposeTimerMutex);


  struct MultiPurposeTimerStrcut *elem = NULL;

  elem = gpFirstMultiPurposeTimerStruct;

  while(elem){
     rc++;
     elem = elem->nextp;
  }

//fprintf(stderr, " %s() count=%d refcount=%d\n", __func__, rc, MultiPurposeTimerCount);

  pthread_mutex_unlock(&gMultiPurposeTimerMutex);

  return rc;
}

int 
getFirstFromTimerList(struct MultiPurposeTimerStrcut *dest, void *last, void **ref){

   int rc = 0;
   struct MultiPurposeTimerStrcut *elem = NULL;
   pthread_mutex_lock(&gMultiPurposeTimerMutex);

   elem = gpFirstMultiPurposeTimerStruct;

   if(last == NULL){
	if(elem){
	  memcpy(dest, elem, sizeof(struct MultiPurposeTimerStrcut));
	  *ref = elem;
      rc = 1;
    }
   } else {
	 // ff to the one after ref 
     while(elem){
       if(elem != last){
		 if(elem){
	       memcpy(dest, elem, sizeof(struct MultiPurposeTimerStrcut));
	       *ref = elem;
		   rc = 1;
		   break;
         }
       }
       elem = elem->nextp;
     }
   }

//fprintf(stderr, " %s() last = %p ref=%p rc=%d\n", __func__, last, *ref, rc);
   pthread_mutex_unlock(&gMultiPurposeTimerMutex);
   return rc;
}

// leave the list in a good state 
// this will keep it around 5 minutes after 
// the expire time, still good for most purposes 
// 

int TimerByAge(struct MultiPurposeTimerStrcut *elem, void *arg){

#define MAXAGE_IN_SECONDS 3600
// setting this for a long timeout to clean nothing should be 
// an hour in there joe s. trying to resolve an issue 
// with the timer thread 

//   struct timeb now;
	struct timeval	now;
   int rc = 0;

//   ftime(&now);
	gettimeofday(&now, NULL);

   if(elem && ((elem->expires.tv_sec + MAXAGE_IN_SECONDS) < now.tv_sec)){
      rc = 1;
   }

   return rc;
}

int 
TimerByRef(struct MultiPurposeTimerStrcut *elem, void *arg){

  int rc = 0;
  if(elem == arg){
    rc = 1;
  }
  return rc;
}

int
TimerMarkedAsDeleted(struct MultiPurposeTimerStrcut *elem, void *arg){

  int rc = 0;
  if(elem->deleted){
    rc = 1;
  }
  return rc;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int
modifyExpiresTimerList (int zCall, int zOpcode, struct timeval zExpires)
{
	static char mod[]="modifyExpiresTimerList";

    char            newTime[32];
    char            oldTime[32];
    struct tm       tempTM;
    struct tm       *pTM;

    int rc = 0;
	struct MultiPurposeTimerStrcut *elem = NULL;
	struct MultiPurposeTimerStrcut *prev = NULL;
	struct timeval	tmpExpires;
	int				found;

	pthread_mutex_lock (&gMultiPurposeTimerMutex);

	(void *)localtime_r(&zExpires.tv_sec, &tempTM);
	pTM = &tempTM;
	(void)strftime(newTime, 32, "%H:%M:%S", pTM);

	elem = gpFirstMultiPurposeTimerStruct;
	
	found = 0;

	while (elem != NULL)
	{
		if ( (elem->opcode == zOpcode ) && (elem->port == zCall) )
		{
			memcpy((struct timespan *) &tmpExpires,  &(elem->expires), sizeof(tmpExpires));
			memcpy((struct timespan *) &elem->expires,  (const void *)&zExpires, sizeof(struct timeval));

			(void *)localtime_r(&tmpExpires.tv_sec, &tempTM);
			pTM = &tempTM;
			(void)strftime(oldTime, 32, "%H:%M:%S", pTM);

			dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				"Changed expires for [opcode=%d,port=%d from [%s] to [%s]",
				elem->opcode, elem->port, oldTime, newTime);

			found = 1;
			break;
		}
        prev = elem;
		elem = elem->nextp;
	}

	if ( ! found )
	{
//		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//				"Timer thread for [%d, %d] not found.  No expires time updated.",
//				zOpcode, zCall);
		pthread_mutex_unlock (&gMultiPurposeTimerMutex);
		return(-1);
	}

	pthread_mutex_unlock (&gMultiPurposeTimerMutex);
    return rc;
}

int printTimerList(int zLine, int zPort)
{
	static char mod[]="printTimerList";

    int i = 0;
	struct MultiPurposeTimerStrcut *elem = NULL;
	struct MultiPurposeTimerStrcut *prev = NULL;

	pthread_mutex_lock (&gMultiPurposeTimerMutex);

	elem = gpFirstMultiPurposeTimerStruct;
	
	i=0;
	while (elem != NULL)
	{
		dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "[%d] timerList[%d] (op=%d, port=%d, data1=%d, data2=%d, op2=%d)", zLine,
				i++, elem->opcode, elem->port, elem->data1, elem->data2, elem->msgToDM.opcode);

        prev = elem;
		elem = elem->nextp;
	}
	if ( i == 0 ) 
	{
		dynVarLog (__LINE__, zPort, mod, REPORT_VERBOSE, TEL_BASE, INFO, "timerList is empty.");
	}

	pthread_mutex_unlock (&gMultiPurposeTimerMutex);
    return 0;
} // END: printTimerList()

int
delFromTimerList(int zLine, int (*cmp)(struct MultiPurposeTimerStrcut *e, void *arg), void *arg){


    int rc = 0;
	struct MultiPurposeTimerStrcut *elem = NULL;
	struct MultiPurposeTimerStrcut *prev = NULL;

	pthread_mutex_lock (&gMultiPurposeTimerMutex);

	elem = gpFirstMultiPurposeTimerStruct;
	
	while (elem != NULL)
	{
		if (cmp(elem, arg) == 1)
		{
			dynVarLog (__LINE__, elem->port, "delFromTimerList", REPORT_VERBOSE, TEL_BASE, INFO,
			   "[%d] Deleting from timerList (op=%d, port=%d, data1=%d, data2=%d, op2=%d)", zLine,
				elem->opcode, elem->port, elem->data1, elem->data2, elem->msgToDM.opcode);

			if(prev){
			  prev->nextp = elem->nextp;
              free(elem);
			  elem = prev->nextp;
			  if(prev->nextp == NULL){
                 ;; // gpLastMultiPurposeTimerStruct = prev;
              }
            } else {
	          gpFirstMultiPurposeTimerStruct = elem->nextp;
			  free(elem);
              elem = gpFirstMultiPurposeTimerStruct;
            }
            MultiPurposeTimerCount--;
			continue;
		}
        prev = elem;
		elem = elem->nextp;
	}

	pthread_mutex_unlock (&gMultiPurposeTimerMutex);
    return rc;
}

// delFromTimerList(DelTimerByAge);

///This function adds certain DMOP commands to the timer thread.
int
addToTimerList (int zLine, int zCall, int zOpcode, struct MsgToDM zMsgToDM,
				struct timeval zExpires, int zData1, int zData2, int zInterval)
{
	int             yRc = 0;
	char            mod[] = { "addToTimerList" };
	char            yMessage[256];

	struct MultiPurposeTimerStrcut *ypTmpMultiPurposeTimerStruct = NULL;

	pthread_mutex_lock (&gMultiPurposeTimerMutex);

	ypTmpMultiPurposeTimerStruct = (struct MultiPurposeTimerStrcut *)
		arc_malloc (zCall, mod, __LINE__,
					sizeof (struct MultiPurposeTimerStrcut));

	if (ypTmpMultiPurposeTimerStruct == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_MEMORY_ERROR, ERR,
				   "[%d] Failed to allocate memory for elemet to add to timer.", zLine);

		pthread_mutex_unlock (&gMultiPurposeTimerMutex);
		return (-1);
	}

	sprintf (yMessage, "%p", ypTmpMultiPurposeTimerStruct);

    memset(ypTmpMultiPurposeTimerStruct, 0, sizeof (struct MultiPurposeTimerStrcut)); 
	ypTmpMultiPurposeTimerStruct->opcode = zOpcode;
	ypTmpMultiPurposeTimerStruct->port = zCall;
	ypTmpMultiPurposeTimerStruct->interval = zInterval;
	ypTmpMultiPurposeTimerStruct->expires = zExpires;
	ypTmpMultiPurposeTimerStruct->data1 = zData1;
	ypTmpMultiPurposeTimerStruct->data2 = zData2;
	ypTmpMultiPurposeTimerStruct->deleted = 0;

	memcpy (&(ypTmpMultiPurposeTimerStruct->msgToDM), &zMsgToDM,
			sizeof (zMsgToDM));

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "[%d] Added to timerList (op=%d, port=%d, data1=%d, data2=%d, op2=%d)", zLine,
				ypTmpMultiPurposeTimerStruct->opcode, ypTmpMultiPurposeTimerStruct->port, 
				ypTmpMultiPurposeTimerStruct->data1, ypTmpMultiPurposeTimerStruct->data2, 
				ypTmpMultiPurposeTimerStruct->msgToDM.opcode);

	if (gpFirstMultiPurposeTimerStruct == NULL)
	{
		ypTmpMultiPurposeTimerStruct->nextp = NULL;
		gpFirstMultiPurposeTimerStruct = ypTmpMultiPurposeTimerStruct;
		// gpLastMultiPurposeTimerStruct = ypTmpMultiPurposeTimerStruct;
		// gpFirstMultiPurposeTimerStruct->nextp = NULL;
	}
	else
	{
		ypTmpMultiPurposeTimerStruct->nextp = gpFirstMultiPurposeTimerStruct;
		gpFirstMultiPurposeTimerStruct = ypTmpMultiPurposeTimerStruct;
	}
	
				

    // added 
    MultiPurposeTimerCount++;

	pthread_mutex_unlock (&gMultiPurposeTimerMutex);

	return (0);

}/*END: int addToTimerList */

#ifdef ACU_LINUX
int
clearConfData (int zCall)
{
	char            mod[] = "clearConfData";

	for (int i = 0; i < MAX_CONF_PORTS; i++)
	{
		gCall[zCall].conf_ports[i] = -1;
	}

	gCall[zCall].confUserCount = 0;

	memset (gCall[zCall].confID, 0, sizeof (gCall[zCall].confID));

	return 0;

}//clearConfData
#endif

///This function sets the call stat for port zCall to zState and prints a debug message for this change in nohup.out.
int
setCallSubState (int zCall, int zState)
{
	char            mod[] = "setCallSubState";

	gCall[zCall].callSubState = zState;

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Sub State changed to", "", zState);

	return(0);
}/*END: int setCallSubState */

///This function sets the call stat for port zCall to zState and prints a debug message for this change in nohup.out.
int
setCallState (int zCall, int zState)
{
	char            mod[] = "changeCallState";

	if (zCall < 0)
	{
		return (-1);
	}

	gCall[zCall].callState = zState;

	if (zState == CALL_STATE_CALL_RELEASED)
	{
		time (&(gCall[zCall].lastReleaseTime));
	}

#ifdef ACU_LINUX
	if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED
		|| gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
		|| gCall[zCall].callState == CALL_STATE_CALL_TERMINATE_CALLED
		|| gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
	{
		clearConfData (zCall);
	}
#endif

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "State changed to", "", zState);
// MR 4802
    if ( gCall[zCall].callState == CALL_STATE_CALL_TRANSFER_MESSAGE_ACCEPTED )  
    {
        gCall[zCall].issuedBlindTransfer = 1;
        dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
            "Set issuedBlindTransfer to %d. [cs=%d]", gCall[zCall].issuedBlindTransfer, zState);
    }

    if ( gCall[zCall].callState == CALL_STATE_IDLE )
    {
        gCall[zCall].issuedBlindTransfer = 0;
    }
// END: MR 4802

	return (0);

}/*END: int setCallState */

int
getPassword (int zCall)
{
	return zCall;
}//getPassword

///This function checks whether port zCall is still active (i.e. the caller is still there).
int
canContinue (char *mod, int zCall)
{
	if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED
		|| gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
		|| gCall[zCall].callState == CALL_STATE_CALL_TERMINATE_CALLED
		|| gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
	{
		return (0);
	}

	if (gCanReadSipEvents == 0 || gCanReadRequestFifo == 0)
	{
		return (0);
	}

	return (1);

}/*END: int canContinue */

///This function makes sure that the DMOP commands received are valid.
/**
This function checks the parameters in the struct received from the Request
fifo and if they are not valid it will return a -1.
*/
int
validateAppRequestParameters (char *mod, struct MsgToDM *zpMsgToDM)
{
	int             zCall;

	zCall = zpMsgToDM->appCallNum;
	if (((zCall < gStartPort) || (zCall > gEndPort)) &&
		(zCall != NULL_APP_CALL_NUM))
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_ELEMENT,
				   ERR, "Invalid call number (%d) passed.", zCall);
		return (-1);
	}

	if (zCall != NULL_APP_CALL_NUM)
	{
		switch (zpMsgToDM->opcode)
		{
		case DMOP_INITTELECOM:
			if (zpMsgToDM->appPassword != gCall[zCall].appPassword)
			{
	struct MsgToApp msgToApp;

				msgToApp.opcode = zpMsgToDM->opcode;
				msgToApp.appPid = zpMsgToDM->appPid;
				msgToApp.appRef = zpMsgToDM->appRef;
				msgToApp.appCallNum = zpMsgToDM->appCallNum;
				msgToApp.appPassword = zpMsgToDM->appPassword;
				msgToApp.returnCode = -1;

				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
						   TEL_INVALID_ELEMENT, ERR,
						   "Failed to process API (%d), call=%d, "
						   "Password (%d) passed by app "
						   "does not match password (%d) in the DynMgr.",
						   zpMsgToDM->opcode, zCall, zpMsgToDM->appPassword,
						   gCall[zCall].appPassword);

				writeGenericResponseToApp (__LINE__, zCall, &msgToApp, mod);
//				writeGenericResponseToApp (__LINE__, zCall,
//										   (struct MsgToApp *) &msgToApp, mod);

				return (-1);
			}

			break;

		case DMOP_APPDIED:
			if (zpMsgToDM->appPid != gCall[zCall].appPid)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
						   TEL_INVALID_ELEMENT, ERR,
						   "For DM opcode (%d), appPid (%d) passed does "
						   "not match DM's appPid (%d).", zpMsgToDM->opcode,
						   zpMsgToDM->appPid, gCall[zCall].appPid);

				return (-1);
			}
			break;

		case DMOP_STARTOCSAPP:
			break;

		case DMOP_STARTNTOCSAPP:
			break;

		case DMOP_STARTFAXAPP:
			break;

		case DMOP_CANTFIREAPP:
			break;

#ifdef VOICE_BIOMETRICS 
        case DMOP_VERIFY_CONTINUOUS_SETUP:
            break;
        case DMOP_VERIFY_CONTINUOUS_GET_RESULTS:
            break;
#endif // VOICE_BIOMETRICS


		default:
			if (zpMsgToDM->appPid != gCall[zCall].appPid)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
						   TEL_INVALID_ELEMENT, ERR,
						   "For DM opcode (%d), appPid (%d) passed does "
						   "not match DM's appPid (%d).", zpMsgToDM->opcode,
						   zpMsgToDM->appPid, gCall[zCall].appPid);

				return (-1);
			}

			break;

		}						/* switch */
	}							/* if */
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_INVALID_ELEMENT,
				   WARN, "Call number passed is NULL.");
	}

	return (0);

}/*END: int validateAppRequestParameters */

///This function extracts the user id from a sip url.
/**
A sip url usually is in the format username@ipaddress.  What this function
does is extract the username and sets it to out.
*/
int
extractIdFromSipUrl (char *in, char *out)
{
	int             outSize = 0;
	int             idStartIndex = 0;
	char           *idStartIndexChar = NULL;
	char           *idEndIndexChar = NULL;
	char            yTmpIn[256];

	sprintf (yTmpIn, "%s", "\0");

	int             idEndIndex = 255;

	if (!out)
	{
		return (-1);
	}

	if (!in)
	{
		sprintf (out, "%s", "anonymous");
		return (-1);
	}

	sprintf (yTmpIn, "%s", in);

	idEndIndex = strlen (yTmpIn) - 1;

	outSize = sizeof (out);

	sprintf (out, "%s", "anonymous");

	idEndIndexChar = strchr (yTmpIn, '@');

	if (idEndIndexChar == NULL)
	{
		sprintf (out, "%s", "anonymous");
		return (0);
	}

	idStartIndexChar = strstr (yTmpIn, "sip:");

	if (idStartIndexChar == NULL)
	{
		idStartIndexChar = yTmpIn;
	}
	else
	{
		idStartIndexChar = idStartIndexChar + 4;
	}

	idEndIndexChar[0] = 0;

	sprintf (out, "%s", idStartIndexChar);

	return (0);

}								/*END: int extractIdFromSipUrl */

///This function reserves certain Call Manager ports as static (i.e. only outgoing calls on those ports).
/**
While Call Manager is starting up, Responsibility sends it a
DMOP_PORT_REQUEST_PERM message and in that message is a buffer that Call
Manager can do bitwise operations on to determine which ports should be
static and which ports should be dynamic.
*/
int
reserveStaticPorts (struct Msg_ReservePort *zpMsg_ReservePort)
{
	char            mod[] = { "reserveStaticPorts" };
	char            bufFromResp[MAX_APP_MESSAGE_DATA - 4];
	int             highestStaticPort;
	int             i, j;
	int             compressedArrayLengthOfInterest;
	unsigned int    mask;
	int             rc;
	int             passwordOfFirstGrantedPort = ++gAppPassword;
	char            message[MAX_LOG_MSG_LENGTH];

	sprintf (message,
			 "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d,maxCallNum:%d,"
			 "totalResources:%d,portRequests(%s)}",
			 zpMsg_ReservePort->opcode,
			 zpMsg_ReservePort->appCallNum,
			 zpMsg_ReservePort->appPid,
			 zpMsg_ReservePort->appRef,
			 zpMsg_ReservePort->appPassword,
			 zpMsg_ReservePort->maxCallNum,
			 zpMsg_ReservePort->totalResources,
			 zpMsg_ReservePort->portRequests);

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Processing %s.", message);

	zpMsg_ReservePort->appPassword = passwordOfFirstGrantedPort;
	zpMsg_ReservePort->opcode = RESOP_PORT_RESPONSE_PERM;

	memcpy (bufFromResp, zpMsg_ReservePort->portRequests,
			sizeof (zpMsg_ReservePort->portRequests));

	highestStaticPort = zpMsg_ReservePort->maxCallNum;

	if (zpMsg_ReservePort->totalResources <= MAX_NUM_CALLS)
	{
		maxNumberOfCalls = zpMsg_ReservePort->totalResources;
	}
	else
	{
		dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, WARN,
				   "Although ResourceDefTab.work has %d number of ports, "
				   "a maximum of %d ports can be run. Running only %d number of ports.",
				   zpMsg_ReservePort->totalResources,
				   MAX_NUM_CALLS, MAX_NUM_CALLS);

		maxNumberOfCalls = MAX_NUM_CALLS;
	}

	compressedArrayLengthOfInterest = highestStaticPort / 8;

	for (i = 0; i < MAX_PORTS; i++)
	{
		//zpStaticPortList[i] = PORT_DYNAMIC;
		gCall[i].permanentlyReserved = 0;
	}

	/*Read Static Calls from the file */
	{
	FILE           *yFpCallInfo = NULL;

	char            yStrFileCallInfo[100];

		sprintf (yStrFileCallInfo, ".staticCallInfo.%d", gDynMgrId);

		yFpCallInfo = fopen (yStrFileCallInfo, "r");

		if (yFpCallInfo == NULL)
		{
			;
		}
		else
		{
	char            staticResources[MAX_PORTS];

			memset (staticResources, 0, sizeof (staticResources));

			fread (staticResources, sizeof (staticResources), 1, yFpCallInfo);

			fclose (yFpCallInfo);

			dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "gStartPort=%d, gEndPort=%d, staticResources=%s",
					   gStartPort, gEndPort, staticResources);

			for (i = gStartPort; i <= gEndPort; i++)
			{
	int             zCall = i;

				if (staticResources[i] == 1)
				{
					dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "Setting port num=%d to permanentlyReserved",
							   i);

					sprintf (gCall[i].logicalName, "%s", "ArcIPDynMgr");
					gCall[i].permanentlyReserved = 1;
					gCall[i].callNum = i;
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, i, APPL_API, 0);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, i, APPL_STATUS,
									STATUS_INIT);

					gCall[i].appPassword = passwordOfFirstGrantedPort++;
				}
				else if (staticResources[i] == 2)	/*OCS Port */
				{
					dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Setting port num=%d to OCS Port", i);
					sprintf (gCall[i].logicalName, "%s", "ArcIPOCSMgr");
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, i, APPL_API, 0);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, i, APPL_STATUS,
									STATUS_IDLE);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, i, APPL_PID,
									getpid ());
					updateAppName (__LINE__, i, i);
				}
				else if (staticResources[i] == 3)	/*FAX Port */
				{
					dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, FAX_BASE,
							   INFO, "Setting port num=%d to FAX Port", i);
					sprintf (gCall[i].logicalName, "%s", "ArcIPFAXMgr");
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, i, APPL_API, 0);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, i, APPL_STATUS,
									STATUS_IDLE);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, i, APPL_PID,
									getpid ());
					updateAppName (__LINE__, i, i);
				}
				else
				{
					sprintf (gCall[i].logicalName, "%s", "ArcIPDynMgr");
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, i, APPL_API, 0);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, i, APPL_STATUS,
									STATUS_IDLE);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, i, APPL_PID,
									getpid ());
					updateAppName (__LINE__, i, i);
				}

			}
		}

		unlink (yStrFileCallInfo);
	}

	memcpy (zpMsg_ReservePort->portRequests, bufFromResp,
			sizeof (zpMsg_ReservePort->portRequests));

	/*
	 * Write response to Responsibility here 
	 */

	writeToResp (mod, (void *) zpMsg_ReservePort);

	return (0);

}								/*END: int reserveStaticPorts */

///This function writes the response to the application.
/**
After Call Manager and Media Manager are done with the DMOP operation sent to
them by an application they write a response to the appliation on the Response
fifo.  This function is what writes that response.  The function must be called
with a struct MsgToApp that is already populated in the function calling
writeGenericResponseToApp().
*/
int
writeGenericResponseToApp (int zLine, int zCall, struct MsgToApp *zpResponse, char *mod)
{
	struct MsgToApp response = *zpResponse;
	int             fd, rc;
	char            fifoName[256];
	int             yIntALeg = -1;

	if (gCall[zCall].leg == LEG_B)
	{
		yIntALeg = gCall[zCall].crossPort;
	}
	else
	{
		yIntALeg = zCall;
	}

	if (yIntALeg < 0 || yIntALeg > MAX_PORTS)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
				   TEL_INVALID_INTERNAL_VALUE, ERR,
				   "[%d] Failed to send message [op=%d, rtc=%d, cs=%d] to app. "
				   "Port(%d) is (%s) and A leg (%d) is invalid.", zLine,
					zpResponse->opcode, zpResponse->returnCode, gCall[zCall].callState,
					 zCall,
				   ((gCall[zCall].leg == LEG_A) ? "A LEG" : " B LEG"),
				   yIntALeg);

		return (-1);
	}

	if (arc_kill (gCall[yIntALeg].appPid, 0) == -1)
	{
		if (errno == ESRCH)
		{
			__DDNDEBUG__ (DEBUG_MODULE_FILE, "App doesn't exist for FIFO ",
						  gCall[yIntALeg].responseFifo, 0);
			return (-1);
		}
	}

	sprintf (fifoName, "%s", gCall[yIntALeg].responseFifo);

	if (!fifoName[0] && gCall[yIntALeg].lastResponseFifo[0])
	{
		sprintf (fifoName, "%s", gCall[yIntALeg].lastResponseFifo);
	}

	__DDNDEBUG__ (DEBUG_MODULE_FILE, "App FIFO ",
				  gCall[yIntALeg].responseFifo, 0);

	if (zpResponse->returnCode < 0)
	{

#if 0
		if (gCall[zpResponse->appCallNum].appPassword !=
			getPassword (yIntALeg))
		{
			/*
			 * Bogus password passed 
			 */
			response.returnCode = -1;
			strcpy (response.message, "Bogus password is passed");
		}
		else
		{
			/*
			 * Call is already dropped 
			 */
			response.returnCode = -3;
			strcpy (response.message, "Call is already dropped");
		}
#endif

	}

	if (gCall[yIntALeg].responseFifoFd <= 0)	/*Always -1 */
	{

#if 0
		if ((mknod (fifoName, S_IFIFO | 0666, 0) < 0) && (errno != EEXIST))
		{
			dynVarLog (__LINE__, yIntALeg, mod, REPORT_NORMAL, TEL_IPC_ERROR,
					   ERR, "[%d] Failed to create fifo (%s). [%d, %s]", fifoName,
					   zLine, errno, strerror (errno));

			return (-1);
		}
#endif

		fd = open (fifoName, O_RDWR | O_NONBLOCK);
		if (fd < 0)
		{
#if 0
			if (strstr (mod, "dropCallThread") == NULL)
			{
				dynVarLog (__LINE__, yIntALeg, mod, REPORT_NORMAL,
						   TEL_IPC_ERROR, ERR,
						   "[%d] Failed to open fifo (%s); unable to send message to app. rc=%d. [%d, %s]",
						   zLine, fifoName, fd, errno, strerror (errno));
			}
#endif
			// MR 4994
			if ( ( errno == 2 ) &&
			     ( ( zpResponse->opcode == DMOP_DISCONNECT ) ||
			       ( zpResponse->opcode == DMOP_EXITTELECOM ) ||
				   ( gCall[yIntALeg].callState == CALL_STATE_IDLE ) ||
				   ( gCall[yIntALeg].callState == CALL_STATE_CALL_RELEASED ) ||
				   ( gCall[yIntALeg].callState == CALL_STATE_CALL_CLOSED ) ||
				   ( gCall[yIntALeg].callState == CALL_STATE_CALL_CANCELLED ) ) )
			{
				dynVarLog (__LINE__, yIntALeg, mod, REPORT_VERBOSE,
					TEL_IPC_ERROR, INFO,
					"[%d] Failed to open fifo (%s); unable to send message [op=%d, rtc=%d, cs=%d] to app. rc=%d. [%d, %s]",
					zLine, fifoName, 
					zpResponse->opcode, zpResponse->returnCode, gCall[yIntALeg].callState,
					fd, errno, strerror (errno));
			}
			else
			{
				dynVarLog (__LINE__, yIntALeg, mod, REPORT_NORMAL,
					TEL_IPC_ERROR, ERR,
					"[%d] Failed to open fifo (%s); unable to send message [op=%d, rtc=%d, cs=%d] to app. rc=%d. [%d, %s]",
					zLine, fifoName, 
					zpResponse->opcode, zpResponse->returnCode, gCall[yIntALeg].callState,
					fd, errno, strerror (errno));
			}
			return (-1);
		}
	}

	rc = write (fd, &response, sizeof (struct MsgToApp));
	if (rc == -1)
	{
		dynVarLog (__LINE__, yIntALeg, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				"[%d] Failed to write to fifo (%s); unable to send message [op=%d, rtc=%d, cs=%d] to app. rc=%d  [%d, %s]",
				zLine, fifoName, 
				zpResponse->opcode, zpResponse->returnCode, gCall[yIntALeg].callState,
				rc, errno, strerror (errno));

		if (errno == EAGAIN || fd == -1)
		{
			;
		}
		else
		{
			close (fd);
		}

		return (-1);
	}

	dynVarLog (__LINE__, yIntALeg, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "[%d] %d = write(%s, [op=%d, rtc=%d, cs=%d]);", zLine, rc, fifoName,
				zpResponse->opcode, zpResponse->returnCode, gCall[yIntALeg].callState);

	close (fd);

	return (0);

}								/*END: int writeGenericResponseToApp */

///This function opens Call Manager's connection to the request fifo.
/**
In order for applications and Responsibility to communicate with Dynamic
Manager they use a fifo.  This fifo is the Request fifo.  Call Manager
reads all of its DMOP commands from this fifo, but first it has to open
it, which is what this function does.
*/
int
openRequestFifo ()
{
char            mod[] = { "openRequestFifo" };
int             i;

	//sprintf (gRequestFifo, "%s/%s", gFifoDir, REQUEST_FIFO);
	sprintf (gRequestFifo, "%s/%s.%d", gFifoDir, REQUEST_FIFO, gDynMgrId);

	if ((mknod (gRequestFifo, S_IFIFO | 0777, 0) < 0) && (errno != EEXIST))
	{

		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "mknod() failed.  Unable not create fifo (%s). [%d, %s]",
				   gRequestFifo, errno, strerror (errno));

		return (-1);
	}

	if ((gRequestFifoFd = open (gRequestFifo, O_RDWR | O_NONBLOCK)) < 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Failed to open request fifo (%s). rc=%d [%d, %s]",
				   gRequestFifo, gRequestFifoFd, errno, strerror (errno));

		return (-1);
	}

	return (0);

}								/*END: int openRequestFifo */

///This function finds the IP address of zHostName and sets zOut to that address.
int
getIpAddress (char *zHostName, char *zOut, char *zErrMsg)
{
	struct hostent *myHost;
	struct in_addr *addPtr;
	char          **p;

	int             err = 0;

	if (!zHostName || !*zHostName)
	{
		sprintf (zErrMsg, "%s", "Failed to get ip address empty host name.");

		return (-1);
	}

	myHost = gethostbyname (zHostName);

	if (myHost == NULL)
	{
		sprintf (zErrMsg, "Failed to get ip address of (%s).", zHostName);

		return (-1);
	}

	addPtr = (struct in_addr *) *myHost->h_addr_list;

	sprintf (zOut, "%s", (char *) inet_ntoa (*addPtr));

	return (0);

}								/*END: getIpAddress */

int
util_sleep (int Seconds, int Milliseconds)
{
	char            mod[] = { "util_sleep" };
	struct timeval  SleepTime;

	int             actualSeconds;
	int             actualMilliseconds;
	int             secondsFromMilli;

	secondsFromMilli = Milliseconds / 1000;
	actualSeconds = Seconds + secondsFromMilli;
	actualMilliseconds = Milliseconds % 1000;

	SleepTime.tv_sec = actualSeconds;
	SleepTime.tv_usec = actualMilliseconds * 1000;

	select (0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &SleepTime);

	return (0);

}								/*END: int util_sleep */

///This function returns the next available port or -1 if there are no available ports.
int getFreeCallNum (int direction)
{
	int             i;
    int 			yTmpReleasePort = -1;
	char            mod[] = "getFreeCallNum";
	char		    status_str[5];
	char			callDirection[50];

    // these are all stats just in case 
    
	int 			found = 0;
	int             app_running = 0;
	int             not_idle = 0;
	int             wrong_port = 0;
	int             wrong_state = 0;
	float			load_level = 0.0;
    
	static int      assignCounter = gDynMgrId;

	time_t          currentTime, yTmpReleaseTime;

	time (&currentTime);

	pthread_mutex_lock (&gFreeCallAssignMutex);

//	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//			   "inside getFreeCallNum with direction=%d, gStartPort=%d, gEndPort=%d",
//			   direction, gStartPort, gEndPort);

	if ( direction == INBOUND )
	{
		sprintf( callDirection, "%s", "INBOUND" );
	}
	else //if ( direction == OUTBOUND )
	{
		sprintf( callDirection, "%s", "OUTBOUND" );
	}



	for (i = gStartPort; i <= gEndPort; i++)
	{

		if ( ( direction == INBOUND ) && ( gCall[i].inboundAllowed == 0 ) )
		{
//            dynVarLog (__LINE__, i, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//                   "SHM: For channel %d, inbound calls are not allowed.", i);
            wrong_port++;
            continue;
        }

		if (gCall[i].permanentlyReserved == 0
			&& gCall[i].portState == PORT_STATE_ON)
		{
            (void) readFromRespShm(mod, tran_tabl_ptr, i, APPL_STATUS, status_str);

            if( (atoi(status_str) != STATUS_IDLE) )
            {
//                dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//                    "SHM: For channel[%d] str=[%s] val=%d, status is not idle.", i, status_str, atoi(status_str));
				not_idle++;
                continue;
            }

			if ((   gCall[i].callState == CALL_STATE_IDLE
				 || gCall[i].callState == CALL_STATE_CALL_CLOSED
				 || gCall[i].callState == CALL_STATE_CALL_TRANSFERCOMPLETE_CLOSED
				 || gCall[i].callState == CALL_STATE_CALL_TRANSFERCOMPLETE
				 || gCall[i].callState == CALL_STATE_CALL_RELEASED)
				&& (gCall[i].callState != CALL_STATE_RESERVED_NEW_CALL))
			{

				/*Check if previous app is gone */

				if (gCall[i].prevAppPid > 0 && gCall[i].canKillApp == YES)
				{

					if (arc_kill (gCall[i].prevAppPid, 0) == 0)
					{
#if 0
						__DDNDEBUG__ (DEBUG_MODULE_CALL,
									  "App exists even if status is idle", "",
									  gCall[i].prevAppPid);
#endif
						app_running++;
						continue;
					}

				}

				// difftime returns a float  

				if(yTmpReleasePort == -1 && (difftime(currentTime, gCall[i].lastReleaseTime) > 1.0))
				{
					yTmpReleaseTime = gCall[i].lastReleaseTime;
					yTmpReleasePort = i;
					found++;
				}

				if(yTmpReleasePort != -1 && (difftime(yTmpReleaseTime, gCall[i].lastReleaseTime) > 0.0))
				{
					yTmpReleasePort = i;
					yTmpReleaseTime = gCall[i].lastReleaseTime;
					found++;
					// time (&(gCall[i].lastReleaseTime));
				}
			} 
            else 
            {
				wrong_state++;
            }
		}

	}							/*for */

	if (yTmpReleasePort > -1)
	{
		gCall[yTmpReleasePort].prevAppPid = -1;
		setCallState (yTmpReleasePort, CALL_STATE_RESERVED_NEW_CALL);
		time (&(gCall[yTmpReleasePort].lastReleaseTime));
		gLastGivenPort = yTmpReleasePort;
	}

	pthread_mutex_unlock (&gFreeCallAssignMutex);

	if(yTmpReleasePort == -1)
	{
		load_level = (app_running + not_idle + wrong_state) / (48.0);

		if ( not_idle == gEndPort + 1 )
		{
			/*
			 ** Changed by Murali on 210527
			 ** Madhav wants to change the message to be clear and take off unwanted message.
			 */
			if ( direction == INBOUND )
			{
				dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_RESOURCE_NOT_AVAILABLE, INFO,
					"%d ports not idle. Unable to get free port for (%s) call because no ports are available. %s",
					not_idle, callDirection, "Rejecting the call.");
			}
			else
			{
				dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_RESOURCE_NOT_AVAILABLE, INFO,
					"%d ports not idle. Unable to get free port for (%s) call because no ports are available.",
					not_idle, callDirection);
			}
		}
		else
		{
			if ( direction == INBOUND )
			{
				dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_RESOURCE_NOT_AVAILABLE, ERR,
					"Unable to get free port for (%s) call. Ports not idle=%d, total ports configured=%d. %s",
					callDirection, not_idle, gEndPort + 1, "Rejecting the call." );
			}
			else
			{
				dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_RESOURCE_NOT_AVAILABLE, ERR,
					"Unable to get free port for (%s) call. Ports not idle=%d, total ports configured=%d.",
					callDirection, not_idle, gEndPort + 1);
			}
		}

    }

	if (direction == INBOUND)
	{
		gCall[yTmpReleasePort].callDirection = INBOUND;
	}
	else
	{
		gCall[yTmpReleasePort].callDirection = OUTBOUND;
	}
	dynVarLog (__LINE__, yTmpReleasePort, mod, REPORT_VERBOSE, TEL_BASE, INFO, 
		"Set gCall[%d].callDirection to %d.  INBOUND=%d.  OUTBOUND=%d",
		yTmpReleasePort, gCall[yTmpReleasePort].callDirection, INBOUND, OUTBOUND);

	return (yTmpReleasePort);

}								/*END: getFreeCallNum */

///This function returns the did == cid for a given call and restes port value
int
getCallNumResetCidAndDid (eXosip_event_t * zEvent, int cid, int did)
{
	int             i;
	char            mod[] = "getCallNumResetCidAndDid";
	char            zCall = -1;

	for (i = gStartPort; i <= gEndPort; i++)
	{
		if (cid == gCall[i].cid && (gCall[i].callState != CALL_STATE_RESERVED_NEW_CALL))	// && did == gCall[i].did)
		{

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Changing CID", "",
						  gCall[i].cid);

			gCall[i].cid = -1;
			return (i);
		}
	}

	return (-1);

}								/*END: getCallNumResetCidAndDid */

///This function returns the did == cid for a given call.
int
getCallNumFromCidAndDid (eXosip_event_t * zEvent, int cid, int did)
{
	int             i;
	char            mod[] = { "getCallNumFromCidAndDid" };
	char            zCall = -1;

	for (i = gStartPort; i <= gEndPort; i++)
	{
		if (gCall[i].callState != CALL_STATE_IDLE)
		{
			if (cid == gCall[i].cid)	// && did == gCall[i].did)
			{
				return (i);
			}
		}
	}

	return (-1);

}								/*END: getCallNumFromCidAndDid */

void
sigterm (int x)
{
	char            mod[] = { "sigterm" };
	int             zCall = -1;

	gCanReadSipEvents = 0;
	gCanReadRequestFifo = 0;

//	__DDNDEBUG__ (DEBUG_MODULE_ALWAYS, "SIGTERM", "", 0);

	dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
			   "SIGTERM received.  Shutting down.");

	return;
}

void
sigpipe (int x)
{
	char            mod[] = { "sigpipe" };
	int             zCall = -1;

	gCanReadSipEvents = 0;
	gCanReadRequestFifo = 0;

	__DDNDEBUG__ (DEBUG_MODULE_ALWAYS, "SIGPIPE", "", 0);

	return;
}

void
sigsegv (int x)
{
	char            mod[] = { "sigsegv" };
	int             zCall = -1;

	gCanReadSipEvents = 0;
	gCanReadRequestFifo = 0;

	__DDNDEBUG__ (DEBUG_MODULE_ALWAYS, "SIGSEGV", "", 0);

	return;
}

void
sigusr (int x)
{
	char            mod[] = { "sigsegv" };
	int             zCall = -1;

	gCanReadSipEvents = 0;
	gCanReadRequestFifo = 0;

	__DDNDEBUG__ (DEBUG_MODULE_ALWAYS, "SIGUSR", "", 0);

	return;
}

int
openSRClientRequestFifo (int zCall)
{
	char            mod[] = { "openSRClientRequestFifo" };
	int             fd = 0;
	int             x, i;
	char            lFifoName[128];

	if (gCall[zCall].srClientFifoFd != -1)
	{
		return (0);
	}

	sprintf (lFifoName, "/tmp/ReqAppToSRClient.%d", zCall % 24);

	if ((fd = open (lFifoName, O_RDWR)) < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Can't open request fifo (%s). rc=%d  [%d, %s]",
				   lFifoName, fd, errno, strerror (errno));

		return (-1);
	}

	for (i = zCall; i < maxNumberOfCalls; i = i + 24)
	{
		gCall[i].srClientFifoFd = fd;
	}

	return (0);
}

int
sendMsgToSRClient (int zCall, struct MsgToDM *zpMsgToDM)
{
	char            mod[] = { "sendMsgToSRClient" };
	int             rc;

	openSRClientRequestFifo (zCall);

	rc = write (gCall[zCall].srClientFifoFd, zpMsgToDM,
				sizeof (struct MsgToDM));
	if (rc == -1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "write() failed. Unable to write a message to SR client. rc=%d. [%d, %s]",
				   rc, errno, strerror (errno));

		return (-1);
	}

	return (0);
}

///This function sends the RMOP_REGISTER message to ArcSipRedirector.
/**
When using multiple Dynamic Managers ArcSipRedirector needs to know when a
Dynamic Manger is started.  Call Manager accomplishes this task by
sending an RMOP_REGISTER message to ArcSipRedirector.  This message tells
ArcSipRedirector the amount of ports that Call Manager has available, its id,
and it's pid.  It also tells ArcSipRedirector which port it is listing on.
*/

#define UNREG_FROM_REDIRECTOR 1

int
registerToRedirector (char *mod, int unreg=0)
{
	int             rc;

	struct Msg_Register yMsgRegister;

	char            yRedirectorFifo[256];

	int             fd = -1;

	int             zCall = -1;

	int             yIntWaitCount = 0;

	sprintf (yRedirectorFifo, "%s/RedirectorFifo", gFifoDir);

	while (gotStaticPortRequest == 0)	/*This is a global variable, set when RESOP_PORT_RESPONSE_PERM is received */
	{
		if (yIntWaitCount == 0)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, WARN,
					   "Waiting for RESOP_PORT_RESPONSE_PERM from ArcIPResp "
					   "before registering to ArcSipRedirector.");
		}

		yIntWaitCount++;
	    if((yIntWaitCount % 10) == 0){
			dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_BASE, WARN,
					   "Waiting for more than 10 Seconds for RESOP_PORT_RESPONSE_PERM from ArcIPResp!");
        }

		sleep (1);
	}

    if(unreg == UNREG_FROM_REDIRECTOR){
	  dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO, "Sending RMOP_DEREGISTER to fifo (%s)", yRedirectorFifo);
	  yMsgRegister.opcode = RMOP_DEREGISTER;
    } else {
      dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO, "Sending RMOP_REGISTER to fifo (%s)", yRedirectorFifo);
	  yMsgRegister.opcode = RMOP_REGISTER;
    }

	yMsgRegister.dmId = gDynMgrId;
	yMsgRegister.pid = getpid ();
	yMsgRegister.port = gSipPort;

	snprintf (yMsgRegister.ip, sizeof (yMsgRegister.ip), "%s", gHostIp);
	sprintf (yMsgRegister.requestFifo, "%s", gRequestFifo);

	if ((mknod (yRedirectorFifo, S_IFIFO | 0666, 0) < 0) && (errno != EEXIST))
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "mknod() failed.  Unable to create Redirector fifo (%s). rc=%d. [%d, %s]",
				   gToRespFifo, rc, errno, strerror (errno));

		return (-1);
	}

	if ((fd = open (yRedirectorFifo, O_RDWR)) < 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Can not open fifo (%s). rc=%d.  [%d, %s]",
				   yRedirectorFifo, rc, errno, strerror (errno));

		return (-1);
	}

	rc = write (fd, &(yMsgRegister), sizeof (yMsgRegister));
	if (rc == -1)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Failed to write to Redirector(%s). "
				   "rc=%d. [%d, %s]", yRedirectorFifo, rc, errno,
				   strerror (errno));

		return (-1);
	}

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO, "Wrote %d bytes to Redirector.", rc);

	close (fd);

	return (0);

}								/*END: int registerToRedirector */

///This function is used to write messages to Responsibility.
int
writeToResp (char *mod, void *zpMsg)
{
	int             zCall = -1;
	int             rc;
	static int      fd;
	static int      first_time = 1;
	char            message[128];
	int             sizeOfMsg;

	struct MsgToRes *lpTmpMsg;

	lpTmpMsg = (struct MsgToRes *) zpMsg;

	zCall = lpTmpMsg->appCallNum;

	if (zCall < 0)
	{
		zCall = -1;
	}

	__DDNDEBUG__ (DEBUG_MODULE_FILE, "Writing to Resp Fifo", gToRespFifo, fd);

	if (first_time == 1)
	{
		first_time = 0;
		if ((mknod (gToRespFifo, S_IFIFO | 0666, 0) < 0) && (errno != EEXIST))
		{
			dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
					   "mknod() failed. Can not create Responsibility fifo (%s). [%d, %s]",
					   gToRespFifo, errno, strerror (errno));

			return (-1);
		}

		if ((fd = open (gToRespFifo, O_RDWR)) < 0)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
					   "Can not open fifo (%s). rc=%d. [%d, %s]",
					   gToRespFifo, fd, errno, strerror (errno));

			return (-1);
		}
	}

	switch (*(int *) zpMsg)
	{
    case RESOP_START_NTOCS_APP_DYN:
	case RESOP_START_OCS_APP:
	case RESOP_START_FAX_APP:
		{
	struct MsgToRes *lpMsg;

			lpMsg = (struct MsgToRes *) zpMsg;

			sprintf (message,
					 "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d,ani:%s,dnis:%s,data:%s}",
					 lpMsg->opcode, lpMsg->appCallNum,
					 lpMsg->appPid, lpMsg->appRef,
					 lpMsg->appPassword, lpMsg->ani, lpMsg->dnis,
					 lpMsg->data);
		}

		gCall[zCall].permanentlyReserved = 0;

		sizeOfMsg = sizeof (struct MsgToRes);

		break;

	case RESOP_VXML_PID:			// BT-210
	case RESOP_FIREAPP:
		{
	struct MsgToRes *lpMsgToRes;

			lpMsgToRes = (struct MsgToRes *) zpMsg;

			sprintf (message,
					 "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d,dnis:%s,ani:%s}",
					 lpMsgToRes->opcode,
					 lpMsgToRes->appCallNum,
					 lpMsgToRes->appPid,
					 lpMsgToRes->appRef,
					 lpMsgToRes->appPassword,
					 lpMsgToRes->dnis, lpMsgToRes->ani);
		}
		sizeOfMsg = sizeof (struct MsgToRes);

		break;

	case RESOP_PORT_RESPONSE_PERM:
		{

	struct Msg_ReservePort *lpMsg_ReservePort;

			lpMsg_ReservePort = (struct Msg_ReservePort *) zpMsg;

			sprintf (message,
					 "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d,maxcallnum:%d,portrequests:%s}",
					 lpMsg_ReservePort->opcode,
					 lpMsg_ReservePort->appCallNum,
					 lpMsg_ReservePort->appPid,
					 lpMsg_ReservePort->appRef,
					 lpMsg_ReservePort->appPassword,
					 lpMsg_ReservePort->maxCallNum,
					 lpMsg_ReservePort->portRequests);
		}

		sizeOfMsg = sizeof (struct Msg_ReservePort);

		break;

	case RESOP_STATIC_APP_GONE:
		{

	struct MsgToRes *lpMsgToRes;

			lpMsgToRes = (struct MsgToRes *) zpMsg;

			sprintf (message,
					 "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d,dnis:%s,ani:%s}",
					 lpMsgToRes->opcode, lpMsgToRes->appCallNum,
					 lpMsgToRes->appPid, lpMsgToRes->appRef,
					 lpMsgToRes->appPassword, lpMsgToRes->dnis,
					 lpMsgToRes->ani);

		}
		sizeOfMsg = sizeof (struct MsgToRes);

	default:
		break;
	}

	rc = write (fd, zpMsg, sizeOfMsg);
	if (rc == -1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "Failed to write (%s) to Resp fifo (%s). "
				   "[%d, %s]", message, gToRespFifo, errno, strerror (errno));

		return (-1);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
			   "Wrote %d bytes to Resp; msg=(%s).", rc, message);

	return (0);

}								/*END: writeToResp */

///This function is used to map ARC error codes to SIP codes.
/**
When we need to terminate a call using DMOP_DROPCALL we are passed an ARC
status code, but to successfully terminate the call we need to send the
SIP caller a valid SIP message.  In order to do this, we use the
following function to map that ARC error code (causeCode) to a SIP error
code (the returned value).
*/
int
mapArcCauseCodeToSipError (char *causeCode, int *zError)
{

#if 0
case 508:
/**/ case 486:					/*Busy Here */
case 480:
	return (51);				/*Temporarily unavailable */
	break;

case 408:
	return (50);				/*Request Timed out */
	break;

case 403:
	return (51);				/*Service forbidden */
	break;

case 484:						/*Address incomplete */
case 503:						/*Service Unavailable */
case 404:
	return (52);				/*Not found */
	break;

case 500:						/*Interval server error */
default:
	return (21);				/*All other */
#endif

	int             yIntCauseCode = 404;

	if (!causeCode)
		return (0);

	if (!causeCode[0])
		return (0);

	if (strstr (causeCode, "UNASSIGNED_NUMBER"))
		yIntCauseCode = 484;
	else if (strstr (causeCode, "NORMAL_CLEARING"))
		yIntCauseCode = 480;
	else if (strstr (causeCode, "CHANNEL_UNACCEPTABLE"))
		yIntCauseCode = 403;
	else if (strstr (causeCode, "USER_BUSY"))
		yIntCauseCode = 486;
	else if (strstr (causeCode, "CALL_REJECTED"))
		yIntCauseCode = 403;
	else if (strstr (causeCode, "DEST_OUT_OF_ORDER"))
		yIntCauseCode = 480;
	else if (strstr (causeCode, "NETWORK_CONGESTION"))
		yIntCauseCode = 503;
	else if (strstr (causeCode, "REQ_CHANNEL_NOT_AVAIL"))
		yIntCauseCode = 503;
	else if (strstr (causeCode, "SEND_SIT"))
		yIntCauseCode = 480;
	else if (strstr (causeCode, "SUBSCRIBER_ABSENT"))
		yIntCauseCode = 486;
	else if (strstr (causeCode, "NO_ANSWER_FROM_USER"))
		yIntCauseCode = 480;
	else
		yIntCauseCode = 480;

	*zError = yIntCauseCode;

	return(0);
}								/*END: int mapArcCauseCodeToSipError */

///This function is used to map SIP error codes to ARC error codes which can be sent back to the application.
/**
In SIP when an error occurs we get an error code that is a number usually used
in the HTTP world.  This is not useful because ARC services uses different error
codes, so we wrote the following function that taks the argument zSipError,
which is the SIP error code, and returns the proper ARC error code.
*/
int
mapSipErrorToArcError (int zCall, int zSipError)
{
	char            yStrMsg[256];

	char            mod[] = { "mapSipErrorToArcError" };

	int             yRc = atoi (gSipCodes[zSipError].value);

#if 0
	sprintf (yStrMsg, "Returning %d for %d", yRc, zSipError);
	__DDNDEBUG__ (DEBUG_MODULE_CALL, yStrMsg, "", -1);
#endif

	return (yRc);

#if 0
	switch (zSipError)
	{
	case 508:
	case 600:
	case 486:					/*Busy Here */
	case 480:
	case 604:
		return (51);			/*Temporarily unavailable */
		break;

	case 408:
		return (50);			/*Request Timed out */
		break;

	case 403:					/*Service forbidden */
	case 503:
	case 606:
		return (52);			/*Service UnAvailable */
		break;

	case 484:					/*Address incomplete */
	case 404:
		return (52);			/*Not found */
		break;

	case 500:					/*Interval server error */
	default:
		return (21);			/*All other */
	}
#endif

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

/*RFC 3398*/

	400 Bad         Request 41 Temporary Failure
		401 Unauthorized 21 Call rejected (*)
		402 Payment required 21 Call rejected
		403 Forbidden 21 Call rejected
		404 Not found 1 Unallocated number
		405 Method not allowed 63 Service or option
		unavailable
		406 Not acceptable 79 Service / option not
		implemented (+)
		407 Proxy authentication required 21 Call rejected (*)
		408 Request timeout 102 Recovery on timer expiry
		410 Gone 22 Number changed
		(w / o diagnostic)
		413 Request Entity too long 127 Interworking (+)
		414 Request - URI too long 127 Interworking (+)
		415 Unsupported media type 79 Service / option not
		implemented (+)
		416 Unsupported URI Scheme 127 Interworking (+)
		420 Bad extension 127 Interworking (+)
		421 Extension Required 127 Interworking (+)
		423 Interval Too Brief 127 Interworking (+)
		480 Temporarily unavailable 18 No user responding
		481 Call / Transaction Does not Exist 41 Temporary Failure
		482 Loop Detected 25 Exchange - routing error
		483 Too many hops 25 Exchange - routing error
		484 Address incomplete 28 Invalid Number Format (+)
		485 Ambiguous 1 Unallocated number
		486 Busy here 17 User busy
		487 Request Terminated-- - (no mapping)
		488 Not Acceptable here-- - by Warning header
		500 Server internal error 41 Temporary failure
		501 Not implemented 79 Not implemented, unspecified
		502 Bad gateway 38 Network out of order
		503 Service unavailable 41 Temporary failure
		504 Server time - out 102 Recovery on timer expiry
		504 Version Not Supported 127 Interworking (+)
		513 Message Too Large 127 Interworking (+)
		600 Busy everywhere 17 User busy
		603 Decline 21 Call rejected
		604 Does not exist anywhere 1 Unallocated number
		606 Not acceptable-- - by Warning header
/*END: RFC 3398*/
#endif
}								/*END: int mapSipErrorToArcError */

///This function is used to log to the application whenever we can't create a thread.
int
processThreadCreationFailure (char *mod, char *threadName,
							  struct MsgToDM *pMsgToDM, int failureCode)
{
	struct MsgToApp response;
	int             rc;

	response.opcode = pMsgToDM->opcode;
	response.appCallNum = pMsgToDM->appCallNum;
	response.appPid = pMsgToDM->appPid;
	response.appRef = pMsgToDM->appRef;
//	response.appRef = 0;
	response.returnCode = -1;
	sprintf (response.message,
			 "Thread creation for %s failed. rc=%d. [%d, %s]", threadName,
			 failureCode, errno, strerror (errno));

	rc = writeGenericResponseToApp (__LINE__, pMsgToDM->appCallNum, &response, mod);

	return (0);

}								/*END: int processThreadCr... */

///This function sets the call specific variables back to default and closes the fifo's this call was using.
int
clearPort (int zCall)
{
	char            mod[] = { "clearPort" };

	int             j = 0;

//	ftime (&(gCall[zCall].lastDtmfTime));
	gettimeofday(&gCall[zCall].lastDtmfTime, NULL);

	gCall[zCall].isInBlastDial = 0;

	if (gCall[zCall].leg == LEG_B)
	{
		gCall[zCall].appPid = -1;
	}

	gCall[zCall].prevAppPid = gCall[zCall].appPid;

	if (gCall[zCall].responseFifoFd > 0)
	{
		close (gCall[zCall].responseFifoFd);
	}
	gCall[zCall].responseFifoFd = -1;

	if (gCall[zCall].responseFifo[0])
	{
		sprintf (gCall[zCall].lastResponseFifo, "%s",
				 gCall[zCall].responseFifo);
	}

	// BT-123
	gCall[zCall].sipFromUsername[0] = 0;
	gCall[zCall].sipFromHost[0] = 0;
	gCall[zCall].sipFromPort[0] = 0;

	gCall[zCall].sipToUsername[0] = 0;
	gCall[zCall].sipToHost[0] = 0;
	gCall[zCall].sipToPort[0] = 0;

	gCall[zCall].eXosipStatusCode = 0;
	gCall[zCall].eXosipReasonPhrase[0] = 0;

	gCall[zCall].leg = LEG_A;
	gCall[zCall].crossPort = -1;
	gCall[zCall].parent_idx = zCall;

	gCall[zCall].gUtteranceFile[0] = 0;
	gCall[zCall].mrcpServer[0] = 0;
	gCall[zCall].mrcpRtpPort = 0;
	gCall[zCall].mrcpRtcpPort = 0;

	gCall[zCall].speechRecFromClient = 0;
	gCall[zCall].speechRec = 0;

	gCall[zCall].waitForPlayEnd = 0;

	/*
	 * GLOBAL Integers
	 */
	gCall[zCall].GV_CallProgress = 0;
	gCall[zCall].GV_CancelOutboundCall = NO;
	gCall[zCall].GV_SRDtmfOnly = 0;
	gCall[zCall].GV_MaxPauseTime = 60;
	gCall[zCall].GV_PlayBackBeepInterval = 5;
	gCall[zCall].GV_KillAppGracePeriod = 5;
	gCall[zCall].GV_CallDuration = gMaxCallDuration;

	/*
	 * GLOBAL Strings
	 */
	gCall[zCall].GV_DtmfBargeinDigits[0] = '\0';
	gCall[zCall].GV_PlayBackPauseGreetingDirectory[0] = '\0';
	gCall[zCall].GV_CallingParty[0] = '\0';
	gCall[zCall].GV_CustData1[0] = '\0';
	gCall[zCall].GV_CustData2[0] = '\0';
	gCall[zCall].GV_DtmfBargeinDigitsInt = 0;
	gCall[zCall].GV_SkipTerminateKey[0] = '\0';
	gCall[zCall].GV_SkipRewindKey[0] = '\0';
	gCall[zCall].GV_PauseKey[0] = '\0';
	gCall[zCall].GV_ResumeKey[0] = '\0';
	gCall[zCall].GV_OutboundCallParm[0] = '\0';
	gCall[zCall].GV_InboundCallParm[0] = '\0';
	gCall[zCall].GV_SipUriVoiceXML[0] = '\0';

	gCall[zCall].GV_PreferredCodec[0] = '\0';
	gCall[zCall].GV_SipAuthenticationUser[0] = '\0';
	gCall[zCall].GV_SipAuthenticationPassword[0] = '\0';
	gCall[zCall].GV_SipAuthenticationId[0] = '\0';
	gCall[zCall].GV_SipAuthenticationRealm[0] = '\0';
	gCall[zCall].GV_SipFrom[0] = '\0';
	gCall[zCall].GV_SipAsAddress[0] = '\0';
	gCall[zCall].GV_DefaultGateway[0] = '\0';
	gCall[zCall].GV_RouteHeader[0] = '\0';
	gCall[zCall].GV_PAssertedIdentity[0] = '\0';
	gCall[zCall].GV_CallInfo[0] = '\0';
	gCall[zCall].GV_Diversion[0] = '\0';
	gCall[zCall].GV_CallerName[0] = '\0';


	gCall[zCall].crossPort = -1;
	gCall[zCall].rtpSendGap = 160;
	gCall[zCall].sendingSilencePackets = 1;
	gCall[zCall].dtmfAvailable = 0;
	gCall[zCall].speakStarted = 0;
	//gCall[zCall].rtp_session      = NULL;
	//gCall[zCall].rtp_session_in   = NULL;
	gCall[zCall].in_user_ts = 0;
	gCall[zCall].out_user_ts = 0;
	gCall[zCall].callNum = zCall;
	setCallState (zCall, CALL_STATE_IDLE);
	//gCall[zCall].callState = CALL_STATE_IDLE;
	gCall[zCall].stopSpeaking = 0;
	gCall[zCall].lastTimeThreadId = 0;
	gCall[zCall].outputThreadId = 0;
	//gCall[zCall].permanentlyReserved = 0;
	gCall[zCall].appPassword = 0;
	gCall[zCall].appPid = -1;
	//gCall[zCall].canKillApp = YES;

	gCall[zCall].lastError = CALL_NO_ERROR;

	gCall[zCall].srClientFifoFd = -1;
	gCall[zCall].ttsClientFifoFd = -1;

	gCall[zCall].telephonePayloadType = 96;
	gCall[zCall].telephonePayloadType_2 = 101;
	gCall[zCall].telephonePayloadType_3 = 120;
	gCall[zCall].telephonePayloadType_4 = 127;
	gCall[zCall].telephonePayloadPresent = 0;
    
	for (j = 0; j < MAX_THREADS_PER_CALL; j++)
	{
		gCall[zCall].threadInfo[j].appRef = -1;
		gCall[zCall].threadInfo[j].threadId = 0;
	}

	gCall[zCall].ttsRtpPort = (zCall * 2 + LOCAL_STARTING_RTP_PORT) + 700;
	gCall[zCall].ttsRtcpPort = (zCall * 2 + LOCAL_STARTING_RTP_PORT) + 701;

#ifdef SR_MRCP
	gCall[zCall].pFirstRtpMrcpData = NULL;
	gCall[zCall].pLastRtpMrcpData = NULL;

	gCall[zCall].gUtteranceFileFp = NULL;
	memset (gCall[zCall].gUtteranceFile, 0,
			sizeof (gCall[zCall].gUtteranceFile));

	gCall[zCall].rtp_session_mrcp = NULL;
#endif

	gCall[zCall].pFirstSpeak = NULL;
	gCall[zCall].pLastSpeak = NULL;
	gCall[zCall].pendingInputRequests = 0;
	gCall[zCall].pendingOutputRequests = 0;

	gCall[zCall].speakProcessWriteFifoFd = -1;
	gCall[zCall].speakProcessReadFifoFd = -1;

	memset (&(gCall[zCall].msgPortOperation), 0, sizeof (struct MsgToApp));

	if (gCall[zCall].permanentlyReserved == 1)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Setting port num=%d to permanentlyReserved.", zCall);

		writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
		writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS, STATUS_INIT);
	}
	else
	{
		writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
		writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS, STATUS_IDLE);
	}

	memset (gCall[zCall].GV_SipURL, 0, 2048);
	memset (gCall[zCall].audioCodecString, 0, 256);
	sprintf (gCall[zCall].audioCodecString, "%s", gPreferredCodec);
	memset (gCall[zCall].sdp_body_remote, 0, 4096);

	sprintf (gCall[zCall].call_type, "%s", "Voice-Only");
	memset (gCall[zCall].rdn, 0, 32);
	memset (gCall[zCall].ocn, 0, 32);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Setting isCallAnswered_noMedia = 0.");

	gCall[zCall].isCallAnswered_noMedia = 0;
	gCall[zCall].full_duplex = SENDRECV;

	gCall[zCall].amrIntCodec[0] = 97;
	for (int j = 1; j < 10; j++)
	{
		gCall[zCall].amrIntCodec[j] = -1;
	}

	memset (gCall[zCall].amrFmtp, 0, 256);
	memset (gCall[zCall].amrFmtpParams, 0, 256);
	gCall[zCall].isG729AnnexB = YES;

	if (gCall[zCall].remote_sdp != NULL)
	{
		sdp_message_free (gCall[zCall].remote_sdp);
		gCall[zCall].remote_sdp = NULL;
	}

	gCall[zCall].codecMissMatch = YES;
	gCall[zCall].presentRestrict = 0;
	memset (gCall[zCall].dnis, 0, sizeof (gCall[zCall].dnis));
	memset (gCall[zCall].ani, 0, sizeof (gCall[zCall].ani));
	memset (gCall[zCall].crossRef, 0, sizeof (gCall[zCall].crossRef));

	for (int i = 0; i < MAX_CONF_PORTS; i++)
	{
		gCall[zCall].conf_ports[i] = -1;
	}
	gCall[zCall].confUserCount = 0;

	memset (gCall[zCall].confID, 0, sizeof (gCall[zCall].confID));

// #ifdef ACU_LINUX

	gCall[zCall].isFaxReserverResourceCalled = 0;
	gCall[zCall].faxRtpPort = -1;
	gCall[zCall].sendFaxReinvite = 0;
	gCall[zCall].faxGotAPI = 0;
	gCall[zCall].faxAlreadyGotReinvite = 0;
	gCall[zCall].send200OKForFax = 0;
	gCall[zCall].sendFaxOnProceed = 0;
	gCall[zCall].faxAlreadyGotReinvite = 0;
	memset (&gCall[zCall].msgSendFax, 0, sizeof (gCall[zCall].msgSendFax));
	gCall[zCall].faxDelayTimerStarted = 0;
// #endif
	memset (gCall[zCall].transferAAI, 0, sizeof (gCall[zCall].transferAAI));
	memset (gCall[zCall].remoteAAI, 0, sizeof (gCall[zCall].remoteAAI));

    // user to user header joes Thu Apr 23 11:51:15 EDT 2015
    memset(gCall[zCall].local_UserToUser, 0, sizeof(gCall[zCall].local_UserToUser));
    gCall[zCall].local_UserToUser_len = 0;
    memset(gCall[zCall].remote_UserToUser, 0, sizeof(gCall[zCall].remote_UserToUser));
    gCall[zCall].remote_UserToUser_len = 0;
    // end user to user 

	gCall[zCall].isInitiate = 0;

	gCall[zCall].isInputThreadStarted = 0;
	gCall[zCall].isOutputThreadStarted = 0;
	gCall[zCall].tid = -1;
	gCall[zCall].reinvite_tid = -1;

	memset (gCall[zCall].originalContactHeader, 0, 1024);
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "originalContactHeader=%s.",
			   gCall[zCall].originalContactHeader);

	gCall[zCall].PRACKRSeqCounter = 1;
	gCall[zCall].PRACKSendRequire = 0;
	gCall[zCall].UAC_PRACKOKReceived = 0;
	gCall[zCall].UAC_PRACKRingCounter = 0;
	gCall[zCall].nextSendPrackSleep = 500;
	gCall[zCall].UAS_PRACKReceived = 0;
	gCall[zCall].sessionTimersSet = 0;

	gCall[zCall].remoteAgent |= USER_AGENT_UNKNOWN;
//	gCall[zCall].remoteAgentString[0] = '\0';

    gCall[zCall].issuedBlindTransfer = 0;
	sprintf (gCall[zCall].redirectedContacts, "\0");

	removeFromTimerList (zCall, MP_OPCODE_END_CALL);

	memset (gCall[zCall].pAIdentity_ip, 0, 256);

	gCall[zCall].callDirection = -1;			// MR 4964

    gCall[zCall].issuedBlindTransfer = 0;           // MR 4802

	gCall[zCall].canExitTransferCallThread = 0;		// BT-171
// MR 4642/4605
	gCall[zCall].alreadyClosed = 0;
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Set alreadyClosed to %d", gCall[zCall].alreadyClosed);
// END 4642/4605

	gCallInfo[zCall].Init (); 		// MR 5022 - session timers

	return (0);

}								/*END: int clearPort */

///This function is used to drop a call on port zCall.
int
dropCall (char *mod, int zCall, int zSync, struct MsgToDM *ptrMsgToDM)
{
	int             rc;
	pthread_attr_t  thread_attr;

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "", 0);

	// if enableed try and drop the signaled digits subscription 
	if (gSipSignaledDigits)
	{
		dropSipSignaledDigitsSubscription (zCall);
	}

	//removeFromTimerList(zCall, MP_OPCODE_END_CALL);

	if (gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
		|| gCall[zCall].callState == CALL_STATE_CALL_RELEASED
		|| gCall[zCall].callState == CALL_STATE_CALL_CLOSED)
	{
		return (0);
	}

	rc = pthread_attr_init (&thread_attr);
	if (rc != 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, DYN_BASE, ERR,
				   "Failed to intialize thread attributes. rc=%d. [%d, %s] Exiting.",
				   rc, errno, strerror (errno));

		return (-1);
	}

	rc = pthread_attr_setdetachstate (&thread_attr, PTHREAD_CREATE_DETACHED);
	if (rc != 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR,
				   ERR,
				   "pthread_attr_setdetachstate() failed.  Failed to set thread detach state. "
				   "rc=%d. [%d, %s].  Will not create dropCall Thread.", rc,
				   errno, strerror (errno));
		return (-1);
	}

	rc = pthread_create (&gCall[zCall].lastTimeThreadId, &thread_attr,
						 dropCallThread, (void *) ptrMsgToDM);

	if (rc != 0)
	{
		processThreadCreationFailure (mod, "dropCallThread", ptrMsgToDM, rc);
	}
	gCall[zCall].dropCallThreadId = gCall[zCall].lastTimeThreadId;
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, WARN,
			"Set dropCallThreadId to %d.", gCall[zCall].dropCallThreadId);


	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calling eXosip_terminate_call", "",
				  zCall);

	eXosip_lock (geXcontext);
	if (gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
		|| gCall[zCall].callState == CALL_STATE_CALL_RELEASED
		|| gCall[zCall].callState == CALL_STATE_CALL_CLOSED)
	{
		eXosip_unlock (geXcontext);
		return (0);
	}

	if (gCall[zCall].cid > -1 && gCall[zCall].did > -1)
	{
		if (gCall[zCall].callState == CALL_STATE_CALL_NEW)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
					   "Sending SIP Message call_terminate - CANCEL");
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Sending SIP Message BYE.");
		}

		rc = eXosip_call_terminate (geXcontext, gCall[zCall].cid, gCall[zCall].did);
		time (&(gCall[zCall].lastReleaseTime));
		setCallState (zCall, CALL_STATE_CALL_CLOSED);
	}
    else
    {
        rc = 99;
        time (&(gCall[zCall].lastReleaseTime));
        setCallState (zCall, CALL_STATE_CALL_CLOSED);
    }

	eXosip_unlock (geXcontext);

	if (rc < 0)
	{
		if ( rc == OSIP_NOTFOUND ) // OSIP_NOTFOUND is -6
		{
			// only print the error if the call is active
			if ( canContinue (mod, zCall) )	
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_EXOSIP_ERROR, ERR,
					   "eXosip_terminate_call(%d, %d) failed.  rc=%d [cs=%d].  Internal eXosip paramater not found.",
				   		gCall[zCall].cid, gCall[zCall].did, rc, gCall[zCall].callState);
			}
		}

		if ( rc == OSIP_SYNTAXERROR ) // OSIP_SYNTAXERROR is -5
		{
			// only print the error if the call is active
			if ( canContinue (mod, zCall) )	
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_EXOSIP_ERROR, ERR,
					   "eXosip_terminate_call(%d, %d) failed.  rc=%d [cs=%d].  Internal eXosip paramater is invalid.",
				   		gCall[zCall].cid, gCall[zCall].did, rc, gCall[zCall].callState);
			}
		}

		gCall[zCall].lastError = CALL_TERMINATE_FAILED;
	}
    else if (rc == 99)
    {
        dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
            "Skipping eXosip_terminate_call (%d, %d)",
            gCall[zCall].cid, gCall[zCall].did);
    }
	else
	{
		gCall[zCall].lastError = CALL_STATE_CALL_TERMINATE_CALLED;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "%d = eXosip_terminate_call(%d, %d);", rc,
				   gCall[zCall].cid, gCall[zCall].did);

	}

	if (gCall[zCall].responseFifo[0] == 0)
	{
		return (-1);
	}

	return (0);

}								/*END: dropCall */

///Fired by the function dropCall().
void           *
dropCallThread (void *z)
{
	char            mod[] = { "dropCallThread" };
	struct MsgToDM *ptrMsgToDM;
	struct Msg_AnswerCall msg;
	struct MsgToApp response;
	int             yRc;
	int             zCall;
	int             waitCount = 0;
	int             ringCountIn100msUnits;
	char            message[MAX_LOG_MSG_LENGTH];
	time_t			currentTime;		// BT-244
	time_t			markTime;		// BT-244

	int             yTmpAppPid = -1;
	int             yTmpAppCid = -1;
	int             yTmpAppDid = -1;
	int             yTmpGetPid = getpid ();

	ptrMsgToDM = (struct MsgToDM *) z;

	msg = *(struct Msg_AnswerCall *) z;

	zCall = msg.appCallNum;

	yTmpAppPid = gCall[zCall].appPid;
	yTmpAppCid = gCall[zCall].cid;
	yTmpAppDid = gCall[zCall].did;

	sprintf (message, "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d,numRings:%d}",
			 msg.opcode,
			 msg.appCallNum,
			 msg.appPid, msg.appRef, msg.appPassword, msg.numRings);

	dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Processing %s. from thread %d", message, pthread_self());

	{		// BT-177
		char	fName[128];
		sprintf(fName, "/tmp/invite.headers.%d", zCall);
		if (access (fName, F_OK) == 0)
		{
			(void) unlink(fName);
		}
	}		

	if (gCall[zCall].permanentlyReserved == 1)
	{
		struct timeval	yTmpTimeb;
//	struct timeb    yTmpTimeb;

//		ftime (&yTmpTimeb);
		gettimeofday(&yTmpTimeb, NULL);

		yRc = addToTimerList (__LINE__, zCall, MP_OPCODE_STATIC_APP_GONE,
							  gCall[zCall].msgToDM, yTmpTimeb, -1, -1, -1);
		JSPRINTF(" %s() adding static app gone for zcall=%d\n", __func__, zCall);

		__DDNDEBUG__ (DEBUG_MODULE_MEMORY, "addToTimerList", "returned ",
					  yRc);

		if (yRc < 0)
		{
			;
		}
	}
	else if (gCall[zCall].leg == LEG_A && gCall[zCall].canKillApp == YES)
	{

		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Registering to kill", "",
					  gCall[zCall].msgToDM.appPid);

	struct timeval	yTmpTimeb;
	//struct timeb    yTmpTimeb;

//		ftime (&yTmpTimeb);
		gettimeofday(&yTmpTimeb, NULL);

		yTmpTimeb.tv_sec += gCall[zCall].GV_KillAppGracePeriod;

		yRc = addToTimerList (__LINE__, zCall,
							  MP_OPCODE_TERMINATE_APP,
							  gCall[zCall].msgToDM, yTmpTimeb, -1, -1, -1);

#if 0
		if (yRc >= 0)
		{
			yTmpTimeb.tv_sec += 3;
			addToTimerList (__LINE__, zCall, MP_OPCODE_KILL_APP,
							gCall[zCall].msgToDM, yTmpTimeb, -1, -1, -1);
		}
#endif

		__DDNDEBUG__ (DEBUG_MODULE_MEMORY, "addToTimerList", "returned ",
					  yRc);

		if (yRc < 0)
		{
			;
		}
	}

	response.returnCode = 0;

	time(&markTime);		// BT-244
	while (gCall[zCall].callState != CALL_STATE_CALL_RELEASED
		   && gCall[zCall].lastError != CALL_TERMINATE_FAILED
		   && gCall[zCall].callState != CALL_STATE_CALL_CLOSED)
	{
		waitCount++;

		time(&currentTime);					// BT-244
		if ( currentTime > (markTime + 1) )		// BT-244
		{
			if (gCall[zCall].dropCallThreadId == -99 )
			{
				gCall[zCall].dropCallThreadId = 0;
				dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, INFO,
							"DropCall thread (%d) failed to exit from previous call. Exiting now.",
							pthread_self ());
				pthread_detach (pthread_self ());
				pthread_exit (NULL);
				return (NULL);
			}
		}

		if (waitCount >= 2000)
		{
			//dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
			//		   "Call terminate timed out. Exiting from dropCallThread, Current Call State=%d", gCall[zCall].callState);

			if(yTmpAppPid == -1 && gCall[zCall].appPid == -1)
			{
				if(gCall[zCall].callState != CALL_STATE_CALL_NEW)
				{
					//MR 4858: If not CALL_STATE_CALL_NEW,  reset it.
					setCallState (zCall, CALL_STATE_IDLE);
				}
				
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, DYN_BASE, ERR,
					"Call terminate timed out. Exiting from dropCallThread, Current Call State=%d (%d, %d)", 
					gCall[zCall].callState, yTmpAppPid, gCall[zCall].appPid);
				gCall[zCall].dropCallThreadId = 0;  // BT-244
				pthread_detach (pthread_self ());
				pthread_exit (NULL);
				return (NULL);
			}

			if (yTmpAppPid != gCall[zCall].appPid)
			{
				if(gCall[zCall].callState != CALL_STATE_CALL_NEW)
				{
					//MR 4858: If not CALL_STATE_CALL_NEW,  reset it.
					setCallState (zCall, CALL_STATE_IDLE);
				}

				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
						   TEL_SIP_SIGNALING, ERR,
						   "Call terminate timed out for yTmpAppPid=%d and appPid=%d. "
						   "Resetting the port, Not writing response to app.", yTmpAppPid,
						   gCall[zCall].appPid);
			} else {
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE,
						   ERR,
						   "Call terminate timed out for yTmpAppPid=%d and appPid=%d.  "
						   "Writing response to app, resetting the port and exiting dropCallThread.",
						   yTmpAppPid, gCall[zCall].appPid);

				setCallState (zCall, CALL_STATE_IDLE);
				//gCall[zCall].callState = CALL_STATE_IDLE;
				response.returnCode = -2;
				response.opcode = DMOP_DISCONNECT;
				response.appCallNum = msg.appCallNum;
				response.appPid = msg.appPid;
				response.appRef = msg.appRef;
				response.appPassword = msg.appPassword;
				yRc = writeGenericResponseToApp (__LINE__, zCall, &response, mod);

				if (gCall[zCall].leg == LEG_B)
				{
					gCall[zCall].prevAppPid = -1;
					gCall[zCall].appPid = -1;
				}
				else
				{
					gCall[zCall].prevAppPid = gCall[zCall].appPid;
					gCall[zCall].appPid = -1;
				}

				gCall[zCall].responseFifo[0] = 0;
				gCall[zCall].cid = -1;
				gCall[zCall].did = -1;
			}

			gCallsFinished++;	//DDN: 20090928
		    if ( gCallsFinished > gCallsStarted ) {
			        dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, WARN,
				   "DEBUG: Incremented gCallsFinished to %d, gCallStarted=%d  Exiting %s() with thread id=%d",
					gCallsFinished, gCallsStarted, __func__, pthread_self());
		    }
			
			gCall[zCall].dropCallThreadId = 0;  // BT-244
			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}

		usleep (10 * 1000);
	}

	if (gCall[zCall].lastError == CALL_TERMINATE_FAILED)
	{
		response.returnCode = -1;
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, WARN,
				   "Call terminate failed. notifying application.");
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Call released, notifying application.");
	}

	if (gCall[zCall].leg == LEG_A &&
		yTmpAppPid != gCall[zCall].appPid &&
		yTmpAppPid > 0 &&
		gCall[zCall].appPid > 0 &&
		gCall[zCall].appPid != yTmpGetPid && yTmpAppPid != yTmpGetPid)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, INFO,
				   "Call released but not notifying application since pid has changed from %d to %d",
				   yTmpAppPid, gCall[zCall].appPid);
	}
	else
	{
		setCallState (zCall, CALL_STATE_IDLE);
		//gCall[zCall].callState = CALL_STATE_IDLE;
		response.opcode = DMOP_DISCONNECT;
		response.appCallNum = msg.appCallNum;
		response.appPid = msg.appPid;
		response.appRef = msg.appRef;
		response.appPassword = msg.appPassword;

		if (gCall[zCall].leg == LEG_A)
		{
			yRc = writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
			if ( (gCall[zCall].permanentlyReserved == 1) || (gShutDownFlag == 1) )
			{
				writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS, STATUS_INIT);
				if (gShutDownFlag == 1)
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						   "gShutDownFlag = 1; setting port num=%d to INIT status.", zCall);
				}
			}
			else
			{
				writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS,
								STATUS_IDLE);
			}
			//updateAppName(__LINE__, zCall, zCall);
		}
		else
		{
			writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
            if(gShutDownFlag == 1){
			   writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS, STATUS_INIT);
			    dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						   "gShutDownFlag = 1; setting port num=%d to INIT status.", zCall);
            } else {
			   writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS, STATUS_IDLE);
            }
			writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_PID, getpid ());
			updateAppName (__LINE__, zCall, zCall);
		}

		if (gCall[zCall].leg == LEG_A)
		{
			gCall[zCall].prevAppPid = gCall[zCall].appPid;
			gCall[zCall].appPid = -1;
		}
		else
		{
			gCall[zCall].prevAppPid = -1;
			gCall[zCall].appPid = -1;
		}
		gCall[zCall].responseFifo[0] = 0;
		gCall[zCall].cid = -1;
		gCall[zCall].did = -1;
		gCallsFinished++;
		if ( gCallsFinished > gCallsStarted )
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, WARN,
				   "DEBUG: Incremented gCallsFinished to %d, gCallStarted=%d  ",
					gCallsFinished, gCallsStarted);
		}
		if (gShutDownFromRedirector == 1 && gSipRedirection)
		{
#if 0
			if (gCallsStarted <= gCallsFinished)
			{

				dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, DYN_BASE, INFO,
						   "Restarting ArcIPDynMgr (%d)."
						   " gShutDownFromRedirector = 1, gCallsStarted = %d gCallsFinished = %d",
						   gDynMgrId, gCallsStarted, gCallsFinished);

//				gCanReadSipEvents = 0;
//				sleep (10);
//				gCanReadRequestFifo = 0;
			}
#endif
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calls started", "",
						  gCallsStarted);
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calls finished", "",
						  gCallsFinished);

			yRc = canDynMgrExit ();

			if (yRc == -99)
			{
                doDynMgrExit(zCall, __func__, __LINE__);
			}
		}
		else if (!gSipRedirection && gCallMgrRecycleBase > 0)
		{
			if ((gCallsStarted >
				(gCallMgrRecycleBase + gCallMgrRecycleDelta * gDynMgrId)) && arc_can_recycle(RECYCLEPIDFILE, gDynMgrId))
			{
	char            yStrPortStatusFile[256];

				sprintf (yStrPortStatusFile, ".portStatus.%d", gDynMgrId);
				unlink (yStrPortStatusFile);

				if (gShutDownFlag == 0)
				{
					dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "Removing (%s) ArcIPDynMgr (%d). "
							   "gShutDownFlag = 1, gCallsStarted = %d.",
							   yStrPortStatusFile, gDynMgrId, gCallsStarted);

					time (&gShutDownFlagTime);
				}

                arc_recycle_down_file(RECYCLEPIDFILE, gDynMgrId);
				gShutDownFlag = 1;
			}
			else
			{
				gShutDownFlag = 0;
			}

			yRc = canDynMgrExit ();

			if ((yRc == -99) && (gCallsStarted <= gCallsFinished) && (gShutDownFlag == 1))
			{
                doDynMgrExit(zCall, __func__, __LINE__);
			}
			dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_CALLMGR_SHUTDOWN, WARN, "%d = canDynMgrExit()", yRc);

#if 0
			if (yRc == -99 && gShutDownFlag == 1)
			{
				if ((gCallsStarted > gCallsFinished) || (gShutDownFlag != 1))
				{				// don't log the same message again
					dynVarLog (__LINE__, -1, mod, REPORT_DETAIL,
							   TEL_CALLMGR_SHUTDOWN, WARN,
							   "Restarting ArcIPDynMgr (%d)."
							   " gShutDownFlag = 1, gCallsStarted = %d gCallsFinished = %d",
							   gDynMgrId, gCallsStarted, gCallsFinished);
				}

				gCanReadSipEvents = 0;
				sleep (10);
				gCanReadRequestFifo = 0;
			}
#endif
		}
	}

	if (gCall[zCall].leg == LEG_B)
	{
		gCall[zCall].prevAppPid = -1;
		gCall[zCall].appPid = -1;
	}

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, DYN_BASE, INFO,
		"Exiting dropCallThread from thread %d", pthread_self());	// BT-244

	gCall[zCall].dropCallThreadId = 0;  // BT-244
	pthread_detach (pthread_self ());
	pthread_exit (NULL);
	return (NULL);

}								/*END: dropCallThread */

///This is the function we use to answer calls and is fired whenever we see a DMOP_ANSWERCALL.
/**
In a SIP call after we receive an invite message we must then send back a 200OK
message with the negotiated codecs in the sdp body of the 200OK message.  This
thread performs that task.  It also waits until we have received an ACK for
that 200OK before it exits.
*/
void           *
answerCallThread (void *z)
{
	char            mod[] = { "answerCallThread" };
	struct MsgToDM *ptrMsgToDM;
	struct Msg_AnswerCall msg;
	struct MsgToApp response;
	int             yRc = 0;
	int             zCall;
	int             waitCount = 0;
	int             ringCountIn100msUnits;
	char            message[MAX_LOG_MSG_LENGTH];
	char            yStrLocalSdpPort[15];
	int             sendRequest = 0;

	size_t          stack_size;
	struct timeval    yCurrentTime, yAnswerTime, yNext200Time;

	long            yLongNext200Time;

	int             yInt200SendCounter = 1;
	int             yIntLoopCounter = 0;
	int             yIntSleepMS = 10;

	osip_message_t *answer = NULL;
	osip_message_t *reAnswer = NULL;
	osip_message_t *ringing = NULL;

	pthread_attr_getstacksize (&gThreadAttr, &stack_size);

//    fprintf(stderr, " %s() stacksize = %d\n", __func__, stack_size);
	gettimeofday(&yNext200Time, NULL);
//	ftime (&yNext200Time);

	ptrMsgToDM = (struct MsgToDM *) z;
	msg = *(struct Msg_AnswerCall *) z;

	// this sets the initial ref for bridged calls 
	//gCall[zCall].parent_idx = zCall;
	//gCallSetCrossRef(gCall, MAX_PORTS, zCall, &gCall[zCall], zCall);
	// 
	zCall = msg.appCallNum;

	sprintf (yStrLocalSdpPort, "%d", gCall[zCall].localSdpPort);
	gCall[zCall].parent_idx = zCall;
	gCallSetCrossRef (gCall, MAX_PORTS, zCall, &gCall[zCall], zCall);

	response.returnCode = 0;
	response.opcode = msg.opcode;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.appPassword = msg.appPassword;
	sprintf (response.message, "BANDWIDTH=%d", gCall[zCall].bandwidth);

     /*
        added for Voiceport and sending a 486 without a 180 when 
        an out of license error is made 
     */

    if(gSipOutOfLicenseResponse == 486){
       dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
                       "Sending SIP Message 180 Ringing, out of license error message set to %d", gSipOutOfLicenseResponse);
        eXosip_lock (geXcontext);
            // build 180 by hand and send 
        yRc = eXosip_call_build_answer (geXcontext, gCall[zCall].tid, 180, &answer);
        arc_fixup_contact_header (answer, gContactIp, gSipPort, zCall);

        yRc = eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 180, answer);
        eXosip_unlock (geXcontext);
    }

	sprintf (message, "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d,numRings:%d}",
			 msg.opcode, msg.appCallNum, msg.appPid, msg.appRef,
			 msg.appPassword, msg.numRings);

	dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Processing %s", message);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
			   "Calling eXosip_call_send_answer(); earlyMedia is set to %d.", gInboundEarlyMedia);
	//yRc = parseSdpMessage (zCall, __LINE__, gCall[zCall].sdp_body_remote_new, gCall[zCall].remote_sdp);

	if (gCall[zCall].codecMissMatch == YES)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
				   "Sending SIP Message 415 Unsupported Media Type.");

		eXosip_lock (geXcontext);
		eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 415, 0);
		sprintf (gCall[zCall].GV_CustData1, "%s", "415");

		sprintf (gCall[zCall].GV_CustData2,
				 "AUDIO_CODEC=%s"
				 "&FRAME_RATE=%d",
				 gCall[zCall].audioCodecString,
				 -1);

		writeCDR ("process_CALL_NEW", zCall, "RC", 20014, -1);	//DDN_TODO: SNMP SetRequest
		//eXosip_call_terminate (geXcontext, eXosipEvent->cid, eXosipEvent->did);
		eXosip_unlock (geXcontext);

		response.returnCode = -1;

		sprintf (response.message, "%s", "TEL_AnswerCall Failed");

		writeGenericResponseToApp (__LINE__, zCall, &response, mod);

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return (NULL);

	}

	dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE,
							   WARN, "gInboundEarlyMedia=%d", gInboundEarlyMedia);
	if (gInboundEarlyMedia == 0)
	{
// MR3069:UAS
		if (gCall[zCall].PRACKSendRequire)
		{
			while (1)
			{
				if (gCall[zCall].UAS_PRACKReceived)
				{
					break;
				}

				if (gCall[zCall].nextSendPrackSleep > 32000)
				{

					dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE,
							   WARN,
							   "Failed to receive PRACK for T1 * 64 seconds. Rejecting the call. "
							   "Sending SIP Message 504");

					eXosip_lock (geXcontext);
					yRc =
						eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 504, NULL);
					eXosip_unlock (geXcontext);

					response.returnCode = -1;

					sprintf (response.message, "TEL_AnswerCall Failed");

					writeGenericResponseToApp (__LINE__, zCall, &response, mod);

					pthread_detach (pthread_self ());
					pthread_exit (NULL);
					return (NULL);
				}

				usleep (gCall[zCall].nextSendPrackSleep * 1000);

				ringing = NULL;

				yRc = buildPRACKAnswer (zCall, &ringing);
				if (yRc == 0)
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "Sending SIP Message 180 Ringing with PRACK.");

					eXosip_lock (geXcontext);
					yRc =
						eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 180,
												 ringing);
					eXosip_unlock (geXcontext);

					gCall[zCall].nextSendPrackSleep *= 2;
				}
				else
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE,
							   WARN,
							   "Failed to answer call %d = buildPRACKAnswer. "
							   "Sending SIP Message 400 Bad Request.", yRc);
					eXosip_lock (geXcontext);
					yRc =
						eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 400, NULL);
					eXosip_unlock (geXcontext);

					response.returnCode = -1;

					sprintf (response.message, "TEL_AnswerCall Failed");

					writeGenericResponseToApp (__LINE__, zCall, &response, mod);

					pthread_detach (pthread_self ());
					pthread_exit (NULL);
					return (NULL);
				}
			}
		}
		else
		{
#if 0
		  if(gSipOutOfLicenseResponse == 486){
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Sending SIP Message 180 Ringing.");

			eXosip_lock (geXcontext);
			// build 180 by hand and send 
			yRc = eXosip_call_build_answer (geXcontext, gCall[zCall].tid, 180, &answer);
			arc_fixup_contact_header (answer, gContactIp, gSipPort, zCall);

			yRc = eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 180, answer);
			eXosip_unlock (geXcontext);
          }
#endif
		}

		if (yRc < 0)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
					   "Failed to answer call %d = eXosip_call_send_answer. "
					   "Sending SIP Message 400 Bad Request.", yRc);
			answer = NULL;

			eXosip_lock (geXcontext);
			yRc = eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 400, NULL);
			eXosip_unlock (geXcontext);

			response.returnCode = -1;

			sprintf (response.message, "%s", "TEL_AnswerCall Failed");

			writeGenericResponseToApp (__LINE__, zCall, &response, mod);

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}
	}

	answer = NULL;

    if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED                        // MR 4950
                 || gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
                 || gCall[zCall].callState == CALL_STATE_CALL_TERMINATE_CALLED
                 || gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
    {   
        ;
    }
    else
    {  
		eXosip_lock (geXcontext);
		yRc = eXosip_call_build_answer (geXcontext, gCall[zCall].tid, 200, &answer);
		osip_message_clone (answer, &reAnswer);
		eXosip_unlock (geXcontext);
	}

	if (answer == NULL || reAnswer == NULL)
	{
        gCall[zCall].cid = -1;
        gCall[zCall].did = -1;

    	if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED                        // MR 4950
                 || gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
                 || gCall[zCall].callState == CALL_STATE_CALL_TERMINATE_CALLED
                 || gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
	    {   
            dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_SIP_SIGNALING,
                   WARN,
                    "Call was cancelled by the remote end. [cs=%d]  No attempt to send answer will be made.  tid=%d. "
                    "Sending SIP Message 400 Bad Request.", gCall[zCall].callState, gCall[zCall].tid);
            __DDNDEBUG__ (DEBUG_MODULE_SIP, "Call was cancelled by the remote end.  No attempt to send answer will be made.", "tid is ",
					gCall[zCall].tid);

		}
		else
		{
			if ( ( yRc == -3 ) && ( gCall[zCall].callState == CALL_STATE_CALL_ACK ) ) // -3 == OSIP_WRONG_STATE
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_SIP_SIGNALING, ERR,
                    "Failed to build answer for call because the call has already been answered. [cs=%d] "
                    " Sending SIP Message 400 Bad Request.",
                    gCall[zCall].callState);
			}
			else
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_SIP_SIGNALING,
                   ERR,
                    "Failed to build answer for call. [cs=%d] context=%p, %d = eXosip_call_build_answer()."
                    " tid=%d. answer=%p clone=%p."
                    " Sending SIP Message 400 Bad Request.",
                    gCall[zCall].callState, geXcontext, yRc, gCall[zCall].tid, answer, reAnswer);
	        	__DDNDEBUG__ (DEBUG_MODULE_SIP, "Failed to build answer", "tid is ", gCall[zCall].tid);
			}
		}

		response.returnCode = -1;

		sprintf (response.message, "%s", "TEL_AnswerCall Failed");

        dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, WARN,
                   "Sending SIP Message 400 Bad Request.");

		eXosip_lock (geXcontext);
		yRc = eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 400, NULL);
		eXosip_unlock (geXcontext);

		writeGenericResponseToApp (__LINE__, zCall, &response, mod);
		writeCDR ("process_CALL_NEW", zCall, "RC", 20014, -1);	//DDN_TODO: SNMP SetRequest
		memset (gCall[zCall].GV_CustData1, 0,
				sizeof (gCall[zCall].GV_CustData1));

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return (NULL);
	}

	if (gCall[zCall].remoteRtpPort > 0)
	{
			yRc = createSdpBody (zCall,
								 yStrLocalSdpPort,
								 gCall[zCall].codecType,
								 -2, gCall[zCall].telephonePayloadType);
	}
	else
	{
		yRc = createSdpBody (zCall, yStrLocalSdpPort, -1, -2, -1);
	}

	osip_message_set_body (answer, gCall[zCall].sdp_body_local,
						   strlen (gCall[zCall].sdp_body_local));
	osip_message_set_content_type (answer, "application/sdp");

	osip_message_set_body (reAnswer, gCall[zCall].sdp_body_local,
						   strlen (gCall[zCall].sdp_body_local));
	osip_message_set_content_type (reAnswer, "application/sdp");

	eXosip_lock (geXcontext);
	//arc_fixup_contact_header(answer, gHostIp, gSipPort, zCall);   
	arc_fixup_contact_header (answer, gContactIp, gSipPort, zCall);
	eXosip_unlock (geXcontext);

	if (gSipSignaledDigits)
	{
		incomingCallWithSipSignaledDigits (answer, zCall);
	}

	osip_uri_param_t *yToTag = NULL;
	osip_uri_param_t *yFromTag = NULL;

	gCall[zCall].sipTo = osip_message_get_to (answer);
	gCall[zCall].sipFrom = osip_message_get_from (answer);

	osip_to_get_tag (gCall[zCall].sipTo, &yToTag);
	osip_to_get_tag (gCall[zCall].sipFrom, &yFromTag);

	if (yToTag && yToTag->gvalue)
	{
		sprintf (gCall[zCall].tagToValue, "%s", yToTag->gvalue);
		__DDNDEBUG__ (DEBUG_MODULE_SIP, "Incoming tagToValue",
					  gCall[zCall].tagToValue, zCall);
	}

	if (yFromTag && yFromTag->gvalue)
	{
		sprintf (gCall[zCall].tagFromValue, "%s", yFromTag->gvalue);
		__DDNDEBUG__ (DEBUG_MODULE_SIP, "Incoming tagFromValue",
					  gCall[zCall].tagFromValue, zCall);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Sending SIP Message 200 OK.");

	if ( gCall[zCall].sessionTimersSet )
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
			"session timer flag is set; setting session timers in response.");
		(void)setSessionTimersResponse(zCall, __LINE__, mod, answer);
		if ( doSessionExpiry(zCall, __LINE__) == 0 )
		{
			gCall[zCall].sessionTimersSet = 0;
		}
	}

	eXosip_lock (geXcontext);
	yRc = eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 200, answer);
	gCall[zCall].SentOk = 1;
	eXosip_unlock (geXcontext);

	if (yRc < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_SIP_SIGNALING,
				   ERR,
				   "Failed to answer call %d = eXosip_call_send_answer(); tid=%d",
				   yRc, gCall[zCall].tid);

		response.returnCode = -1;

		sprintf (response.message, "%s", "TEL_AnswerCall Failed");

		writeGenericResponseToApp (__LINE__, zCall, &response, mod);

		if (reAnswer != NULL)
		{
			osip_message_free (reAnswer);
			reAnswer = NULL;
		}

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return (NULL);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
			   " %d = eXosip_call_send_answer", yRc);

//	ftime (&yCurrentTime);
	gettimeofday(&yCurrentTime, NULL);

	yAnswerTime.tv_sec = yCurrentTime.tv_sec + (msg.numRings * 6) + 20;

	//ftime (&yNext200Time);
	gettimeofday(&yNext200Time, NULL);
	yNext200Time.tv_usec = yNext200Time.tv_usec / 1000;

	yLongNext200Time = yNext200Time.tv_sec * 1000 + yNext200Time.tv_usec + 500;

	while (gCall[zCall].callState != CALL_STATE_CALL_ACK
		   || gCall[zCall].isCallAnswered_noMedia == 1)
	{
		//ftime (&yCurrentTime);
		gettimeofday(&yCurrentTime, NULL);
		yCurrentTime.tv_usec = yCurrentTime.tv_usec / 1000;

		if ((yCurrentTime.tv_sec * 1000 + yCurrentTime.tv_usec) >
			(yLongNext200Time))
		{
	char            yStrNext200Msg[256];

			sprintf (yStrNext200Msg,
					 "Current time(%d) Next 200 (%d), yInt200SendCounter(%d)",
					 yCurrentTime.tv_sec, yNext200Time.tv_sec,
					 yInt200SendCounter);

			__DDNDEBUG__ (DEBUG_MODULE_CALL, yStrNext200Msg,
						  "Retransmitting 200 OK", zCall);
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						  "Retransmitting 200 OK");
			yNext200Time.tv_sec = yNext200Time.tv_sec + yInt200SendCounter;

			yLongNext200Time += (yInt200SendCounter * 1000);

			if (yInt200SendCounter < 4)
			{
				yInt200SendCounter *= 2;
			}

			eXosip_lock (geXcontext);
			yRc = cb_udp_snd_message_external (zCall, reAnswer);
			eXosip_unlock (geXcontext);
		}

		if ((yCurrentTime.tv_sec > yAnswerTime.tv_sec + 1) ||
			(gCall[zCall].callState == CALL_STATE_CALL_RELEASED))
		{
			writeCDR ("process_CALL_NEW", zCall, "RC", 20014, -1);	//DDN_TODO: SNMP SetRequest
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_SIP_SIGNALING,
					   ERR,
					   "Failed to get ACK for call. (Timed out or callState = CALL_STATE_CALL_RELEASED(%d)). "
					   "time [%ld, %ld] callState=%d", 
						CALL_STATE_CALL_RELEASED,
						yCurrentTime.tv_sec, yAnswerTime.tv_sec, gCall[zCall].callState);

			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, DYN_BASE, WARN,
					   "Sending SIP Message call_terminate (603).");

			setCallState (zCall, CALL_STATE_CALL_RELEASED);
			eXosip_lock (geXcontext);
			eXosip_call_terminate (geXcontext, gCall[zCall].cid, gCall[zCall].did);
			gCall[zCall].lastConnectedTime = -1;            // BT-72
			time (&(gCall[zCall].lastReleaseTime));
			writeCDR ("process_CALL_NEW", zCall, "RC", 20014, -1);
			eXosip_unlock (geXcontext);

			response.returnCode = -2;

			sprintf (response.message, "%s", "TEL_AnswerCall Timedout");

			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			gCall[zCall].msgToDM.opcode = DMOP_DROPCALL;
			yRc =
				notifyMediaMgr (__LINE__, zCall,
								(struct MsgToDM *) &(gCall[zCall].msgToDM),
								mod);

			if (reAnswer != NULL)
			{
				osip_message_free (reAnswer);
				reAnswer = NULL;
			}

			gCallsFinished++;	//DDN: 20090928 Since start count was incremented in EXOSIP_CALL_INVITE
			if ( gCallsFinished > gCallsStarted )
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, WARN,
						   "DEBUG: Incremented gCallsFinished to %d, gCallStarted=%d  ",
						gCallsFinished, gCallsStarted);
			}
	
			return (NULL);
		}

		if (gCall[zCall].isCallAnswered_noMedia == 1 && sendRequest == 0)
		{
			sendRequest++;
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Got sendRecvStatus=%d in SDP message; waiting for sendrecv.",
					   gCall[zCall].sendRecvStatus);

			switch (gCall[zCall].sendRecvStatus)
			{
			case SENDONLY:
				if (gCall[zCall].isInputThreadStarted == 0)
				{
					gCall[zCall].isInputThreadStarted = 1;
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Setting isInputThreadStarted to 1.");
					if (gCall[zCall].remoteRtpPort > 0)
					{
	struct MsgToDM  msgRtpDetails;
						memcpy (&msgRtpDetails, &(gCall[zCall].msgToDM),
								sizeof (struct MsgToDM));

						msgRtpDetails.opcode = DMOP_RTPDETAILS;
						sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
								 gCall[zCall].leg,
								 gCall[zCall].remoteRtpPort,
								 gCall[zCall].localSdpPort,
								 (gCall[zCall].
								  telephonePayloadPresent) ? gCall[zCall].
								 telephonePayloadType : -99, RECVONLY,
								 gCall[zCall].codecType,
								 gCall[zCall].remoteRtpIp);

						yRc =
							notifyMediaMgr (__LINE__, zCall,
											(struct MsgToDM *)
											&(msgRtpDetails), mod);

					}

				}

				break;

			case RECVONLY:
				if (gCall[zCall].isOutputThreadStarted == 0)
				{
					gCall[zCall].isOutputThreadStarted = 1;

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Setting isOutputThreadStarted to 1.");

					if (gCall[zCall].remoteRtpPort > 0)
					{
	struct MsgToDM  msgRtpDetails;
						memcpy (&msgRtpDetails, &(gCall[zCall].msgToDM),
								sizeof (struct MsgToDM));

						msgRtpDetails.opcode = DMOP_RTPDETAILS;
						sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
								 gCall[zCall].leg,
								 gCall[zCall].remoteRtpPort,
								 gCall[zCall].localSdpPort,
								 (gCall[zCall].
								  telephonePayloadPresent) ? gCall[zCall].
								 telephonePayloadType : -99, SENDONLY,
								 gCall[zCall].codecType,
								 gCall[zCall].remoteRtpIp);

						yRc =
							notifyMediaMgr (__LINE__, zCall,
											(struct MsgToDM *)
											&(msgRtpDetails), mod);

					}

				}
				break;
			case INACTIVE:
			default:
				break;
			}
		}

		usleep (yIntSleepMS * 1000);

		if (!canContinue (mod, zCall))
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, WARN,
					   "Can not continue; exiting answerCallThread().");
			response.returnCode = -2;

			sprintf (response.message, "%s", "TEL_AnswerCall Timedout");

			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			gCall[zCall].msgToDM.opcode = DMOP_DROPCALL;
			yRc =
				notifyMediaMgr (__LINE__, zCall,
								(struct MsgToDM *) &(gCall[zCall].msgToDM),
								mod);

			if (reAnswer != NULL)
			{
				osip_message_free (reAnswer);
				reAnswer = NULL;
			}

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}

	}//while

	if (reAnswer != NULL)
	{
		osip_message_free (reAnswer);
		reAnswer = NULL;
	}

	if (gCall[zCall].crossPort > 0)
	{
		setCallState (zCall, CALL_STATE_CALL_BRIDGED);
		//gCall[zCall].callState = CALL_STATE_CALL_BRIDGED;
	}

	while (gCall[zCall].remoteRtpPort <= 0)
	{
		usleep (100 * 1000);	//wait
	}

	//Send Rtp Details to Media Mgr
	if (gCall[zCall].remoteRtpPort > 0)
	{

		if (gCall[zCall].isInputThreadStarted == 0)
		{
	struct MsgToDM  msgRtpDetails;
			memcpy (&msgRtpDetails, &(gCall[zCall].msgToDM),
					sizeof (struct MsgToDM));

			msgRtpDetails.opcode = DMOP_RTPDETAILS;
			sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
					 gCall[zCall].leg,
					 gCall[zCall].remoteRtpPort,
					 gCall[zCall].localSdpPort,
					 (gCall[zCall].telephonePayloadPresent) ? gCall[zCall].
					 telephonePayloadType : -99, RECVONLY,
					 gCall[zCall].codecType, gCall[zCall].remoteRtpIp);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Calling notifyMediaMgr with data(%s), opcode=%d.",
					   msgRtpDetails.data, msgRtpDetails.opcode);

			yRc =
				notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msgRtpDetails),
								mod);
		}
		if (gCall[zCall].isOutputThreadStarted == 0)
		{
	struct MsgToDM  msgRtpDetails;
			memcpy (&msgRtpDetails, &(gCall[zCall].msgToDM),
					sizeof (struct MsgToDM));

			msgRtpDetails.opcode = DMOP_RTPDETAILS;
			sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
					 gCall[zCall].leg,
					 gCall[zCall].remoteRtpPort,
					 gCall[zCall].localSdpPort,
					 (gCall[zCall].telephonePayloadPresent) ? gCall[zCall].
					 telephonePayloadType : -99, SENDONLY,
					 gCall[zCall].codecType, gCall[zCall].remoteRtpIp);
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Calling notifyMediaMgr with data(%s), opcode=%d.",
					   msgRtpDetails.data, msgRtpDetails.opcode);

			yRc =
				notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msgRtpDetails),
								mod);
		}

	}

	createMrcpRtpDetails (zCall);

	//Send ANSWERCALL structure to media Mgr
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling notifyMediaMgr with opcode=%d.",
			   gCall[zCall].msgToDM.opcode);

	yRc =
		notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(gCall[zCall].msgToDM),
						mod);

	if (gCall[zCall].GV_CallDuration == 0)
	{
		/*No action if GV_CallDuration is explicitly set to 0 */
		;
	}
	else if (gCall[zCall].GV_CallDuration > 0)
	{
//	struct timeb    yTmpTimeb;
	struct timeval	yTmpTimeb;

//		ftime (&yTmpTimeb);
		gettimeofday(&yTmpTimeb, NULL);

		yTmpTimeb.tv_sec += gCall[zCall].GV_CallDuration;

		yRc = addToTimerList (__LINE__, zCall, MP_OPCODE_END_CALL,
							  gCall[zCall].msgToDM,
							  yTmpTimeb, gCall[zCall].msgToDM.appPid, -1, -1);

		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
				   "Registering MP_OPCODE_END_CALL with timerThread GV_CallDuration=%d",
				   gCall[zCall].GV_CallDuration);

	}
	else if (gMaxCallDuration > 0)
	{
		/*Default */
//	struct timeb    yTmpTimeb;
	struct timeval	yTmpTimeb;

//		ftime (&yTmpTimeb);
		gettimeofday(&yTmpTimeb, NULL);

		yTmpTimeb.tv_sec += gMaxCallDuration;

		yRc = addToTimerList (__LINE__, zCall, MP_OPCODE_END_CALL,
							  gCall[zCall].msgToDM,
							  yTmpTimeb, gCall[zCall].msgToDM.appPid, -1, -1);

		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
				   "Registering MP_OPCODE_END_CALL with timerThread maxCallDuration=%d.",
				   gMaxCallDuration);
	}

	pthread_detach (pthread_self ());
	pthread_exit (NULL);

	return (NULL);

}/*END: void * answerCallThread */

///This is the function we use to initiate calls and is fired whenever we see a DMOP_INITIATECALL.
/**
The following function is a thread fired by process_DMOP_INITIATECALL.  We use
this thread to send an invite to the destiniation we wish to establish a SIP
call with.
*/



/*
//#define INITIATE_CALL_BAIL_LINE()\
//	 do { fprintf(stderr," %s() bail at file=%s line=%d\n", __func__, __FILE__, __LINE__); } while (0)\
		\

*/

#define INITIATE_CALL_BAIL_LINE() 




void           *
initiateCallThread (void *z)
{
	char            mod[] = { "initiateCallThread" };
	struct MsgToDM *ptrMsgToDM;
	struct Msg_InitiateCall msg;
	struct MsgToApp response;
	int             yRc;
	int             zCall;
	int             waitCount = 0;
	int             ringCountIn100msUnits;
	char            message[MAX_LOG_MSG_LENGTH];
	time_t          yCurrentTime, yConnectTime;
	char            yStrDestination[500];
	char            yStrFromUrl[256];
	char            yStrLocalSdpPort[10];
	char            yStrTmpIPAddress[256];
	char           *pch = strchr (msg.ipAddress, '@');
	char            yStrRemoteIPAddress[255] = "";

    char            ipAddress[512];
    char            phoneNumber[512];

	char           *texto = NULL;
	size_t			length;

	osip_message_t *invite = NULL;
	osip_message_t *send_ack = NULL;

	time (&yCurrentTime);
	time (&yConnectTime);

	msg = *(struct Msg_InitiateCall *) z;

	zCall = msg.appCallNum;

	gCall[zCall].outboundRetCode = 0;
	gCall[zCall].cid = -1;
	gCall[zCall].did = -1;

	yConnectTime = yConnectTime + 2 + msg.numRings * 6;
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Waiting for call to be connected for at least %d seconds. [ numRings=%d; connectTime=%d ]",
			   yConnectTime - yCurrentTime, msg.numRings, yConnectTime);

	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	gCall[zCall].remoteRtpPort = 0;
	gCall[zCall].isInitiate = 1;

    /*
        If these global vars arent set use the globals 
        set in the platform
    */

	if (!gCall[zCall].GV_SipAuthenticationUser[0]) {
		sprintf (gCall[zCall].GV_SipAuthenticationUser, "%s", gAuthUser);
	}

	if (!gCall[zCall].GV_SipAuthenticationPassword[0]) {
		sprintf (gCall[zCall].GV_SipAuthenticationPassword, "%s",
				 gAuthPassword);
	}

	if (!gCall[zCall].GV_SipAuthenticationId[0]) {
		sprintf (gCall[zCall].GV_SipAuthenticationId, "%s", gAuthId);
	}

	if (!gCall[zCall].GV_SipAuthenticationRealm[0]) {
		sprintf (gCall[zCall].GV_SipAuthenticationRealm, "%s", gAuthRealm);
	}

	if (!gCall[zCall].GV_SipFrom[0]) {
		sprintf (gCall[zCall].GV_SipFrom, "%s", gFromUrl);
	}

	if (!gCall[zCall].GV_DefaultGateway[0]) {
		sprintf (gCall[zCall].GV_DefaultGateway, "%s", gDefaultGateway);
	}

    /*
	  End Global Vars
    */

	if (strstr (msg.ipAddress, "FILE:") != 0)
	{
		if ( (yRc = getParmFromFile(zCall, "ipAddress", msg.ipAddress, ipAddress)) != 0 )
		{
			response.returnCode = -1;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			// eXosip_unlock (geXcontext); // MR4921 - extranious eXosip_unlock()
	       	if(invite){
	          osip_message_free(invite);
	          invite = NULL;
	        }
	        INITIATE_CALL_BAIL_LINE();
	
			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}
	}
	else
	{
		sprintf(ipAddress, "%s", msg.ipAddress);
	} 

	if (strstr (msg.phoneNumber, "FILE:") != 0)
	{
		if ( (yRc = getParmFromFile(zCall, "phoneNumber", msg.phoneNumber, phoneNumber)) != 0 )
		{
			response.returnCode = -1;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			// eXosip_unlock (geXcontext);		// MR4921 - extranious eXosip_unlock()
	       	if(invite){
	          osip_message_free(invite);
	          invite = NULL;
	        }
	        INITIATE_CALL_BAIL_LINE();
	
			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}
	}
	else
	{
		sprintf(phoneNumber, "%s", msg.phoneNumber);
	}
	sprintf (yStrTmpIPAddress, "%s", ipAddress);
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"yStrTmpIPAddress:(%s) msg.informat:%d", yStrTmpIPAddress, msg.informat);

	pch = strstr (yStrTmpIPAddress, "@");

	if (strstr (yStrTmpIPAddress, "sip:") && msg.informat != E_164)
	{
		if (pch == NULL)
		{
			if (msg.informat == NONE || msg.informat == NAMERICAN)
			{
				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "%s@[%s];user=phone",
							 phoneNumber, gCall[zCall].GV_DefaultGateway);
//fprintf(stderr, " %s(%d) yStrDestination=%s\n", __func__, __LINE__, yStrDestination);
				}
				else
				{
					sprintf (yStrDestination, "%s@%s;user=phone",
							 phoneNumber, gCall[zCall].GV_DefaultGateway);
//fprintf(stderr, " %s(%d) yStrDestination=%s\n", __func__, __LINE__, yStrDestination);
				}
			}
			else
			{
				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "%s@[%s];user=phone",
							 phoneNumber, gCall[zCall].remoteRtpIp);
//fprintf(stderr, " %s(%d) yStrDestination=%s\n", __func__, __LINE__, yStrDestination);
				}
				else
				{				// check with rahul about the below
					sprintf (yStrRemoteIPAddress, "%s",
							 gCall[zCall].remoteRtpIp);
//fprintf(stderr, " %s(%d) yStrDestination=%s\n", __func__, __LINE__, yStrDestination);
				}
			}
		}
		else
		{

			if (gEnableIPv6Support != 0)
			{
	char           *tempChar = strstr (pch, "]:");
	char           *ptr = NULL;

				sprintf (yStrDestination, "%s", ipAddress);
//fprintf(stderr, " %s(%d) yStrDestination=%s\n", __func__, __LINE__, yStrDestination);
				if (tempChar == NULL)
				{
					sprintf (yStrRemoteIPAddress, "%s", pch + 2);
					if ((ptr = strchr (yStrRemoteIPAddress, ']')) != NULL)
					{
						*ptr = '\0';
					}

				}
				else
				{
					snprintf (yStrRemoteIPAddress,
							  strlen (pch) - strlen (tempChar), "%s",
							  pch + 2);
					if ((ptr = strchr (yStrRemoteIPAddress, ']')) != NULL)
					{
						*ptr = '\0';
					}
				}
			}
			else
			{
	char           *tempChar = strstr (pch, ":");

				sprintf (yStrDestination, "%s", ipAddress);
				if (tempChar == NULL)
				{
					sprintf (yStrRemoteIPAddress, "%s", pch + 1);
				}
				else
				{
					snprintf (yStrRemoteIPAddress,
							  strlen (pch) - strlen (tempChar), "%s",
							  pch + 1);
				}
			}
		}
	}
	else
	{
		if (pch == NULL)
		{

			if (gEnableIPv6Support != 0)
			{
				if (msg.informat == NONE || msg.informat == NAMERICAN)
				{
	char           *ptr = NULL;

					sprintf (yStrDestination, "sip:%s@[%s];user=phone",
							 phoneNumber, gCall[zCall].GV_DefaultGateway);
					sprintf (yStrRemoteIPAddress, "[%s]",
							 gCall[zCall].GV_DefaultGateway);
					if ((ptr = strchr (yStrRemoteIPAddress, ']')) != NULL)
					{
						*ptr = '\0';
					}

//fprintf(stderr, " %s(%d) yStrDestination=%s\n", __func__, __LINE__, yStrDestination);
				}
				else
				{
	char           *ptr = NULL;

					sprintf (yStrDestination, "sip:%s@[%s];user=phone",
							 phoneNumber, gCall[zCall].remoteRtpIp);
					sprintf (yStrRemoteIPAddress, "[%s]",
							 gCall[zCall].remoteRtpIp);
					if ((ptr = strchr (yStrRemoteIPAddress, ']')) != NULL)
					{
						*ptr = '\0';
					}
				}
			}
			else
			{
				if (msg.informat == NONE || msg.informat == NAMERICAN)
				{
					sprintf (yStrDestination, "sip:%s@%s;user=phone",
							 phoneNumber, gCall[zCall].GV_DefaultGateway);
					sprintf (yStrRemoteIPAddress, "%s",
							 gCall[zCall].GV_DefaultGateway);
				}
				else
				{
					sprintf (yStrDestination, "sip:%s@%s;user=phone",
							 phoneNumber, gCall[zCall].remoteRtpIp);

					sprintf (yStrRemoteIPAddress, "%s",
							 gCall[zCall].remoteRtpIp);
				}
			}
		}
		else
		{
			if (gEnableIPv6Support != 0)
			{
	char           *tempChar = strstr (pch, "]:");
	char           *ptr = NULL;

				sprintf (yStrDestination, "sip:[%s]", ipAddress);

//fprintf(stderr, " %s(%d) yStrDestination=%s\n", __func__, __LINE__, yStrDestination);
				if (tempChar == NULL)
				{
					sprintf (yStrRemoteIPAddress, "%s", pch + 2);
					if ((ptr = strchr (yStrRemoteIPAddress, ']')) != NULL)
					{
						*ptr = '\0';
					}
				}
				else
				{
					snprintf (yStrRemoteIPAddress,
							  strlen (pch) - strlen (tempChar), "%s",
							  pch + 2);
					if ((ptr = strchr (yStrRemoteIPAddress, ']')) != NULL)
					{
						*ptr = '\0';
					}
				}
			}
			else
			{
	char           *tempChar = strstr (pch, ":");

				sprintf (yStrDestination, "sip:%s", ipAddress);
				if (tempChar == NULL)
				{
					sprintf (yStrRemoteIPAddress, "%s", pch + 1);
				}
				else
				{
					snprintf (yStrRemoteIPAddress,
							  strlen (pch) - strlen (tempChar), "%s",
							  pch + 1);
				}
			}
		}
	}

//		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//			"DJB: GV_SipFrom is (%s)", gCall[zCall].GV_SipFrom);
//		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//			"DJB: GV_CallerName is (%s)", gCall[zCall].GV_CallerName);
	if (gCall[zCall].GV_SipFrom[0])
	{
		if (gCall[zCall].GV_CallerName[0])
		{
			sprintf (yStrFromUrl, "\"%s\" <sip:%s>",
					 gCall[zCall].GV_CallerName, gCall[zCall].GV_SipFrom);
		}
		else
		{
			sprintf (yStrFromUrl, "sip:%s", gCall[zCall].GV_SipFrom);
		}
	}
	else if (gCall[zCall].GV_SipAuthenticationUser[0])
	{
		if (gCall[zCall].GV_CallerName[0])
		{
			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrFromUrl, "\"%s\" <sip:%s@[%s]>",
						 gCall[zCall].GV_CallerName,
						 gCall[zCall].GV_SipAuthenticationUser, gHostIp);
			}
			else
			{
				sprintf (yStrFromUrl, "\"%s\" <sip:%s@%s>",
						 gCall[zCall].GV_CallerName,
						 gCall[zCall].GV_SipAuthenticationUser, gHostIp);
			}
		}
		else
		{
			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrFromUrl, "sip:%s@[%s]",
						 gCall[zCall].GV_SipAuthenticationUser, gHostIp);
//fprintf(stderr, " %s(%d) yStrFromUrl=%s\n", __func__, __LINE__, yStrFromUrl);
			}
			else
			{
				sprintf (yStrFromUrl, "sip:%s@%s",
						 gCall[zCall].GV_SipAuthenticationUser, gHostIp);
//fprintf(stderr, " %s(%d) yStrFromUrl=%s\n", __func__, __LINE__, yStrFromUrl);
			}
		}
	}
	else
	{
		if (gCall[zCall].GV_CallerName[0])
		{
			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrFromUrl, "\"%s\" <sip:arc@[%s]>",
						 gCall[zCall].GV_CallerName, gHostIp);
//fprintf(stderr, " %s(%d) yStrFromUrl=%s\n", __func__, __LINE__, yStrFromUrl);
			}
			else
			{
				sprintf (yStrFromUrl, "\"%s\" <sip:arc@%s>",
						 gCall[zCall].GV_CallerName, gHostIp);
//fprintf(stderr, " %s(%d) yStrFromUrl=%s\n", __func__, __LINE__, yStrFromUrl);
			}
		}
		else
		{
			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrFromUrl, "sip:arc@[%s]", gHostIp);
//fprintf(stderr, " %s(%d) yStrFromUrl=%s\n", __func__, __LINE__, yStrFromUrl);
			}
			else
			{
				sprintf (yStrFromUrl, "sip:arc@%s", gHostIp);
//fprintf(stderr, " %s(%d) yStrFromUrl=%s\n", __func__, __LINE__, yStrFromUrl);
			}
		}
	}
	if ((msg.informat == E_164) && gSipEnableEnum)
	{

	char           *enum_ip = NULL;
	char           *enum_uri = NULL;
	struct ares_naptr_list_t *list = NULL;

		enum_ip = enum_convert (phoneNumber, gSipEnumTLD);

		if (enum_ip)
		{
			enum_lookup (&list, enum_ip, 0);
			if (list)
			{
				enum_match (list, ENUM_SERVICE_SIP, phoneNumber,
							&enum_uri);
				if (enum_uri)
				{
					snprintf (yStrDestination, sizeof (yStrDestination), "%s",
							  enum_uri);
				}
			}
		}

		if (enum_ip)
			free (enum_ip);
		if (enum_uri)
			free (enum_uri);

        // ok free naptr list here 
        ares_naptr_list_free (&list);

	}

	// end enum lookups

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "InitiateCall to ", yStrDestination,
				  zCall);

	eXosip_lock (geXcontext);

	arc_add_authentication_info (gCall[zCall].GV_SipAuthenticationUser, gCall[zCall].GV_SipAuthenticationId, gCall[zCall].GV_SipAuthenticationPassword, "",	/*Algorith,  default MD5 */
								 gCall[zCall].GV_SipAuthenticationRealm);

	if (gOutboundProxyRequired)
	{
	char            yStrTmpProxy[256];

		if (gEnableIPv6Support != 0)
		{
			sprintf (yStrTmpProxy, "sip:[%s]", gOutboundProxy);
//fprintf(stderr, " %s(%d) yStrTmpProxy=%s\n", __func__, __LINE__, yStrTmpProxy);
		}
		else
		{
//fprintf(stderr, " %s(%d) yStrTmpProxy=%s\n", __func__, __LINE__, yStrTmpProxy);
			sprintf (yStrTmpProxy, "sip:%s", gOutboundProxy);
		}

//fprintf(stderr, " %s(%d) yStrTmpProxy=%s yStrFromUrl=%s yStrDestination=%s\n", __func__, __LINE__, yStrTmpProxy, yStrFromUrl, yStrDestination);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "yStrRemoteIPAddress=(%s) yStrTmpProxy:(%s) yStrFromUrl:(%s) yStrDestination=(%s)", yStrRemoteIPAddress,
					yStrTmpProxy, yStrFromUrl, yStrDestination);

		yRc = eXosip_call_build_initial_invite (geXcontext, &invite,
												yStrTmpProxy,
												yStrFromUrl,
												yStrDestination, "");
	}
	else
	{
#if 1

	int             rc = lookup_host (yStrRemoteIPAddress);

		if (rc == -1 && msg.informat != E_164)
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL,
						  "Failed to bridge the call, could not build initial invite",
						  "", zCall);
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
					   TEL_BRIDGECALL_FAILURE, ERR,
					   "Failed to bridge the call, could not build initial invite. "
					   "rc=%d; msg.informat=%d (E_164=%d)", rc, msg.informat,
					   E_164);

			response.returnCode = -54;
			gCall[zCall].outboundRetCode = 54;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			eXosip_unlock (geXcontext);

			gCall[zCall].crossPort = -1;
			gCall[zCall].leg = LEG_A;
	
 	        if(invite){
              osip_message_free(invite);
              invite = NULL;
            }

			INITIATE_CALL_BAIL_LINE();

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);

		}
#endif

//fprintf(stderr, " %s(%d) yStrDestination=%s yStrFromUrl=%s\n", __func__, __LINE__, yStrDestination, yStrFromUrl);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "yStrRemoteIPAddress=(%s) yStrFromUrl:(%s) yStrDestination=(%s) routeHeader:(%s)", yStrRemoteIPAddress,
					yStrFromUrl, yStrDestination, gCall[zCall].GV_RouteHeader);

		osip_message_to_str (invite, &texto, &length);

		yRc = eXosip_call_build_initial_invite (geXcontext, &invite,
												yStrDestination,
												yStrFromUrl,
												gCall[zCall].GV_RouteHeader,
												"");

	
#if 0
		gViaHost = (char *) malloc(64);
		sprintf(gViaHost, "%s", gHostIp);
		osip_via_t *via = NULL;
		char		*p = NULL;


		yRc = osip_message_get_via(invite, 0, &via);
		osip_via_to_str(via, &texto);
		p = via_get_host(via);

		via_set_host(via, gViaHost);

		yRc = osip_message_get_via(invite, 0, &via);
		osip_via_to_str(via, &texto);
		p = via_get_host(via);
#endif
		osip_contact_t *contact;

		osip_list_t    *list = &invite->contacts;
		osip_uri_t     *orig = NULL;
		char           *tmp2 = NULL;


		if (list != NULL)
		{
			contact = (osip_contact_t *) osip_list_get (list, 0);
			if (contact)
			{
				int	rc;

				char *tmp = NULL;
				orig = osip_contact_get_url (contact);
				if (orig)
				{
					char	tPort[64] = "";
					rc = osip_uri_to_str (orig, &tmp);
					arc_get_port_from_contact_header (tmp, tPort);
				}
	
			}
		}
	}

	if (yRc < 0)
	{
		response.returnCode = -1;
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);
		eXosip_unlock (geXcontext);

       	if(invite){
          osip_message_free(invite);
          invite = NULL;
        }
  
        INITIATE_CALL_BAIL_LINE();

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return (NULL);
	}

	eXosip_unlock (geXcontext);

	sprintf (yStrLocalSdpPort, "%d", gCall[zCall].localSdpPort);

	yRc = createInitiateSdpBody (zCall, yStrLocalSdpPort, -1, -2, 101);	//-2: No video in sdp

	/*MR_3069:  If Prack is required, include it in the outbound INVITE message. */
	if (gPrackMethod == PRACK_REQUIRE)
	{
		osip_message_set_header (invite, "Require", "100rel");
		gCall[zCall].UAC_PRACKOKReceived = 0;
		gCall[zCall].UAC_PRACKRingCounter = 0;
	}
	else if (gPrackMethod == PRACK_SUPPORTED)
	{
		osip_message_set_header (invite, "Supported", "100rel");
		gCall[zCall].UAC_PRACKOKReceived = 0;
		gCall[zCall].UAC_PRACKRingCounter = 0;
	}

	if (gCall[zCall].GV_PAssertedIdentity[0] != '\0')
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Setting P-Asserted-Identity = %s",
				   gCall[zCall].GV_PAssertedIdentity);
		osip_message_set_header (invite, "P-Asserted-Identity",
								 gCall[zCall].GV_PAssertedIdentity);
	}

	if (gCall[zCall].GV_Diversion[0] != '\0')
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Setting Diversion = %s", gCall[zCall].GV_Diversion);
		osip_message_set_header (invite, "Diversion",
								 gCall[zCall].GV_Diversion);
	}

	if (gCall[zCall].GV_CallInfo[0] != '\0')
	{
		osip_message_set_header (invite, "Call-Info",
								 gCall[zCall].GV_CallInfo);
	}

	osip_message_set_body (invite, gCall[zCall].sdp_body_local, strlen(gCall[zCall].sdp_body_local));
	osip_message_set_content_type (invite, "application/sdp");

	eXosip_lock (geXcontext);

	
	arc_fixup_contact_header (invite, gContactIp, gSipPort, zCall);

	if (phoneNumber[0])
	{
		sprintf (gCall[zCall].msgToDM.data, "%s", phoneNumber);
	}
	else
	{
	osip_uri_t     *sipMainUri = NULL;

		sipMainUri = osip_message_get_uri (invite);
		if (sipMainUri && sipMainUri->username && sipMainUri->username[0])
		{
			sprintf (gCall[zCall].msgToDM.data, "%s", sipMainUri->username);
		}
		else
		{
			sprintf (gCall[zCall].msgToDM.data, "%s", yStrDestination);
		}
	}

	// DDN:20111212
	arc_insert_global_headers (zCall, invite);
	yRc = eXosip_call_send_initial_invite (geXcontext, invite);
    // the eXosip call send function eats the message_t structure 
	invite = NULL;
	eXosip_unlock (geXcontext);


	if (yRc < 0)
	{
		response.returnCode = -1;
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);

	    INITIATE_CALL_BAIL_LINE();

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return (NULL);
	}

	gCall[zCall].lastError = CALL_NO_ERROR;
	gCall[zCall].cid = yRc;
	sprintf(gCall[zCall].sipFromStr, "%s", yStrFromUrl);  // MR 4964

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
	   "Set gCall[%d].sipFromStr to (%s)", zCall,  gCall[zCall].sipFromStr);

	response.returnCode = 0;

//	while (gCall[zCall].callState != CALL_STATE_CALL_ANSWERED
//		   || gCall[zCall].remoteRtpPort <= 0)
//	                                                               // BT-79 
    while ( (gCall[zCall].callState != CALL_STATE_CALL_ANSWERED && gCall[zCall].callState != CALL_STATE_CALL_ACK)
           || gCall[zCall].remoteRtpPort <= 0)

	{
		usleep (50 * 1000);

		//
		// this signals that the call has been redirected back to the app 
		// here we create a message to the app and send it back 

		if (gCall[zCall].callState == CALL_STATE_CALL_REDIRECTED)
		{

			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
					   "Outbound Call has been redirected. ");

			response.returnCode = -1 * mapSipErrorToArcError (zCall, 300);
			gCall[zCall].outboundRetCode = response.returnCode;
			sprintf (response.message, "%d:%s", 22, "300 Multiple Choice");

			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			gCall[zCall].cid = -1;
			setCallState (zCall, CALL_STATE_CALL_RELEASED);

            INITIATE_CALL_BAIL_LINE();

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}
		else if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED
				 || gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
				 || gCall[zCall].callState == CALL_STATE_CALL_TERMINATE_CALLED
				 || gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
		{
			response.returnCode = -3;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			setCallState (zCall, CALL_STATE_CALL_RELEASED);

			gCall[zCall].cid = -1;


            INITIATE_CALL_BAIL_LINE();

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}

		//
		// the following will time out the call when appropriate
		//

		time (&yCurrentTime);


		if (yCurrentTime >= yConnectTime
			|| gCall[zCall].callState == CALL_STATE_CALL_REQUESTFAILURE)
		{
			response.returnCode = -50;
			sprintf (response.message, "%d:%s", 50, "Ring no answer");

			if (gCall[zCall].callState == CALL_STATE_CALL_REQUESTFAILURE)
			{
				gCall[zCall].outboundRetCode = gCall[zCall].eXosipStatusCode;

				response.returnCode =
					-1 * mapSipErrorToArcError (zCall,
												gCall[zCall].
												eXosipStatusCode);
				sprintf (response.message, "%d:%s",
						 mapSipErrorToArcError (zCall,
												gCall[zCall].
												eXosipStatusCode),
						 gCall[zCall].eXosipReasonPhrase);
			}
			else if (yCurrentTime >= yConnectTime)
			{
				gCall[zCall].outboundRetCode = 408;	//Request Timeout

				if (gCall[zCall].callState == CALL_STATE_CALL_RINGING)
				{
					response.returnCode = -50;
					sprintf (response.message, "%d:%s", 50, "Ring no answer");
				}
				else
				{
					response.returnCode = -52;
					sprintf (response.message, "%d:%s", 52, "No ring back");
				}
			}

			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
					   "Sending SIP Message call_terminate (603).");

			eXosip_lock (geXcontext);
			eXosip_call_terminate (geXcontext, gCall[zCall].cid, gCall[zCall].did);
			time (&(gCall[zCall].lastReleaseTime));
			eXosip_unlock (geXcontext);

			writeGenericResponseToApp (__LINE__, zCall, &response, mod);

			gCall[zCall].cid = -1;

			setCallState (zCall, CALL_STATE_CALL_RELEASED);

            INITIATE_CALL_BAIL_LINE();

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}
	}

	while (gCall[zCall].remoteRtpPort <= 0)
	{
		usleep (50 * 1000);		//wait
	}

	if (gCall[zCall].remoteRtpPort > 0)
	{
	struct MsgToDM  msgRtpDetails;

		memcpy (&msgRtpDetails, &(gCall[zCall].msgToDM),
				sizeof (struct MsgToDM));

		msgRtpDetails.opcode = DMOP_RTPDETAILS;

		sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
				 gCall[zCall].leg,
				 gCall[zCall].remoteRtpPort,
				 gCall[zCall].localSdpPort,
				 (gCall[zCall].telephonePayloadPresent) ? gCall[zCall].
				 telephonePayloadType : -99, gCall[zCall].full_duplex,
				 gCall[zCall].codecType, gCall[zCall].remoteRtpIp);

#ifdef ACU_LINUX
		// have to specify inbound/outbound
		// if outbound and callprogress, send it to the aculab thread

		// this is to be done where DMOP_RTPDETAILS is sent
		if (gCall[zCall].GV_CallProgress == 4)
		{
	FILE           *yFpCallProgressInfo = NULL;
	char            yStrFileCallProgressInfo[100];

			sprintf (yStrFileCallProgressInfo, "%s/callProgressInfo.%d",
					 gCacheDirectory, zCall);

			yFpCallProgressInfo = fopen (yStrFileCallProgressInfo, "w+");

			if (yFpCallProgressInfo == NULL)
			{
				dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_NORMAL,
						   TEL_FILE_IO_ERROR, ERR,
						   "fopen(%s, w+) failed. Unable to write call Progress info. [%d, %s]",
						   yStrFileCallProgressInfo, errno, strerror (errno));
			}
			else
			{

				fprintf (yFpCallProgressInfo, "DIRECTION=OUTBOUND\n");

				/*
				 * DDN_TODO: Add other call Progress info to send to media mgr here
				 */
				fclose (yFpCallProgressInfo);
			}
		}
#endif

		yRc =
			notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msgRtpDetails), mod);
	}

	if (gCall[zCall].GV_CallDuration == 0)
	{
		/*No action if GV_CallDuration is explicitly set to 0 */
		;
	}
	else if (gCall[zCall].GV_CallDuration > 0)
	{
//	struct timeb    yTmpTimeb;
	struct timeval	yTmpTimeb;

//		ftime (&yTmpTimeb);
		gettimeofday(&yTmpTimeb, NULL);

		yTmpTimeb.tv_sec += gCall[zCall].GV_CallDuration;

		yRc = addToTimerList (__LINE__, zCall, MP_OPCODE_END_CALL,
							  gCall[zCall].msgToDM,
							  yTmpTimeb, gCall[zCall].msgToDM.appPid, -1, -1);

		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
				   "Registering MP_OPCODE_END_CALL with timerThread GV_CallDuration=%d.",
				   gCall[zCall].GV_CallDuration);

	}
	else if (gMaxCallDuration > 0)
	{
		/*Default */
	//struct timeb    yTmpTimeb;
	struct timeval	yTmpTimeb;

		//ftime (&yTmpTimeb);
		gettimeofday(&yTmpTimeb, NULL);

		yTmpTimeb.tv_sec += gMaxCallDuration;

		yRc = addToTimerList (__LINE__, zCall, MP_OPCODE_END_CALL,
							  gCall[zCall].msgToDM,
							  yTmpTimeb, gCall[zCall].msgToDM.appPid, -1, -1);

		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
				   "Registering MP_OPCODE_END_CALL with timerThread maxCallDuration=%d.",
				   gMaxCallDuration);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Calling notifyMediaMgr with opcode=%d; yStrDestination=%s.",
			   gCall[zCall].msgToDM.opcode, yStrDestination);

	gCall[zCall].appPid = msg.appPid;

#ifdef ACU_LINUX
	if (gCall[zCall].GV_CallProgress != 4)
	{
		yRc =
			notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(gCall[zCall].msgToDM),
							mod);
	}
#else
	//if(gCall[zCall].GV_CallProgress != 4)
	{
		yRc =
			notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(gCall[zCall].msgToDM),
							mod);
	}
#endif

#if 1 // DMOP_ CALL CONNECTED 
    // writing DMOP_CALL_CONNECTED to application API, 
    // Joe S.  Wed Jul 15 05:39:12 EDT 2015

	char ani_buff[128];
    char *ptr;

	snprintf(ani_buff, sizeof(ani_buff), "%s", gCall[zCall].GV_SipFrom);

	ptr = strchr(ani_buff, '@');
    if(ptr){
	  *ptr = '\0';
    }

    response.opcode = DMOP_CALL_CONNECTED;
    snprintf(response.message, sizeof(response.message), "ANI=%s", ani_buff);
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Writing DMOP_CALL_CONNECTED response from InitiateCall. ANI=[%s]", ani_buff);
	writeGenericResponseToApp (__LINE__, zCall, &response, mod);

#endif    // end DMOP_CALL_CONNECTED 




//RGDEBUG Should write InitiateCall Response from MediaMgr
#if 0

	gCall[zCall].appPid = msg.appPid;

	sprintf (response.message, "CALL=%d^AUDIO_CODEC=%s^VIDEO_CODEC=%s",
			 zCall, gCall[zCall].audioCodecString,
			 gCall[zCall].videoCodecString);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Writing response from InitiateCall response.message=%s.",
			   response.message);
#ifdef ACU_LINUX
	// don't call this if call progress is set to 4

	// chech gCall[zCall].GV_CallProgress to determine call progress
	if (gCall[zCall].GV_CallProgress != 4)
	{
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);
	}
#else
	if (gCall[zCall].GV_CallProgress != 4)
	{
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);
	}
#endif

#endif

    INITIATE_CALL_BAIL_LINE();

	pthread_detach (pthread_self ());
	pthread_exit (NULL);

	return (NULL);

}								/*END: void * initiateCallThread */

///This is the function we use to bridge calls and is fired whenever we see a DMOP_BRIDGECALL.
void           *
listenCallThread (void *z)
{
	char            mod[] = { "listenCallThread" };
	struct MsgToDM *ptrMsgToDM;
	struct Msg_ListenCall msg;
	struct MsgToApp response;
	int             yRc;
	int             zCall;
	int             zOutboundCall;
	int             waitCount = 0;
	int             ringCountIn100msUnits;
	int             rtpDetailsSent = 0;
	char            message[MAX_LOG_MSG_LENGTH];
	time_t          yCurrentTime, yConnectTime;
	char            yStrDestination[500];
	char            yStrFromUrl[256];
	char            yStrLocalSdpPort[10];
	char            yStrPayloadNumber[10];
	char            yStrPayloadMime[255];

	osip_message_t *invite = NULL;
	osip_message_t *send_ack = NULL;
	osip_message_t *answer = NULL;
	osip_message_t *ringing = NULL;

	char			ipAddress[512];
	char			phoneNumber[512];

	time (&yCurrentTime);
	time (&yConnectTime);

	msg = *(struct Msg_ListenCall *) z;

	zCall = msg.appCallNum;
	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	zOutboundCall = getFreeCallNum (OUTBOUND);

	gCall[zCall].outboundRetCode = 0;

	if (zOutboundCall == -1)
	{
		response.returnCode = -1;
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return (NULL);
	}

	gCall[zOutboundCall].bridgeDestination[0] = 0;

	if (gCall[zCall].callState != CALL_STATE_CALL_ACK)
	{
		if (gCall[zCall].PRACKSendRequire)
		{
			ringing = NULL;
			yRc = buildPRACKAnswer (zCall, &ringing);
			if (yRc == 0)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "Sending SIP Message 180 Ringing with PRACK.");
				eXosip_lock (geXcontext);
				yRc =
					eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 180, ringing);
				eXosip_unlock (geXcontext);
			}
		}
		else
		{
			eXosip_lock (geXcontext);
			yRc = eXosip_call_build_answer (geXcontext, gCall[zCall].tid, 180, &answer);
			arc_fixup_contact_header (answer, gHostIp, gSipPort, zCall);
			yRc = eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 180, answer);
			eXosip_unlock (geXcontext);
		}
	}

	memcpy (&(gCall[zOutboundCall].msgToDM), &(gCall[zCall].msgToDM),
			sizeof (struct MsgToApp));

	if (gCall[zCall].GV_SipAuthenticationUser[0])
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationUser, "%s",
				 gCall[zCall].GV_SipAuthenticationUser);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationUser, "%s",
				 gAuthUser);
	}

	if (gCall[zCall].GV_SipAuthenticationPassword[0])
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationPassword, "%s",
				 gCall[zCall].GV_SipAuthenticationPassword);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationPassword, "%s",
				 gAuthPassword);
	}

	if (gCall[zCall].GV_SipAuthenticationId[0])
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationId, "%s",
				 gCall[zCall].GV_SipAuthenticationId);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationId, "%s", gAuthId);
	}

	if (gCall[zCall].GV_SipAuthenticationRealm[0])
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationRealm, "%s",
				 gCall[zCall].GV_SipAuthenticationRealm);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationRealm, "%s",
				 gAuthRealm);
	}

	if (gCall[zCall].GV_SipFrom[0])
	{
		sprintf (gCall[zOutboundCall].GV_SipFrom, "%s",
				 gCall[zCall].GV_SipFrom);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipFrom, "%s", gFromUrl);
	}

	if (gCall[zCall].GV_DefaultGateway[0])
	{
		sprintf (gCall[zOutboundCall].GV_DefaultGateway, "%s",
				 gCall[zCall].GV_DefaultGateway);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_DefaultGateway, "%s",
				 gDefaultGateway);
	}

	gCall[zCall].crossPort = zOutboundCall;
	gCall[zCall].leg = LEG_A;

	gCall[zOutboundCall].crossPort = zCall;
	gCall[zOutboundCall].leg = LEG_B;

	yConnectTime = yConnectTime + 2 + msg.numRings * 6;

	gCall[zOutboundCall].remoteRtpPort = -1;

	if (strstr (msg.ipAddress, "FILE:") != 0)
	{
		if ( (yRc = getParmFromFile(zCall, "ipAddress", msg.ipAddress, ipAddress)) != 0 )
		{
			response.returnCode = -1;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}
	}
	else
	{
		sprintf(ipAddress, "%s", msg.ipAddress);
	}

	if (strstr (msg.phoneNumber, "FILE:") != 0)
	{
		if ( (yRc = getParmFromFile(zCall, "phoneNumber", msg.phoneNumber, phoneNumber)) != 0 )
		{
			response.returnCode = -1;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}
	}
	else
	{
		sprintf(phoneNumber, "%s", msg.phoneNumber);
	}

	char            yStrTmpIPAddress[256];
	char           *pch = strchr (ipAddress, '@');

	sprintf (yStrTmpIPAddress, "%s", ipAddress);

	pch = strstr (yStrTmpIPAddress, "@");

	if (strstr (yStrTmpIPAddress, "sip:"))
	{
		if (pch == NULL)
		{
			if (msg.informat == NONE)
			{
				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "%s@[%s];user=phone",
							 phoneNumber,
							 gCall[zOutboundCall].GV_DefaultGateway);
				}
				else
				{
					sprintf (yStrDestination, "%s@%s;user=phone",
							 phoneNumber,
							 gCall[zOutboundCall].GV_DefaultGateway);
				}
			}
			else
			{
				if (gCall[zCall].remoteRtpIp && gCall[zCall].remoteRtpIp[0])
				{
	int             yIntRemoteRtpIpLen = strlen (gCall[zCall].remoteRtpIp);

					if (gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] ==
						'\r'
						|| gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] ==
						'\n')
					{
						gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] = 0;
					}
				}

				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "%s@[%s];user=phone",
							 phoneNumber, gCall[zCall].remoteRtpIp);
				}
				else
				{
					sprintf (yStrDestination, "%s@%s;user=phone",
							 phoneNumber, gCall[zCall].remoteRtpIp);
				}
			}
		}
		else
		{
			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrDestination, "[%s]", ipAddress);
			}
			else
			{
				sprintf (yStrDestination, "%s", ipAddress);
			}
		}
	}
	else
	{
		if (pch == NULL)
		{
			if (msg.informat == NONE)
			{
				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "sip:%s@[%s];user=phone",
							 phoneNumber,
							 gCall[zOutboundCall].GV_DefaultGateway);
				}
				else
				{
					sprintf (yStrDestination, "sip:%s@%s;user=phone",
							 phoneNumber,
							 gCall[zOutboundCall].GV_DefaultGateway);
				}
			}
			else
			{
				if (gCall[zCall].remoteRtpIp && gCall[zCall].remoteRtpIp[0])
				{
	int             yIntRemoteRtpIpLen = strlen (gCall[zCall].remoteRtpIp);

					if (gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] ==
						'\r'
						|| gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] ==
						'\n')
					{
						gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] = 0;
					}
				}

				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "sip:%s@[%s];user=phone",
							 phoneNumber, gCall[zCall].remoteRtpIp);
				}
				else
				{
					sprintf (yStrDestination, "sip:%s@%s;user=phone",
							 phoneNumber, gCall[zCall].remoteRtpIp);
				}
			}
		}
		else
		{
			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrDestination, "sip:[%s]", ipAddress);
			}
			else
			{
				sprintf (yStrDestination, "sip:%s", ipAddress);
			}
		}
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "yStrDestination=(%s)", yStrDestination);

	if (gCall[zOutboundCall].GV_SipFrom[0])
	{
		if (gEnableIPv6Support != 0)
		{
			sprintf (yStrFromUrl, "sip:[%s]",
					 gCall[zOutboundCall].GV_SipFrom);
		}
		else
		{
			sprintf (yStrFromUrl, "sip:%s", gCall[zOutboundCall].GV_SipFrom);
		}
	}
	else if (gCall[zOutboundCall].GV_SipAuthenticationUser[0])
	{
		if (gEnableIPv6Support != 0)
		{
			sprintf (yStrFromUrl, "sip:%s@[%s]",
					 gCall[zOutboundCall].GV_SipAuthenticationUser, gHostIp);
		}
		else
		{
			sprintf (yStrFromUrl, "sip:%s@%s",
					 gCall[zOutboundCall].GV_SipAuthenticationUser, gHostIp);
		}
	}
	else
	{
		if (gEnableIPv6Support != 0)
		{
			sprintf (yStrFromUrl, "sip:arc@[%s]", gHostIp);
		}
		else
		{
			sprintf (yStrFromUrl, "sip:arc@%s", gHostIp);
		}
	}

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Bridge Call B_LEG ", yStrDestination,
				  zOutboundCall);
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "yStrFromUrl=%s", yStrFromUrl);

	eXosip_lock (geXcontext);
	arc_add_authentication_info (gCall[zOutboundCall].GV_SipAuthenticationUser, gCall[zOutboundCall].GV_SipAuthenticationId, gCall[zOutboundCall].GV_SipAuthenticationPassword, "",	/*Algorith,  default MD5 */
								 gCall[zOutboundCall].
								 GV_SipAuthenticationRealm);

	if (strstr (yStrDestination, "sip:") == NULL)
	{
		if (gEnableIPv6Support != 0)
		{
			sprintf (gCall[zOutboundCall].bridgeDestination, "[%s]",
					 yStrDestination);
		}
		else
		{
			sprintf (gCall[zOutboundCall].bridgeDestination, "%s",
					 yStrDestination);
		}
	}
	else
	{
		if (gEnableIPv6Support != 0)
		{
			sprintf (gCall[zOutboundCall].bridgeDestination + 4, "[%s]",
					 yStrDestination);
		}
		else
		{
			sprintf (gCall[zOutboundCall].bridgeDestination + 4, "%s",
					 yStrDestination);
		}
	}

	if (gOutboundProxyRequired)
	{
	char            yStrTmpProxy[256];

		if (gEnableIPv6Support != 0)
		{
			sprintf (yStrTmpProxy, "sip:[%s]", gOutboundProxy);
		}
		else
		{
			sprintf (yStrTmpProxy, "sip:%s", gOutboundProxy);
		}

		yRc = eXosip_call_build_initial_invite (geXcontext, &invite,
												yStrTmpProxy,
												yStrFromUrl,
												yStrDestination, "");
	}
	else
	{
		yRc = eXosip_call_build_initial_invite (geXcontext, &invite,
												yStrDestination,
												yStrFromUrl,
												gCall[zCall].GV_RouteHeader,
												"");
	}

	if (yRc < 0)
	{

		__DDNDEBUG__ (DEBUG_MODULE_CALL,
					  "Failed to bridge the call, could not build initial invite",
					  "", zCall);

		response.returnCode = -1;
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);
		eXosip_unlock (geXcontext);

		gCall[zCall].crossPort = -1;
		gCall[zCall].leg = LEG_A;

		gCall[zOutboundCall].crossPort = -1;
		gCall[zOutboundCall].leg = LEG_A;

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return (NULL);
	}

	gCall[zCall].currentInvite = invite;

	sprintf (yStrLocalSdpPort, "%d",
			 LOCAL_STARTING_RTP_PORT + (zOutboundCall * 2));

	createListenSdpBody (zOutboundCall, yStrLocalSdpPort,
						 gCall[zCall].codecType, -2,
						 gCall[zCall].telephonePayloadType);

	osip_message_set_body (invite, gCall[zOutboundCall].sdp_body_local,
						   strlen (gCall[zOutboundCall].sdp_body_local));
	osip_message_set_content_type (invite, "application/sdp");

	arc_fixup_contact_header (invite, gHostIp, gSipPort, zCall);

	arc_insert_global_headers (zCall, invite);

	yRc = eXosip_call_send_initial_invite (geXcontext, invite);
	eXosip_unlock (geXcontext);

	if (yRc < 0)
	{

		__DDNDEBUG__ (DEBUG_MODULE_CALL,
					  "Failed to bridge the call, could not send initial invite",
					  "", zCall);

		response.returnCode = -1;
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);

		gCall[zCall].crossPort = -1;
		gCall[zCall].leg = LEG_A;

		gCall[zOutboundCall].crossPort = -1;
		gCall[zOutboundCall].leg = LEG_A;

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return (NULL);
	}

	gCall[zOutboundCall].lastError = CALL_NO_ERROR;
	gCall[zOutboundCall].cid = yRc;

	response.returnCode = 0;

	while (gCall[zOutboundCall].callState != CALL_STATE_CALL_ANSWERED ||
		   gCall[zOutboundCall].remoteRtpPort <= 0)
	{
		usleep (10 * 1000);

		// save this room for 
		// call redirection or 300 multiple choice 

		// below is for call timeout 

		time (&yCurrentTime);

		if (yCurrentTime >= yConnectTime ||
			gCall[zOutboundCall].callState == CALL_STATE_CALL_REQUESTFAILURE)
		{

			response.returnCode = -50;
			sprintf (response.message, "%d:%s", 50, "Ring no answer");

			if (gCall[zOutboundCall].callState ==
				CALL_STATE_CALL_REQUESTFAILURE)
			{

				gCall[zOutboundCall].outboundRetCode =
					gCall[zOutboundCall].eXosipStatusCode;
				gCall[zCall].outboundRetCode =
					gCall[zOutboundCall].eXosipStatusCode;

				response.returnCode =
					-1 * mapSipErrorToArcError (zCall,
												gCall[zOutboundCall].
												eXosipStatusCode);

				sprintf (response.message, "%d:%s",
						 mapSipErrorToArcError (zCall,
												gCall[zOutboundCall].
												eXosipStatusCode),
						 gCall[zOutboundCall].eXosipReasonPhrase);
			}

			writeGenericResponseToApp (__LINE__, zCall, &response, mod);

			gCall[zCall].crossPort = -1;
			gCall[zCall].leg = LEG_A;

			gCall[zOutboundCall].crossPort = -1;
			gCall[zOutboundCall].leg = LEG_A;

			eXosip_lock (geXcontext);
			eXosip_call_terminate (geXcontext, gCall[zOutboundCall].cid,
								   gCall[zOutboundCall].did);
			time (&(gCall[zOutboundCall].lastReleaseTime));
			eXosip_unlock (geXcontext);

			gCall[zOutboundCall].cid = -1;

			setCallState (zOutboundCall, CALL_STATE_CALL_CLOSED);
			//gCall[zOutboundCall].callState = CALL_STATE_CALL_CLOSED;

			__DDNDEBUG__ (DEBUG_MODULE_CALL,
						  "Failed to bridge the call, B_LEG did not answer",
						  "", zCall);

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}
		else if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED
				 || gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
				 || gCall[zCall].callState == CALL_STATE_CALL_TERMINATE_CALLED
				 || gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
		{
			setCallSubState (zOutboundCall, CALL_STATE_CALL_CLOSED);

			gCall[zCall].crossPort = -1;
			gCall[zCall].leg = LEG_A;

			gCall[zOutboundCall].crossPort = -1;
			gCall[zOutboundCall].leg = LEG_A;

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Failed to bridge the call",
						  "A Leg already disconnected.", zOutboundCall);

#if 1
			eXosip_lock (geXcontext);
			eXosip_call_terminate (geXcontext, gCall[zOutboundCall].cid,
								   gCall[zOutboundCall].did);
			eXosip_unlock (geXcontext);
#endif

			gCall[zOutboundCall].cid = -1;

			setCallState (zOutboundCall, CALL_STATE_CALL_CLOSED);
			//gCall[zOutboundCall].callState = CALL_STATE_CALL_CLOSED;

			pthread_detach (pthread_self ());
			pthread_exit (NULL);

			return (NULL);
		}
		else if (gCall[zOutboundCall].callState == CALL_STATE_CALL_RINGING)
		{
			if (gCall[zOutboundCall].remoteRtpPort > 0 && !rtpDetailsSent)
			{
	struct MsgToDM  msgRtpDetails;

				/*Send Pre Initiate Message */
	struct Msg_InitiateCall yTmpMsgInitiate;
				memcpy (&yTmpMsgInitiate, &(msg),
						sizeof (struct Msg_InitiateCall));
				yTmpMsgInitiate.appCallNum2 = zOutboundCall;
				yTmpMsgInitiate.appCallNum1 = zCall;
				yTmpMsgInitiate.appCallNum = zCall;
				yTmpMsgInitiate.opcode = DMOP_MEDIACONNECT;
				yTmpMsgInitiate.appPid = msg.appPid;
				//yRc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(yTmpMsgInitiate), mod);
				/*END: Send Pre Initiate Message */
				memcpy (&msgRtpDetails, &(gCall[zOutboundCall].msgToDM),
						sizeof (struct MsgToDM));
				gCall[zOutboundCall].codecType = gCall[zCall].codecType;
				gCall[zOutboundCall].full_duplex = SENDONLY;
				gCall[zOutboundCall].telephonePayloadType =
					gCall[zCall].telephonePayloadType;
				msgRtpDetails.opcode = DMOP_RTPDETAILS;
				sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
						 LEG_B,
						 gCall[zOutboundCall].remoteRtpPort,
						 gCall[zOutboundCall].localSdpPort,
						 (gCall[zOutboundCall].
						  telephonePayloadPresent) ? gCall[zOutboundCall].
						 telephonePayloadType : -99,
						 gCall[zOutboundCall].full_duplex,
						 gCall[zOutboundCall].codecType,
						 gCall[zOutboundCall].remoteRtpIp);
				yRc =
					notifyMediaMgr (__LINE__, zOutboundCall,
									(struct MsgToDM *) &(msgRtpDetails), mod);
				rtpDetailsSent = 1;
				setCallSubState (zOutboundCall,
								 CALL_STATE_CALL_MEDIACONNECTED);
				setCallSubState (zCall, CALL_STATE_CALL_MEDIACONNECTED);
			}
		}

		if (gCall[zCall].callState != CALL_STATE_CALL_ACK)
		{
			eXosip_lock (geXcontext);
			yRc = eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 180, NULL);
			eXosip_unlock (geXcontext);
		}
	}							/*END: while */

	eXosip_lock (geXcontext);
	eXosip_call_build_ack (geXcontext, gCall[zOutboundCall].did, &send_ack);
	arc_fixup_contact_header (send_ack, gHostIp, gSipPort, zCall);
	arc_fixup_sip_request_line (send_ack, "", 0, zCall);
	eXosip_call_send_ack (geXcontext, gCall[zOutboundCall].did, send_ack);
	eXosip_unlock (geXcontext);

	gCall[zOutboundCall].appPid = msg.appPid;

	gCall[zCall].appPid = msg.appPid;

	msg.appCallNum2 = zOutboundCall;
	msg.appCallNum1 = zCall;
	msg.appCallNum = zCall;

	sprintf (response.message, "%d", zOutboundCall);

	yRc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msg), mod);

	if (gCall[zOutboundCall].remoteRtpPort > 0 && !rtpDetailsSent)
	{

	struct MsgToDM  msgRtpDetails;

		memcpy (&msgRtpDetails, &(gCall[zOutboundCall].msgToDM),
				sizeof (struct MsgToDM));

		if (gCall[zOutboundCall].leg == LEG_B
			&& gCall[zOutboundCall].crossPort >= 0)
		{
	int             yALegPort = gCall[zOutboundCall].crossPort;

			gCall[zOutboundCall].codecType = gCall[yALegPort].codecType;

			gCall[zOutboundCall].full_duplex = gCall[yALegPort].full_duplex;

			gCall[zOutboundCall].telephonePayloadType =
				gCall[yALegPort].telephonePayloadType;
		}

		msgRtpDetails.opcode = DMOP_RTPDETAILS;

		sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
				 LEG_B,
				 gCall[zOutboundCall].remoteRtpPort,
				 gCall[zOutboundCall].localSdpPort,
				 (gCall[zOutboundCall].
				  telephonePayloadPresent) ? gCall[zOutboundCall].
				 telephonePayloadType : -99, gCall[zOutboundCall].full_duplex,
				 gCall[zOutboundCall].codecType,
				 gCall[zOutboundCall].remoteRtpIp);

		yRc =
			notifyMediaMgr (__LINE__, zOutboundCall,
							(struct MsgToDM *) &(msgRtpDetails), mod);
		rtpDetailsSent = 1;

		setCallSubState (zOutboundCall, CALL_STATE_CALL_MEDIACONNECTED);
		setCallSubState (zCall, CALL_STATE_CALL_MEDIACONNECTED);

	}

	pthread_detach (pthread_self ());
	pthread_exit (NULL);
	return (NULL);

}								/*END: void * listenCallThread */

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
int
getBlastDialDestList (int zCall, char *zFile, int *zNumDests)
{
	static char     mod[] = "getBlastDialDestList";
	FILE           *fp;
	char            fileName[128];
	char            line[256];
	char            lhs[64];
	char            rhs[64];
	int             i;

	*zNumDests = 0;
	if (access (zFile, F_OK) < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
				   ERR,
				   "Unable to access blastDial datafile (%s); "
				   "Cannot process blastDial request.  [%d, %s]", zFile,
				   errno, strerror (errno));
		return (-1);
	}

	if ((fp = fopen (zFile, "r")) == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
				   ERR,
				   "Unable to open blastDial datafile (%s); "
				   "Cannot process blastDial request.  [%d, %s]", zFile,
				   errno, strerror (errno));
		unlink (zFile);
		return (-1);
	}

	memset (line, 0, sizeof (line));
	i = 0;
	while (fgets (line, sizeof (line) - 1, fp))
	{
		if (line[(int) strlen (line) - 1] == '\n')
		{
			line[(int) strlen (line) - 1] = '\0';
		}

		memset (rhs, 0, sizeof (rhs));
		sscanf (line, "%[^,],%s", gCall[zCall].dlist[i].destination, rhs);
		gCall[zCall].dlist[i].inputType = atoi (rhs);
		gCall[zCall].dlist[i].outboundChannel = -1;
		gCall[zCall].dlist[i].resultCode = -1;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "dlist[%d].destination=(%s) dlist[%d].inputType=(%d, IP=%d)",
				   i, gCall[zCall].dlist[i].destination,
				   i, gCall[zCall].dlist[i].inputType, IP);
		i++;
	}

	*zNumDests = i;
	fclose (fp);
	unlink (zFile);
	return (0);
}								// END: getBlastDialDestList()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int
buildSendBlastDial (int zCall, int zIndex)
{
	char            mod[] = "buildSendBlastDial";
	struct Msg_BlastDial msg;
	struct MsgToApp response;
	int             yRc;
	int             zOutboundCall;
	int             popCrossPort;
	int             waitCount = 0;
	int             ringCountIn100msUnits;
	int             rtpDetailsSent = 0;
	char            message[MAX_LOG_MSG_LENGTH];
	time_t          yCurrentTime, yConnectTime;
	char            yStrDestination[500] = "unset";
	char            yStrFromUrl[256] = "unset";
	char            yStrLocalSdpPort[10] = "";
	char            yStrPayloadNumber[10];
	char            yStrPayloadMime[255];
	char            yStrRemoteIPAddress[255] = "unset";
	int             informat;

	osip_message_t *invite = NULL;
	osip_message_t *send_ack = NULL;
	osip_message_t *answer = NULL;

	zOutboundCall = gCall[zCall].dlist[zIndex].outboundChannel;

	gCall[zCall].outboundRetCode = 0;
	gCall[zOutboundCall].parent_idx = zCall;
//  gCall[zCall].leg = LEG_A;
	gCall[zOutboundCall].leg = LEG_B;
	gCall[zOutboundCall].bridgeDestination[0] = 0;

#if 0

	if (gCall[zCall].callState != CALL_STATE_CALL_ACK)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Sending SIP Message 180 Ringing.");

		eXosip_lock (geXcontext);
		yRc = eXosip_call_build_answer (geXcontext, gCall[zCall].tid, 180, &answer);
		if (yRc == 0)
		{
			arc_fixup_contact_header (answer, gHostIp, gSipPort, zCall);
			yRc = eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 180, answer);
		}
		eXosip_unlock (geXcontext);
	}

#endif

//  memcpy (&(gCall[zOutboundCall].msgToDM), &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "appPid=%d appRef=%d.",
			   gCall[zCall].msgToDM.appPid, gCall[zCall].msgToDM.appRef);
	if (gCall[zCall].GV_SipAuthenticationUser[0])
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationUser, "%s",
				 gCall[zCall].GV_SipAuthenticationUser);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationUser, "%s",
				 gAuthUser);
	}

	if (gCall[zCall].GV_SipAuthenticationPassword[0])
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationPassword, "%s",
				 gCall[zCall].GV_SipAuthenticationPassword);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationPassword, "%s",
				 gAuthPassword);
	}

	if (gCall[zCall].GV_SipAuthenticationId[0])
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationId, "%s",
				 gCall[zCall].GV_SipAuthenticationId);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationId, "%s", gAuthId);
	}

	if (gCall[zCall].GV_SipAuthenticationRealm[0])
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationRealm, "%s",
				 gCall[zCall].GV_SipAuthenticationRealm);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationRealm, "%s",
				 gAuthRealm);
	}

	if (gCall[zCall].GV_SipFrom[0])
	{
		sprintf (gCall[zOutboundCall].GV_SipFrom, "%s",
				 gCall[zCall].GV_SipFrom);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipFrom, "%s", gFromUrl);
	}

	if (gCall[zCall].GV_DefaultGateway[0])
	{
		sprintf (gCall[zOutboundCall].GV_DefaultGateway, "%s",
				 gCall[zCall].GV_DefaultGateway);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_DefaultGateway, "%s",
				 gDefaultGateway);
	}

	gCallSetCrossRef (gCall, MAX_PORTS, zCall, &gCall[zOutboundCall],
					  zOutboundCall);
	popCrossPort = gCall[zCall].crossPort;

//  gCall[zCall].crossPort = zOutboundCall;

	gCall[zCall].leg = LEG_A;

	gCall[zOutboundCall].crossPort = zCall;
	gCall[zOutboundCall].canKillApp = NO;

	gCallDumpCrossRefs (gCall, zOutboundCall);
	gCallDumpCrossRefs (gCall, zCall);

//  yConnectTime = yConnectTime + 2 + msg.numRings * 6;

	gCall[zOutboundCall].remoteRtpPort = -1;

	char            yStrTmpIPAddress[256];
	char           *pch = NULL;	//strchr (msg.ipAddress, '@');

	sprintf (yStrTmpIPAddress, "%s", gCall[zCall].dlist[zIndex].destination);;
	informat = gCall[zCall].dlist[zIndex].inputType;
	pch = strstr (yStrTmpIPAddress, "@");

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "yStrTmpIPAddress=%s informat=%d, NONE=%d.",
			   yStrTmpIPAddress, gCall[zCall].dlist[zIndex].inputType, NONE);

	if (strstr (yStrTmpIPAddress, "sip:")
		|| strstr (yStrTmpIPAddress, "SIP:"))
	{
		if (pch == NULL)
		{
			if (informat == NONE)
			{
				sprintf (yStrDestination, "%s@%s;user=phone",
						 gCall[zCall].dlist[zIndex].destination,
						 gCall[zOutboundCall].GV_DefaultGateway);
				sprintf (yStrRemoteIPAddress, "%s",
						 gCall[zOutboundCall].GV_DefaultGateway);
			}
			else if (informat != E_164)
			{
				if (gCall[zCall].remoteRtpIp && gCall[zCall].remoteRtpIp[0])
				{
	int             yIntRemoteRtpIpLen = strlen (gCall[zCall].remoteRtpIp);

					if (gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] ==
						'\r'
						|| gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] ==
						'\n')
					{
						gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] = 0;
					}
				}

				//sprintf (yStrDestination, "%s@%s;user=phone", msg.phoneNumber, gCall[zCall].remoteRtpIp);
				sprintf (yStrDestination, "%s;user=phone", yStrTmpIPAddress);
				sscanf (yStrTmpIPAddress, "sip:%s", yStrRemoteIPAddress);
			}
		}
		else
		{
#if 1
	char           *tempChar = NULL;

			if (gEnableIPv6Support != 0)
			{
				tempChar = strstr (pch, "]:");
				sprintf (yStrDestination, "[%s]", yStrTmpIPAddress);
			}
			else
			{
				tempChar = strstr (pch, ":");
				sprintf (yStrDestination, "%s", yStrTmpIPAddress);
			}
#endif

			if (tempChar == NULL)
			{
				sprintf (yStrRemoteIPAddress, "%s", pch + 1);
			}
			else
			{
				snprintf (yStrRemoteIPAddress,
						  strlen (pch) - strlen (tempChar), "%s", pch + 1);
			}
		}
	}
	else
	{
		if (pch == NULL)
		{
			if (informat == NONE)
			{
				;;
#if 1
				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "sip:%s@[%s];user=phone",
							 gCall[zCall].dlist[zIndex].destination,
							 gCall[zOutboundCall].GV_DefaultGateway);
				}
				else
				{
					sprintf (yStrDestination, "sip:%s@%s;user=phone",
							 gCall[zCall].dlist[zIndex].destination,
							 gCall[zOutboundCall].GV_DefaultGateway);
				}
				sprintf (yStrRemoteIPAddress, "%s",
						 gCall[zOutboundCall].GV_DefaultGateway);
#endif
			}
			else if (informat != E_164)
			{
				if (gCall[zCall].remoteRtpIp && gCall[zCall].remoteRtpIp[0])
				{
	int             yIntRemoteRtpIpLen = strlen (gCall[zCall].remoteRtpIp);

					if (gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] ==
						'\r'
						|| gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] ==
						'\n')
					{
						gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] = 0;
					}
				}
				sprintf (yStrDestination, "sip:%s@%s;user=phone",
						 gCall[zCall].dlist[zIndex].destination,
						 gCall[zCall].remoteRtpIp);
				sprintf (yStrRemoteIPAddress, "%s", gCall[zCall].remoteRtpIp);
			}
		}
		else
		{
	char           *tempChar = strstr (pch, ":");

			sprintf (yStrDestination, "sip:%s",
					 gCall[zCall].dlist[zIndex].destination);
			if (tempChar == NULL)
			{
				sprintf (yStrRemoteIPAddress, "%s", pch + 1);
			}
			else
			{
				snprintf (yStrRemoteIPAddress,
						  strlen (pch) - strlen (tempChar), "%s", pch + 1);
			}
		}
	}

//fprintf(stderr, " %s(%d) yStrRemoteIPAddress = %s\n",__func__, __LINE__, yStrRemoteIPAddress);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "yStrDestination=(%s)", yStrDestination);
//fprintf(stderr, " %s(%d) yStrDestination = %s\n", __func__, __LINE__, yStrDestination);

	if (gCall[zOutboundCall].GV_SipFrom[0])
	{
		if (gCall[zCall].GV_CallerName[0])
		{
			sprintf (yStrFromUrl, "\"%s\" <sip:%s>",
					 gCall[zCall].GV_CallerName, gCall[zCall].GV_SipFrom);
		}
		else
		{
			sprintf (yStrFromUrl, "sip:%s", gCall[zOutboundCall].GV_SipFrom);
		}
	}
	else if (gCall[zOutboundCall].GV_SipAuthenticationUser[0])
	{
		if (gCall[zCall].GV_CallerName[0])
		{
			sprintf (yStrFromUrl, "\"%s\" <sip:%s@%s>",
					 gCall[zCall].GV_CallerName,
					 gCall[zOutboundCall].GV_SipAuthenticationUser, gHostIp);
		}
		else
		{
			sprintf (yStrFromUrl, "sip:%s@%s",
					 gCall[zOutboundCall].GV_SipAuthenticationUser, gHostIp);
		}
	}
	else
	{
		if (gCall[zCall].GV_CallerName[0])
		{
			sprintf (yStrFromUrl, "\"%s\" <sip:arc@%s>",
					 gCall[zCall].GV_CallerName, gHostIp);
		}
		else
		{
			sprintf (yStrFromUrl, "sip:arc@%s", gHostIp);
		}
	}

	//
	// enum will plow over the previous mess 
	//

	if ((informat == E_164) && gSipEnableEnum)
	{
	char           *enum_ip = NULL;
	char           *enum_uri = NULL;
	struct ares_naptr_list_t *list = NULL;
	struct hostent *host = gethostbyname (yStrRemoteIPAddress);

		enum_ip =
			enum_convert (gCall[zCall].dlist[zIndex].destination,
						  gSipEnumTLD);

		if (enum_ip)
		{
			enum_lookup (&list, enum_ip, 0);
			if (list)
			{
				enum_match (list, ENUM_SERVICE_SIP,
							gCall[zCall].dlist[zIndex].destination,
							&enum_uri);
				if (enum_uri)
				{
					snprintf (yStrDestination, sizeof (yStrDestination), "%s",
							  enum_uri);
				}
			}
		}

		if (enum_ip)
			free (enum_ip);
		if (enum_uri)
			free (enum_uri);
	}
	// end enum lookups

	eXosip_lock (geXcontext);
	arc_add_authentication_info (gCall[zOutboundCall].GV_SipAuthenticationUser, gCall[zOutboundCall].GV_SipAuthenticationId, gCall[zOutboundCall].GV_SipAuthenticationPassword, "",	/*Algorith,  default MD5 */
								 gCall[zOutboundCall].
								 GV_SipAuthenticationRealm);

	if (strstr (yStrDestination, "sip:") == NULL)
	{
		sprintf (gCall[zOutboundCall].bridgeDestination, "%s",
				 yStrDestination);
	}
	else
	{
		sprintf (gCall[zOutboundCall].bridgeDestination + 4, "%s",
				 yStrDestination);
	}

	if (gOutboundProxyRequired)
	{
	char            yStrTmpProxy[256];

		sprintf (yStrTmpProxy, "sip:%s", gOutboundProxy);

		yRc = eXosip_call_build_initial_invite (geXcontext, &invite, yStrTmpProxy,
												yStrFromUrl,
												yStrDestination, "");

// #define GLOBAL_CROSSING 
// do this from the make file

#ifdef GLOBAL_CROSSING
		if (1)
		{
	char            newCallId[256];

			sprintf (newCallId, "%s@%s", invite->call_id->number, gHostIp);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Changed call id from %s to %s.",
					   invite->call_id->number, newCallId);

			osip_call_id_free (invite->call_id);

			invite->call_id = NULL;

			yRc = osip_message_set_call_id (invite, (const char *) newCallId);
		}
#endif

	}
	else
	{
#if 1
//RG added to check if the remote ip is known
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "yStrRemoteIPAddress=%s", yStrRemoteIPAddress);

//fprintf(stderr, " %s(%d) yStrRemoteIPAddress=%s\n", __func__,  __LINE__, yStrRemoteIPAddress);

	int             rc = lookup_host (yStrRemoteIPAddress);

		if (rc == -1)
		{
//          gCall[zCall].outboundRetCode =  54;
//          writeGenericResponseToApp (__LINE__, zCall, &response, mod);
//
//          gCall[zCall].crossPort = -1;
//          gCall[zCall].leg = LEG_A;
//
//          gCall[zOutboundCall].crossPort = -1;
//          gCall[zOutboundCall].leg = LEG_A;
			eXosip_unlock (geXcontext);
			gCall[zCall].dlist[zIndex].outboundChannel = -1;
			gCall[zCall].dlist[zIndex].resultCode = -54;
			sprintf (gCall[zCall].dlist[zIndex].resultData, "%s",
					 "Bad Destination");

			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_SIP_SIGNALING,
					   ERR,
					   "lookup_host() failed.  Unable to bridge the call, could not build initial invite.");

			return (-1);

		}
#endif

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Building invite for (%s)", yStrDestination);
		yRc = eXosip_call_build_initial_invite (geXcontext, &invite,
												yStrDestination,
												yStrFromUrl,
												gCall[zCall].GV_RouteHeader,
												"");

#ifdef GLOBAL_CROSSING
		if (1)
		{
	char            newCallId[256];

			sprintf (newCallId, "%s@%s", invite->call_id->number, gHostIp);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Changed call id from %s to %s.",
					   invite->call_id->number, newCallId);

			osip_call_id_free (invite->call_id);

			invite->call_id = NULL;

			yRc = osip_message_set_call_id (invite, (const char *) newCallId);
		}
#endif
	}

	if (yRc < 0)
	{
		eXosip_unlock (geXcontext);
		gCall[zCall].dlist[zIndex].resultCode = -1;
		sprintf (gCall[zCall].dlist[zIndex].resultData, "%s",
				 "Failed to bridge the call, could not build initial invite");
		return (-1);
	}

	gCall[zCall].currentInvite = invite;

	sprintf (yStrLocalSdpPort, "%d",
			 LOCAL_STARTING_RTP_PORT + (zOutboundCall * 2));

//	if (strstr (gCall[zCall].call_type, "Video"))
//	{
//		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//				   "Calling createBridgeSdpBody, crossPort=%d.",
//				   zOutboundCall);
//
//		createBridgeSdpBody (zOutboundCall, yStrLocalSdpPort,
//							 //gCall[zCall].codecType, 
//							 -1, gCall[zCall].telephonePayloadType, zCall);
//	}
//	else
//	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling createSdpBody, crossPort=%d.", zOutboundCall);
		createSdpBody (zOutboundCall, yStrLocalSdpPort,
					   gCall[zCall].codecType, -2,
					   gCall[zCall].telephonePayloadType);
//	}

/*MR_3069:  Check the config file.  If Prack is required, include it in the outbound INVITE message.*/
	if (gPrackMethod == PRACK_REQUIRE)
	{
		osip_message_set_header (invite, "Require", "100rel");
		gCall[zCall].UAC_PRACKOKReceived = 0;
		gCall[zCall].UAC_PRACKRingCounter = 0;
	}
	else if (gPrackMethod == PRACK_SUPPORTED)
	{
		osip_message_set_header (invite, "Supported", "100rel");
		gCall[zCall].UAC_PRACKOKReceived = 0;
		gCall[zCall].UAC_PRACKRingCounter = 0;
	}

	if (gCall[zCall].GV_PAssertedIdentity[0] != '\0')
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Setting P-Asserted-Identity = %s",
				   gCall[zCall].GV_PAssertedIdentity);
		osip_message_set_header (invite, "P-Asserted-Identity",
								 gCall[zCall].GV_PAssertedIdentity);
	}

	if (gCall[zCall].GV_Diversion[0] != '\0')
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Setting Diversion = %s", gCall[zCall].GV_Diversion);
		osip_message_set_header (invite, "Diversion",
								 gCall[zCall].GV_Diversion);
	}

	if (gCall[zCall].GV_CallInfo[0] != '\0')
	{
		osip_message_set_header (invite, "Call-Info",
								 gCall[zCall].GV_CallInfo);
	}

	osip_message_set_body (invite, gCall[zOutboundCall].sdp_body_local,
						   strlen (gCall[zOutboundCall].sdp_body_local));
	osip_message_set_content_type (invite, "application/sdp");

	arc_fixup_contact_header (answer, gHostIp, gSipPort, zCall);

	arc_insert_global_headers (zCall, invite);

	yRc = eXosip_call_send_initial_invite (geXcontext, invite);
	eXosip_unlock (geXcontext);

	if (yRc < 0)
	{
		gCall[zCall].dlist[zIndex].resultCode = -1;
		sprintf (gCall[zCall].dlist[zIndex].resultData, "%s",
				 "Failed to bridge the call, could not send initial invite");
		return (-1);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Setting cid = %d for outbound port %d", yRc, zOutboundCall);

	gCall[zOutboundCall].lastError = CALL_NO_ERROR;
	gCall[zOutboundCall].cid = yRc;

//  response.returnCode = 0;

	return (0);
}								/*END: void * buildSendBlastDial */

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
int
stopBlastDialCalls (int zCall, int zNotThisPort, int zNumDests)
{
	static char     mod[] = "stopBlastDialCalls";
	int             zOutboundCall;
	int             i;
	time_t          yCurrentTime;
	time_t          yConnectTime;

	for (i = 0; i < zNumDests; i++)
	{
		if ((i == zNotThisPort) || (gCall[zCall].dlist[i].resultCode == -54))
		{
			continue;
		}

		zOutboundCall = gCall[zCall].dlist[i].outboundChannel;

		gCall[zCall].dlist[i].outboundChannel = -1;
		if (gCall[zCall].GV_CancelOutboundCall == YES)
		{
			gCall[zCall].dlist[i].resultCode = -55;
			sprintf (gCall[zCall].dlist[i].resultData, "%s",
					 "Call Cancelled");
		}
		else if (gCall[zOutboundCall].callState == CALL_STATE_CALL_RINGING)
		{
			gCall[zCall].dlist[i].resultCode = -50;
			sprintf (gCall[zCall].dlist[i].resultData, "%s",
					 "Ring no answer");
		}
		else if (gCall[zOutboundCall].callState ==
				 CALL_STATE_CALL_REQUESTFAILURE)
		{
			gCall[zOutboundCall].outboundRetCode =
				gCall[zOutboundCall].eXosipStatusCode;
			gCall[zCall].dlist[i].resultCode =
				-1 * mapSipErrorToArcError (zCall,
											gCall[zOutboundCall].
											eXosipStatusCode);
			sprintf (gCall[zCall].dlist[i].resultData, "%d:%s",
					 mapSipErrorToArcError (zCall,
											gCall[zOutboundCall].
											eXosipStatusCode),
					 gCall[zOutboundCall].eXosipReasonPhrase);
		}
		else
		{
			gCall[zCall].dlist[i].resultCode = -52;
			sprintf (gCall[zCall].dlist[i].resultData, "%s", "No ring back");
		}

		//response.returnCode = -50;

		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
				   "Sending SIP Message call_terminate (603) outbound port %d, %s.",
				   i, gCall[zCall].dlist[i].destination);

		eXosip_lock (geXcontext);
		eXosip_call_terminate (geXcontext, gCall[zOutboundCall].cid,
							   gCall[zOutboundCall].did);
		time (&(gCall[zOutboundCall].lastReleaseTime));
		eXosip_unlock (geXcontext);

		gCall[zOutboundCall].cid = -1;

		setCallState (zOutboundCall, CALL_STATE_CALL_CLOSED);
	}

	return (0);
}								// END: stopBlastDialCalls

void           *
blastDialThread (void *z)
{
	char            mod[] = "blastDialThread";
	struct MsgToDM *ptrMsgToDM;
	struct Msg_InitiateCall initCallMsg;
	struct MsgToApp response;

	int             numDests;
	int             i;
	int             j, k;
	int             tmpNumDests;
	int             zCall;
	struct Msg_BlastDial msg;
	int             rc;
	time_t          yCurrentTime;
	time_t          yConnectTime;
	int             atleastOneSuccess;
	char            tmpStr[128];
	char            responseStr[512];
	int             zOutboundCall;
	int             popCrossPort;
	int             waitCount = 0;
	int             ringCountIn100msUnits;
	int             rtpDetailsSent = 0;
	char            message[MAX_LOG_MSG_LENGTH];
	char            yStrDestination[500];
	char            yStrFromUrl[256];
	char            yStrLocalSdpPort[10];
	char            yStrPayloadNumber[10];
	char            yStrPayloadMime[255];
	char            yStrRemoteIPAddress[255];
	char            yBlastResultFile[128];

	osip_message_t *invite = NULL;
	osip_message_t *send_ack = NULL;
	osip_message_t *answer = NULL;

	time (&yCurrentTime);
	time (&yConnectTime);

	memset ((char *) responseStr, '\0', sizeof (responseStr));

	msg = *(struct Msg_BlastDial *) z;
	zCall = msg.appCallNum;

	memset ((struct Msg_InitiateCall *) &initCallMsg, '\0',
			sizeof (initCallMsg));
	memcpy (&initCallMsg, z, sizeof (initCallMsg));
	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	rc = getBlastDialDestList (zCall, msg.dataFile, &numDests);
	if ((rc == -1) || ((numDests < 1) || (numDests > 5)))
	{
		response.returnCode = -1;
		sprintf (response.message,
				 "Invalid number of destinations (%d). "
				 "Must be between 1 and 5.", numDests);
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);
		clearPort (zCall);
		if (access (msg.dataFile, R_OK) > 0)
		{
			(void) unlink (msg.dataFile);
		}
		pthread_detach (pthread_self ());
		pthread_exit (NULL);
	}

	if (access (msg.dataFile, R_OK) > 0)
	{
		(void) unlink (msg.dataFile);
	}

	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	gCall[zCall].outboundRetCode = 0;

	for (i = 0; i < numDests; i++)
	{
		gCall[zCall].blastDialPorts[i] = getFreeCallNum (OUTBOUND);
		if (gCall[zCall].blastDialPorts[i] < 0)
		{
			// The log message is logged in the getFreeCallNum()
			// routine.  No need here.
			//
//			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
//					   TEL_RESOURCE_NOT_AVAILABLE, ERR,
//					   "Failed to get a free slot for outbound call.");
			gCall[zCall].blastDialPorts[i] = -1;
		}
		else
		{
	int             yIntTmpOutboundPort = gCall[zCall].blastDialPorts[i];
	char            blastDialFile[256] = "";

			clearPort (gCall[zCall].blastDialPorts[i]);

			sprintf (blastDialFile, "/tmp/.blastDialResult.%d",
					 yIntTmpOutboundPort);
			if (access (blastDialFile, R_OK) == 0)
			{
				unlink (blastDialFile);
			}
			setCallState (yIntTmpOutboundPort, CALL_STATE_RESERVED_NEW_CALL);

			gCall[zCall].dlist[i].outboundChannel =
				gCall[zCall].blastDialPorts[i];
		}
	}

	if (i < 1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
				   TEL_RESOURCE_NOT_AVAILABLE, ERR,
				   "Free call slot not available.");

		response.returnCode = -1;
		sprintf (response.message, "%s", "No free slots available.");
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);
		clearPort (zCall);
		pthread_detach (pthread_self ());
		pthread_exit (NULL);
	}

	gCall[zCall].blastDialAnswered = 0;
	atleastOneSuccess = 0;
	tmpNumDests = numDests;
	for (i = 0; i < numDests; i++)
	{
		rc = buildSendBlastDial (zCall, i);
		if (rc == 0)
		{
			atleastOneSuccess = 1;
//          gCall[zOutboundCall].parent_idx = zCall;
//          gCall[zOutboundCall].leg = LEG_B;
//          gCall[zOutboundCall].bridgeDestination[0] = 0;
		}
		else
		{
			gCall[zCall].blastDialPorts[i] = -1;
			tmpNumDests -= 1;
		}
	}

//  gCall[zCall].numDests = tmpNumDests;
	gCall[zCall].numDests = numDests;
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "gCall[%d].numDests set to %d.", zCall, gCall[zCall].numDests);

//    gCallSetCrossRef(gCall, MAX_PORTS, zCall, &gCall[zOutboundCall], zOutboundCall);
//   popCrossPort = gCall[zCall].crossPort;
//  gCall[zCall].crossPort = zOutboundCall;

	// gCall[zCall].leg = LEG_A;

//  gCall[zOutboundCall].crossPort = zCall;
//  gCall[zOutboundCall].canKillApp = NO;

//   gCallDumpCrossRefs(gCall, zOutboundCall);
//  gCallDumpCrossRefs(gCall, zCall);

	yConnectTime = yConnectTime + 2 + msg.numRings * 6;

	if (!atleastOneSuccess)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_SIP_SIGNALING,
				   ERR,
				   "All attempts to build and send sip invite failed.  Returning failure.");

		response.returnCode = -1;

		if ((rc = createBlastDialResultFile (zCall, yBlastResultFile)) == 0)
		{
			sprintf (response.message, "%s", yBlastResultFile);
		}
		else
		{
			sprintf (response.message, "%s",
					 "Unable to creat blastDial result file so cannot send failure results to app.  "
					 "All attempts to build and send sip invite failed.  Returning failure.");
		}

		writeGenericResponseToApp (__LINE__, zCall, &response, mod);
//      clearPort(zCall);
		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return (NULL);
	}

	while (!gCall[zCall].blastDialAnswered)
	{
		if (yCurrentTime >= yConnectTime
			|| gCall[zCall].GV_CancelOutboundCall == YES)
		{
//            struct Msg_SetGlobal yMsgSetGlobal;
//
//            yMsgSetGlobal.opcode        = DMOP_SETGLOBAL;
//            yMsgSetGlobal.appPid        = gCall[zCall].msgToDM.appPid;
//            yMsgSetGlobal.appRef        = 99999;
//            yMsgSetGlobal.appPassword   = zCall;
//            yMsgSetGlobal.appCallNum    = zCall;
//
//            sprintf(yMsgSetGlobal.name, "$INTERNAL_BRIDGE_RINGING");
//            yMsgSetGlobal.value  = 0;

			rc = stopBlastDialCalls (zCall, -1, gCall[zCall].numDests);
			response.returnCode = -1;
			if ((rc =
				 createBlastDialResultFile (zCall, yBlastResultFile)) == 0)
			{
				sprintf (response.message, "%s", yBlastResultFile);
			}
			else
			{
				sprintf (response.message, "%s",
						 "Unable to creat blastDial result file so cannot send failure results to app.  "
						 "All attempts to build and send sip invite failed.  Returning failure.");
			}
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);

			gCall[zCall].isInBlastDial = 0;
			gCall[zCall].crossPort = -1;
			gCall[zCall].leg = LEG_A;
			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}
		else if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED
				 || gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
				 || gCall[zCall].callState == CALL_STATE_CALL_TERMINATE_CALLED
				 || gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
					   TEL_CALLER_DISCONNECTED, WARN,
					   "blastDial failed, A Leg(%d) already disconnected.",
					   zCall);
			setCallSubState (zOutboundCall, CALL_STATE_CALL_CLOSED);

			gCall[zCall].leg = LEG_A;

//          struct Msg_SetGlobal yMsgSetGlobal;
//                          
//          yMsgSetGlobal.opcode        = DMOP_SETGLOBAL;
//          yMsgSetGlobal.appPid        = gCall[zCall].msgToDM.appPid;
//          yMsgSetGlobal.appRef        = 99999;
//          yMsgSetGlobal.appPassword   = zCall;
//          yMsgSetGlobal.appCallNum    = zCall;
//
//          sprintf(yMsgSetGlobal.name, "$INTERNAL_BRIDGE_RINGING");
//          yMsgSetGlobal.value  = 0;
//          notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &yMsgSetGlobal, mod);

			rc = stopBlastDialCalls (zCall, -1, gCall[zCall].numDests);
			response.returnCode = -1;

//RGDEBUG :: Blast dial results are alread returned from EXOSIP_CALL_CLOSED.
#if 0
			if ((rc =
				 createBlastDialResultFile (zCall, yBlastResultFile)) == 0)
			{
				sprintf (response.message, "%s", yBlastResultFile);
			}
			else
			{
				sprintf (response.message, "%s",
						 "Unable to creat blastDial result file so cannot send failure results to app.  "
						 "All attempts to build and send sip invite failed.  Returning failure.");
			}
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
#endif

			gCall[zCall].crossPort = -1;
			gCall[zCall].leg = LEG_A;
			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}

		for (i = 0; i < gCall[zCall].numDests; i++)
		{
			if (gCall[zCall].blastDialPorts[i] == -1)
			{
				continue;
			}
			zOutboundCall = gCall[zCall].dlist[i].outboundChannel;

			if (gCall[zOutboundCall].callState == CALL_STATE_CALL_ANSWERED)
			{
				gCall[zCall].crossPort = zOutboundCall;
				gCall[zCall].blastDialAnswered = 1;
				gCall[zCall].dlist[i].resultCode = 0;
				gCall[zCall].dlist[i].outboundChannel = zOutboundCall;
				sprintf (gCall[zCall].dlist[i].resultData, "%s",
						 "Call Answered");

				rc = stopBlastDialCalls (zCall, i, gCall[zCall].numDests);

				eXosip_lock (geXcontext);

				eXosip_call_build_ack (geXcontext, gCall[zOutboundCall].did, &send_ack);
				if (send_ack != NULL)
				{
					arc_fixup_contact_header (send_ack, gHostIp, gSipPort,
											  zCall);
					eXosip_call_send_ack (geXcontext, gCall[zOutboundCall].did, send_ack);
				}
				eXosip_unlock (geXcontext);

				for (j = 0; j < gCall[zCall].numDests; j++)
				{
					if (gCall[zCall].dlist[j].outboundChannel ==
						zOutboundCall)
					{
						break;
					}
				}
				gCall[zOutboundCall].appPid = msg.appPid;
				gCall[zCall].appPid = msg.appPid;

				if (gCall[zCall].dlist[j].inputType == IP)
				{
					sprintf (initCallMsg.ipAddress, "%s",
							 gCall[zCall].dlist[j].destination);
					memset (initCallMsg.phoneNumber, 0,
							sizeof (initCallMsg.phoneNumber));
				}
				else
				{
					sprintf (initCallMsg.phoneNumber, "%s",
							 gCall[zCall].dlist[j].destination);
					memset (initCallMsg.ipAddress, 0,
							sizeof (initCallMsg.phoneNumber));
				}

				initCallMsg.opcode = DMOP_INITIATECALL;
				initCallMsg.whichOne = 1;
				initCallMsg.appCallNum1 = zCall;
				initCallMsg.appCallNum2 = zOutboundCall;
				memset ((char *) initCallMsg.ani, '\0',
						sizeof (initCallMsg.ani));
				initCallMsg.informat = gCall[zCall].dlist[j].inputType;
				initCallMsg.listenType = 1;
				memset ((char *) initCallMsg.resourceType, '\0',
						sizeof (initCallMsg.resourceType));
				rc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(initCallMsg),
									 mod);

//              dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
//                  "DJB: %d=notifyMediaMgr: opcode=%d appCallNum=%d "
//                  "appPid=%d appRef=%d appPassword=%d numRings=%d ipAddress=(%s) "
//                  "phoneNumber=(%s) whichOne=%d appCallNum1=%d appCallNum2=%d "
//                  "ani=(%s) informat=%d listenType=%d resourceType=(%s)",
//                  rc, initCallMsg.opcode, initCallMsg.appCallNum, initCallMsg.appPid,
//                  initCallMsg.appRef, initCallMsg.appPassword,
//                  initCallMsg.numRings, initCallMsg.ipAddress, initCallMsg.phoneNumber,
//                  initCallMsg.whichOne, initCallMsg.appCallNum1, initCallMsg.appCallNum2,
//                  initCallMsg.ani, initCallMsg.informat, initCallMsg.listenType,
//                  initCallMsg.resourceType);
//  
//  //          sprintf (response.message, "%d", zOutboundCall);
				//          djb - Build response

				if (gCall[zOutboundCall].remoteRtpPort > 0 && !rtpDetailsSent)
				{
	struct MsgToDM  msgRtpDetails;

					memcpy (&msgRtpDetails, &(gCall[zOutboundCall].msgToDM),
							sizeof (struct MsgToDM));
					if (gCall[zOutboundCall].leg == LEG_B
						&& gCall[zOutboundCall].crossPort >= 0)
					{
	int             yALegPort = gCall[zOutboundCall].crossPort;

						gCall[zOutboundCall].codecType =
							gCall[yALegPort].codecType;
						gCall[zOutboundCall].full_duplex =
							gCall[yALegPort].full_duplex;
						gCall[zOutboundCall].telephonePayloadType =
							gCall[yALegPort].telephonePayloadType;
					}

					msgRtpDetails.opcode = DMOP_RTPDETAILS;

					sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
							 gCall[zOutboundCall].leg,
							 gCall[zOutboundCall].remoteRtpPort,
							 gCall[zOutboundCall].localSdpPort,
							 (gCall[zOutboundCall].
							  telephonePayloadPresent) ? gCall[zOutboundCall].
							 telephonePayloadType : -99,
							 gCall[zOutboundCall].full_duplex,
							 gCall[zOutboundCall].codecType,
							 gCall[zOutboundCall].remoteRtpIp);

					rc = notifyMediaMgr (__LINE__, zOutboundCall,
										 (struct MsgToDM *) &(msgRtpDetails),
										 mod);
					rtpDetailsSent = 1;
				}

				pthread_detach (pthread_self ());
				pthread_exit (NULL);
				return (NULL);

#if 0
				if (gCall[zOutboundCall].callState ==
					CALL_STATE_CALL_REDIRECTED)
				{
					dynVarLog (__LINE__, zOutboundCall, mod, REPORT_DETAIL,
							   TEL_BASE, INFO,
							   "Outbound Call has been redirected");

					response.returnCode =
						-1 * mapSipErrorToArcError (zOutboundCall, 300);
					sprintf (response.message, "%d:%s", 22,
							 "300 Multiple Choice");

					sprintf (gCall[zCall].redirectedContacts, "%s",
							 gCall[zOutboundCall].redirectedContacts);

					writeGenericResponseToApp (__LINE__, zCall, &response, mod);

					//              if(msg.opcode == DMOP_BRIDGE_THIRD_LEG)
					//              {
					//                  gCall[zCall].crossPort = popCrossPort;      //Put the original B Leg back
					////                }
					//              else
					//              {
					gCall[zCall].crossPort = -1;
					//              }

					gCall[zCall].leg = LEG_A;

					gCall[zOutboundCall].crossPort = -1;
					gCall[zOutboundCall].leg = LEG_A;

					gCall[zOutboundCall].cid = -1;
					setCallState (zOutboundCall, CALL_STATE_CALL_RELEASED);

					pthread_detach (pthread_self ());
					pthread_exit (NULL);
					return (NULL);
				}
#endif

			}

			usleep (10 * 1000);
			time (&yCurrentTime);

			if (gCall[zCall].blastDialPorts[i] == -1)
			{
				continue;
			}

			zOutboundCall = gCall[zCall].blastDialPorts[i];
			if (gCall[zOutboundCall].callState != CALL_STATE_CALL_ANSWERED ||
				gCall[zOutboundCall].remoteRtpPort <= 0)
			{
				//          usleep (10 * 1000);

#if 0
				dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE,
						   INFO,
						   "bridge port(%d) A Leg(%d) callState of A(%d) popCrossPort=%d callState of popCrossPort(%d).",
						   zOutboundCall, zCall, gCall[zCall].callState,
						   popCrossPort, gCall[popCrossPort].callState);
#endif

				// fprintf(stderr, " %s() call state == %d\n", __func__, gCall[zOutboundCall].callState);

#if 0
				if (gCall[zOutboundCall].callState ==
					CALL_STATE_CALL_REDIRECTED)
				{
					dynVarLog (__LINE__, zOutboundCall, mod, REPORT_DETAIL,
							   TEL_BASE, INFO,
							   "Outbound Call has been redirected");

					response.returnCode =
						-1 * mapSipErrorToArcError (zOutboundCall, 300);
					sprintf (response.message, "%d:%s", 22,
							 "300 Multiple Choice");

					sprintf (gCall[zCall].redirectedContacts, "%s",
							 gCall[zOutboundCall].redirectedContacts);

					writeGenericResponseToApp (__LINE__, zCall, &response, mod);

					//              if(msg.opcode == DMOP_BRIDGE_THIRD_LEG)
					//              {
					//                  gCall[zCall].crossPort = popCrossPort;      //Put the original B Leg back
					////                }
					//              else
					//              {
					gCall[zCall].crossPort = -1;
					//              }

					gCall[zCall].leg = LEG_A;

					gCall[zOutboundCall].crossPort = -1;
					gCall[zOutboundCall].leg = LEG_A;

					gCall[zOutboundCall].cid = -1;
					setCallState (zOutboundCall, CALL_STATE_CALL_RELEASED);

					pthread_detach (pthread_self ());
					pthread_exit (NULL);
					return (NULL);
				}
#endif

				time (&yCurrentTime);
			}
			else if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED
					 || gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
					 || gCall[zCall].callState ==
					 CALL_STATE_CALL_TERMINATE_CALLED
					 || gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
						   TEL_CALLER_DISCONNECTED, WARN,
						   "Failed to bridge(%d) the call, A Leg(%d) already disconnected.",
						   zOutboundCall, zCall);
				setCallSubState (zOutboundCall, CALL_STATE_CALL_CLOSED);

				//              if(msg.opcode == DMOP_BRIDGE_THIRD_LEG)
				//              {
				//                  gCall[zCall].crossPort = popCrossPort;      //Put the original B Leg back
				//              }
				//              else
				//              {
				gCall[zCall].crossPort = -1;
				//              }

				gCall[zCall].leg = LEG_A;
				for (i = 0; i < gCall[zCall].numDests; i++)
				{
					gCall[i].crossPort = -1;
					gCall[i].leg = LEG_A;

					eXosip_lock (geXcontext);
					eXosip_call_terminate (geXcontext, gCall[zOutboundCall].cid,
										   gCall[zOutboundCall].did);
					time (&(gCall[zOutboundCall].lastReleaseTime));
					eXosip_unlock (geXcontext);

				}

	struct Msg_SetGlobal yMsgSetGlobal;

				yMsgSetGlobal.opcode = DMOP_SETGLOBAL;
				yMsgSetGlobal.appPid = gCall[zCall].msgToDM.appPid;
				yMsgSetGlobal.appRef = 99999;
				yMsgSetGlobal.appPassword = zCall;
				yMsgSetGlobal.appCallNum = zCall;

				sprintf (yMsgSetGlobal.name, "$INTERNAL_BRIDGE_RINGING");
				yMsgSetGlobal.value = 0;
				notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &yMsgSetGlobal,
								mod);

				gCall[zOutboundCall].cid = -1;

				setCallState (zOutboundCall, CALL_STATE_CALL_CLOSED);
				//gCall[zOutboundCall].callState = CALL_STATE_CALL_CLOSED;

				//              pthread_detach (pthread_self ());
				//              pthread_exit (NULL);

				return (NULL);
			}
			else if (gCall[zOutboundCall].callState ==
					 CALL_STATE_CALL_RINGING)
			{
				if (gEarlyMediaSupported == 1 &&
					gCall[zOutboundCall].remoteRtpPort > 0 && !rtpDetailsSent)
				{
	struct MsgToDM  msgRtpDetails;

					/*Send Pre Initiate Message */
	struct Msg_InitiateCall yTmpMsgInitiate;
					memcpy (&yTmpMsgInitiate, &(msg),
							sizeof (struct Msg_InitiateCall));
					yTmpMsgInitiate.appCallNum2 = zOutboundCall;
					yTmpMsgInitiate.appCallNum1 = zCall;
					yTmpMsgInitiate.appCallNum = zCall;
					yTmpMsgInitiate.opcode = DMOP_MEDIACONNECT;
					yTmpMsgInitiate.appPid = msg.appPid;
					rc = notifyMediaMgr (__LINE__, zCall,
										 (struct MsgToDM *)
										 &(yTmpMsgInitiate), mod);
					/*END: Send Pre Initiate Message */

					memcpy (&msgRtpDetails, &(gCall[zOutboundCall].msgToDM),
							sizeof (struct MsgToDM));
					gCall[zOutboundCall].codecType = gCall[zCall].codecType;
					gCall[zOutboundCall].full_duplex =
						gCall[zCall].full_duplex;
					gCall[zOutboundCall].telephonePayloadType =
						gCall[zCall].telephonePayloadType;
					msgRtpDetails.opcode = DMOP_RTPDETAILS;
					sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
							 gCall[zOutboundCall].leg,
							 gCall[zOutboundCall].remoteRtpPort,
							 gCall[zOutboundCall].localSdpPort,
							 (gCall[zOutboundCall].
							  telephonePayloadPresent) ? gCall[zOutboundCall].
							 telephonePayloadType : -99,
							 gCall[zOutboundCall].full_duplex,
							 gCall[zOutboundCall].codecType,
							 gCall[zOutboundCall].remoteRtpIp);
					rc = notifyMediaMgr (__LINE__, zOutboundCall,
										 (struct MsgToDM *) &(msgRtpDetails),
										 mod);
					rtpDetailsSent = 1;
					setCallSubState (zOutboundCall,
									 CALL_STATE_CALL_MEDIACONNECTED);
					setCallSubState (zCall, CALL_STATE_CALL_MEDIACONNECTED);
				}
			}

			if (gCall[zCall].callState != CALL_STATE_CALL_ACK
				&& gCall[zCall].isCallAnswered == NO)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Sending SIP Message 180 Ringing.");

				eXosip_lock (geXcontext);
				rc = eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 180, NULL);
				eXosip_unlock (geXcontext);
			}
		}						// END: for
	}							/*END: while */

	eXosip_lock (geXcontext);
	eXosip_call_build_ack (geXcontext, gCall[zOutboundCall].did, &send_ack);
	if (send_ack != NULL)
	{
		arc_fixup_contact_header (send_ack, gHostIp, gSipPort, zCall);
		eXosip_call_send_ack (geXcontext, gCall[zOutboundCall].did, send_ack);
	}
	eXosip_unlock (geXcontext);

	gCall[zOutboundCall].appPid = msg.appPid;

	gCall[zCall].appPid = msg.appPid;

	sprintf (response.message, "%d", zOutboundCall);

//  yRc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msg), mod);

	if (gCall[zOutboundCall].remoteRtpPort > 0 && !rtpDetailsSent)
	{

	struct MsgToDM  msgRtpDetails;

		memcpy (&msgRtpDetails, &(gCall[zOutboundCall].msgToDM),
				sizeof (struct MsgToDM));

		if (gCall[zOutboundCall].leg == LEG_B
			&& gCall[zOutboundCall].crossPort >= 0)
		{
	int             yALegPort = gCall[zOutboundCall].crossPort;

			gCall[zOutboundCall].codecType = gCall[yALegPort].codecType;

			gCall[zOutboundCall].full_duplex = gCall[yALegPort].full_duplex;

			gCall[zOutboundCall].telephonePayloadType =
				gCall[yALegPort].telephonePayloadType;
		}

		msgRtpDetails.opcode = DMOP_RTPDETAILS;

		sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
				 gCall[zOutboundCall].leg,
				 gCall[zOutboundCall].remoteRtpPort,
				 gCall[zOutboundCall].localSdpPort,
				 (gCall[zOutboundCall].
				  telephonePayloadPresent) ? gCall[zOutboundCall].
				 telephonePayloadType : -99, gCall[zOutboundCall].full_duplex,
				 gCall[zOutboundCall].codecType,
				 gCall[zOutboundCall].remoteRtpIp);

		rc = notifyMediaMgr (__LINE__, zOutboundCall,
							 (struct MsgToDM *) &(msgRtpDetails), mod);
		rtpDetailsSent = 1;
	}

	pthread_detach (pthread_self ());
	pthread_exit (NULL);
	return (NULL);

}								/*END: void * blastDialThread */

/*-----------------------------------------------------------------------------
createBlastDialResultFile():
-----------------------------------------------------------------------------*/
static int
createBlastDialResultFile (int zCall, char *zResultFile)
{
	static char     mod[] = "createBlastDialResultFile";
	FILE           *fp;
	char            fileName[128];
	int             i;

	/* Create a file and send it */
	// sprintf(fileName, ".blastDial.%d", port);
	sprintf (fileName, "%s.%d", BLASTDIAL_RESULT_FILE_BASE, zCall);
	*zResultFile = '\0';

	if (gCall[zCall].numDests <= 0)
	{
		if (access (fileName, R_OK) > 0)
		{
			(void) unlink (fileName);
		}
		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
				   "No multiple calls for port %d; will not create "
				   "result file (%s).", zCall, fileName);
		return (0);
	}

	if ((fp = fopen (fileName, "w+")) == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
				   ERR,
				   "Failed to open file (%s). [%d, %s].  Unable to send "
				   "blastDial results to application.", fileName, errno,
				   strerror (errno));

		if (access (fileName, R_OK) > 0)
		{
			(void) unlink (fileName);
		}
		return (-1);
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Successfully opened file (%s) for writing.", fileName);

	for (i = 0; i < gCall[zCall].numDests; i++)
	{
		fprintf (fp, "%s,%d,%d,%s\n",
				 gCall[zCall].dlist[i].destination,
				 gCall[zCall].dlist[i].outboundChannel,
				 gCall[zCall].dlist[i].resultCode,
				 gCall[zCall].dlist[i].resultData);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "gCall[%d].dlist[%d].destination=(%s,%d,%d,%s) ",
				   zCall, i, gCall[zCall].dlist[i].destination,
				   gCall[zCall].dlist[i].outboundChannel,
				   gCall[zCall].dlist[i].resultCode,
				   gCall[zCall].dlist[i].resultData);
	}

	fclose (fp);
	sprintf (zResultFile, "%s", fileName);

	return (0);
}								// END: createBlastDialResultFile()

int
writeDisconnectToBLeg (int zCall)
{
	char            mod[] = "writeDisconnectToBLeg";
	int             yRc = 0;
	struct MsgToDM  msgDisconnect;

	msgDisconnect.appCallNum = zCall;
	msgDisconnect.opcode = DMOP_B_LEG_DISCONNECTED;
	msgDisconnect.appPid = gCall[zCall].msgToDM.appPid;
	yRc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msgDisconnect), mod);
	return(0);
}

///This is the function we use to bridge calls and is fired whenever we see a DMOP_BRIDGECALL.
void           *
bridgeCallThread (void *z)
{
	char            mod[] = { "bridgeCallThread" };
	struct MsgToDM *ptrMsgToDM;
	struct Msg_InitiateCall msg;
	struct Msg_BridgeThirdLeg msgThirdLeg;
	struct MsgToApp response;
	int             yRc;
	int             zCall;
	int             zOutboundCall;
	int             popCrossPort;
	int             waitCount = 0;
	int             ringCountIn100msUnits;
	int             rtpDetailsSent = 0;
	char            message[MAX_LOG_MSG_LENGTH];
	time_t          yCurrentTime, yConnectTime;
	char            yStrDestination[500];
	char            yStrFromUrl[256];
	char            yStrLocalSdpPort[10];
	char            yStrPayloadNumber[10];
	char            yStrPayloadMime[255];
	char            yStrRemoteIPAddress[255];
// MR 4655
	struct Msg_InitiateCall msgCallIssued;
// END: MR 4655

	osip_message_t *invite = NULL;
	osip_message_t *send_ack = NULL;
	osip_message_t *answer = NULL;

	time (&yCurrentTime);
	time (&yConnectTime);

	msg = *(struct Msg_InitiateCall *) z;

	zCall = msg.appCallNum;

	memcpy (&msgThirdLeg, &(gCall[zCall].msgToDM),
			sizeof (struct Msg_BridgeThirdLeg));

// MR 4655 
	memcpy (&msgCallIssued, &msg, sizeof (struct Msg_InitiateCall));
	msgCallIssued.opcode = DMOP_CALL_INITIATED;
	msgCallIssued.informat = 1;   // Call is in initiated state
// END: MR 4655

	if (msgThirdLeg.opcode == DMOP_BRIDGE_THIRD_LEG)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "C-LEG leg[a]=%d leg[b]=%d leg[c]=%d",
				   msgThirdLeg.appCallNum1, msgThirdLeg.appCallNum2,
				   msgThirdLeg.appCallNum3);

		msg.informat = msgThirdLeg.informat;
	}

	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	zOutboundCall = getFreeCallNum (OUTBOUND);
// MR 4655 
		yRc = notifyMediaMgr (__LINE__, zOutboundCall, (struct MsgToDM *) &msgCallIssued, mod);
// END: MR 4655

	gCall[zCall].outboundRetCode = 0;

	if (zOutboundCall == -1)
	{
		response.returnCode = -1;
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return (NULL);
	}

	if (msg.opcode != DMOP_BRIDGE_THIRD_LEG)
	{
		gCall[zOutboundCall].parent_idx = zCall;
		gCall[zCall].leg = LEG_A;
		gCall[zOutboundCall].leg = LEG_B;
	}
	else
	{
		gCall[zOutboundCall].parent_idx = zCall;
		gCall[zOutboundCall].leg = LEG_B;
	}

	gCall[zOutboundCall].bridgeDestination[0] = 0;

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"B_LEG port is %d.", zOutboundCall);

#if 0

	if (gCall[zCall].callState != CALL_STATE_CALL_ACK)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
				   "Sending SIP Message 180 Ringing.");

		eXosip_lock (geXcontext);
		yRc = eXosip_call_build_answer (geXcontext, gCall[zCall].tid, 180, &answer);
		if (yRc == 0)
		{
			arc_fixup_contact_header (answer, gHostIp, gSipPort, zCall);
			yRc = eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 180, answer);
		}
		eXosip_unlock (geXcontext);
	}

#endif

	memcpy (&(gCall[zOutboundCall].msgToDM), &(gCall[zCall].msgToDM),
			sizeof (struct MsgToApp));

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "appPid=%d appRef=%d",
			   gCall[zCall].msgToDM.appPid, gCall[zCall].msgToDM.appRef);

	if (gCall[zCall].GV_SipAuthenticationUser[0])
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationUser, "%s",
				 gCall[zCall].GV_SipAuthenticationUser);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationUser, "%s",
				 gAuthUser);
	}

	if (gCall[zCall].GV_SipAuthenticationPassword[0])
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationPassword, "%s",
				 gCall[zCall].GV_SipAuthenticationPassword);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationPassword, "%s",
				 gAuthPassword);
	}

	if (gCall[zCall].GV_SipAuthenticationId[0])
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationId, "%s",
				 gCall[zCall].GV_SipAuthenticationId);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationId, "%s", gAuthId);
	}

	if (gCall[zCall].GV_SipAuthenticationRealm[0])
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationRealm, "%s",
				 gCall[zCall].GV_SipAuthenticationRealm);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipAuthenticationRealm, "%s",
				 gAuthRealm);
	}

	if (gCall[zCall].GV_SipFrom[0])			// BT-37
	{
		sprintf (gCall[zOutboundCall].GV_SipFrom, "%s",
				 gCall[zCall].GV_SipFrom);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_SipFrom, "%s", gFromUrl);
	}

	if (gCall[zCall].GV_DefaultGateway[0])
	{
		sprintf (gCall[zOutboundCall].GV_DefaultGateway, "%s",
				 gCall[zCall].GV_DefaultGateway);
	}
	else
	{
		sprintf (gCall[zOutboundCall].GV_DefaultGateway, "%s",
				 gDefaultGateway);
	}

	gCallSetCrossRef (gCall, MAX_PORTS, zCall, &gCall[zOutboundCall],
					  zOutboundCall);
	popCrossPort = gCall[zCall].crossPort;
	gCall[zCall].crossPort = zOutboundCall;

	// gCall[zCall].leg = LEG_A;

	gCall[zOutboundCall].crossPort = zCall;
	gCall[zOutboundCall].canKillApp = NO;

	gCallDumpCrossRefs (gCall, zOutboundCall);
	gCallDumpCrossRefs (gCall, zCall);

	yConnectTime = yConnectTime + 2 + msg.numRings * 6;
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Waiting for call to be connected for at least %d seconds. [ numRings=%d; connectTime=%d ]",
			   yConnectTime - yCurrentTime, msg.numRings, yConnectTime);

	gCall[zOutboundCall].remoteRtpPort = -1;

	char            yStrTmpIPAddress[256];

	char           *pch = NULL;	//strchr (msg.ipAddress, '@');

    char            ipAddress[512];
    char            phoneNumber[512];

	if (strstr (msg.ipAddress, "FILE:") != 0)
	{
		if ( (yRc = getParmFromFile(zCall, "ipAddress", msg.ipAddress, ipAddress)) != 0 )
		{
			response.returnCode = -1;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			// eXosip_unlock (geXcontext);			// MR4921 - extranious eXosip_unlock()
			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}
	}
	else
	{
		sprintf(ipAddress, "%s", msg.ipAddress);
	}

	if (strstr (msg.phoneNumber, "FILE:") != 0)
	{
		if ( (yRc = getParmFromFile(zCall, "phoneNumber", msg.phoneNumber, phoneNumber)) != 0 )
		{
			response.returnCode = -1;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			// eXosip_unlock (geXcontext);			// MR4921 - extranious eXosip_unlock()
			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}
	}
	else
	{
		sprintf(phoneNumber, "%s", msg.phoneNumber);
	}


	sprintf (yStrTmpIPAddress, "%s", ipAddress);
	pch = strstr (yStrTmpIPAddress, "@");

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "yStrTmpIPAddress=%s informat=%d, NONE=%d", yStrTmpIPAddress,
			   msg.informat, NONE);

	if (strstr (yStrTmpIPAddress, "sip:")
		|| strstr (yStrTmpIPAddress, "SIP:"))
	{
		if (pch == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, 3278, INFO,
					   "yStrDestination=%s", yStrDestination);
			if (msg.informat == NONE)
			{
				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "%s@[%s];user=phone",
							 phoneNumber,
							 gCall[zOutboundCall].GV_DefaultGateway);
					sprintf (yStrRemoteIPAddress, "[%s]",
							 gCall[zOutboundCall].GV_DefaultGateway);
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "yStrDestination=%s", yStrDestination);
				}
				else
				{
					sprintf (yStrDestination, "%s@%s;user=phone",
							 phoneNumber,
							 gCall[zOutboundCall].GV_DefaultGateway);
					sprintf (yStrRemoteIPAddress, "%s",
							 gCall[zOutboundCall].GV_DefaultGateway);
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "yStrDestination=%s", yStrDestination);
				}
			}
			else if (msg.informat != E_164)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE,
						   INFO, "P-Asserted-Identity IP = %s",
						   gCall[zCall].pAIdentity_ip);
				if (gCall[zCall].pAIdentity_ip
					&& gCall[zCall].pAIdentity_ip[0])
				{
					sprintf (gCall[zCall].remoteRtpIp, "%s",
							 gCall[zCall].pAIdentity_ip);
				}

				if (gCall[zCall].remoteRtpIp && gCall[zCall].remoteRtpIp[0])
				{
	int             yIntRemoteRtpIpLen = strlen (gCall[zCall].remoteRtpIp);

					if (gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] ==
						'\r'
						|| gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] ==
						'\n')
					{
						gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] = 0;
					}
				}

				//sprintf (yStrDestination, "%s@%s;user=phone", phoneNumber, gCall[zCall].remoteRtpIp);
				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "[%s];user=phone",
							 yStrTmpIPAddress);
					sscanf (yStrTmpIPAddress, "sip:[%s]",
							yStrRemoteIPAddress);

	char           *yTmpChr = NULL;

					if ((yTmpChr = strchr (yStrTmpIPAddress, ']')) != NULL)
					{
						yTmpChr[0] = 0;
					}
				}
				else
				{
					sprintf (yStrDestination, "%s;user=phone",
							 yStrTmpIPAddress);
					sscanf (yStrTmpIPAddress, "sip:%s", yStrRemoteIPAddress);
				}
			}
		}
		else
		{
	char           *tempChar = NULL;

			if (gEnableIPv6Support != 0)
			{
				tempChar = strstr (pch, ":]");

				sprintf (yStrDestination, "%s", ipAddress);
				if (tempChar == NULL)
				{
	char           *ptr = NULL;

					if ((ptr = strchr (pch, ']')) != NULL)
					{
						*ptr = '\0';
					}
					sprintf (yStrRemoteIPAddress, "%s", pch + 2);
//fprintf(stderr, " %s(%d) %s\n", __func__, __LINE__, yStrRemoteIPAddress);
				}
				else
				{
	char           *ptr = NULL;

					if ((ptr = strchr (pch, ']')) != NULL)
					{
						*ptr = '\0';
					}
					snprintf (yStrRemoteIPAddress,
							  strlen (pch) - strlen (tempChar), "%s",
							  pch + 2);
//fprintf(stderr, " %s(%d) %s\n", __func__, __LINE__, yStrRemoteIPAddress);
//fprintf(stderr, " %s(%d) \n", __func__, __LINE__);
				}
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE,
						   INFO, "yStrDestination=%s", yStrDestination);

			}
			else
			{
				tempChar = strstr (pch, ":");

				sprintf (yStrDestination, "%s", ipAddress);
				if (tempChar == NULL)
				{
					sprintf (yStrRemoteIPAddress, "%s", pch + 1);
//fprintf(stderr, " %s(%d) %s\n", __func__, __LINE__, yStrRemoteIPAddress);
//fprintf(stderr, " %s(%d) \n", __func__, __LINE__);
				}
				else
				{
					snprintf (yStrRemoteIPAddress,
							  strlen (pch) - strlen (tempChar), "%s",
							  pch + 1);
//fprintf(stderr, " %s(%d) %s\n", __func__, __LINE__, yStrRemoteIPAddress);
//fprintf(stderr, " %s(%d) \n", __func__, __LINE__);
				}
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, 3278, INFO,
						   "yStrDestination=%s", yStrDestination);
			}					// end

		}
	}
	else
	{
		if (pch == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, 3278, INFO,
					   "yStrDestination=%s", yStrDestination);
			if (msg.informat == NONE)
			{
				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "sip:%s@[%s];user=phone",
							 phoneNumber,
							 gCall[zOutboundCall].GV_DefaultGateway);
					sprintf (yStrRemoteIPAddress, "[%s]",
							 gCall[zOutboundCall].GV_DefaultGateway);
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "yStrDestination=%s", yStrDestination);
				}
				else
				{
					sprintf (yStrDestination, "sip:%s@%s;user=phone",
							 phoneNumber,
							 gCall[zOutboundCall].GV_DefaultGateway);
					sprintf (yStrRemoteIPAddress, "%s",
							 gCall[zOutboundCall].GV_DefaultGateway);
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "yStrDestination=%s", yStrDestination);
				}
			}
			else if (msg.informat != E_164)
			{

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, 3278, INFO,
						   "P-Asserted-Identity IP = %s",
						   gCall[zCall].pAIdentity_ip);
				if (gCall[zCall].pAIdentity_ip
					&& gCall[zCall].pAIdentity_ip[0])
				{
					sprintf (gCall[zCall].remoteSipIp, "%s",
							 gCall[zCall].pAIdentity_ip);
				}

				if (gCall[zCall].remoteSipIp && gCall[zCall].remoteSipIp[0])
				{
	int             yIntRemoteRtpIpLen = strlen (gCall[zCall].remoteSipIp);

					if (gCall[zCall].remoteSipIp[yIntRemoteRtpIpLen - 1] ==
						'\r'
						|| gCall[zCall].remoteSipIp[yIntRemoteRtpIpLen - 1] ==
						'\n')
					{
						gCall[zCall].remoteSipIp[yIntRemoteRtpIpLen - 1] = 0;
					}
				}
				if (ipAddress != NULL || ipAddress[0] != '\0')
				{
					if (gEnableIPv6Support != 0)
					{
						sprintf (yStrDestination, "sip:%s@[%s];user=phone",
								 ipAddress, gCall[zCall].remoteSipIp);
					}
					else
					{
						sprintf (yStrDestination, "sip:%s@%s;user=phone",
								 ipAddress, gCall[zCall].remoteSipIp);
					}
				}
				else
				{
					if (gEnableIPv6Support != 0)
					{
						sprintf (yStrDestination, "sip:%s@[%s];user=phone",
								 phoneNumber, gCall[zCall].remoteSipIp);
					}
					else
					{
						sprintf (yStrDestination, "sip:%s@%s;user=phone",
								 phoneNumber, gCall[zCall].remoteSipIp);
					}
				}
				sprintf (yStrRemoteIPAddress, "%s", gCall[zCall].remoteSipIp);
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "yStrDestination=%s", yStrDestination);
			}
		}
		else
		{
	char           *tempChar = NULL;

			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrDestination, "sip:[%s]", ipAddress);
			}
			else
			{
				sprintf (yStrDestination, "sip:%s", ipAddress);
			}
			tempChar = strstr (pch, ":");

			if (tempChar == NULL)
			{
				sprintf (yStrRemoteIPAddress, "%s", pch + 1);
			}
			else
			{
				snprintf (yStrRemoteIPAddress,
						  strlen (pch) - strlen (tempChar), "%s", pch + 1);
			}
		}
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "yStrDestination=(%s)", yStrDestination);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
	   "gCall[%d].sipFromStr=(%s) gCall[%d].sipToStr=(%s) gCall[%d].callDirection=%d"
	   " gCall[%d].sipFromStr=(%s)  gCall[%d].sipToStr=(%s)  gCall[%d].callDirection=%d",
		zCall, gCall[zCall].sipFromStr, zCall, gCall[zCall].sipToStr, zCall, gCall[zCall].callDirection,
		zOutboundCall, gCall[zOutboundCall].sipFromStr, zOutboundCall, gCall[zOutboundCall].sipToStr, zOutboundCall, gCall[zOutboundCall].callDirection);

	if ( gCall[zCall].sipFromStr != NULL && gCall[zCall].sipFromStr[0] != '\0' )      // MR 4964 & MR 5012
	{
		sprintf(yStrFromUrl, "%s", gCall[zCall].sipFromStr);
		
		char *p;
		if ( (p=strstr(yStrFromUrl, "tag")) != NULL) // remove tag
		{
			char *q;
			if ( (q=strchr(p, ';')) == NULL )
			{
				p--;
				if ( *p == ';' )
				{
					*p = '\0';
				}
			}
			else
			{
				q++;
				sprintf(p, "%s", q);
			}
		}
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"'From' string set to (%s) from original INVITE", yStrFromUrl);
	}					// END - MR 4964

	// else if (gCall[zOutboundCall].GV_SipFrom[0])  // MR 5061 - change 'else fi' to if.
	if (gCall[zOutboundCall].GV_SipFrom[0])
	{
		if (gCall[zCall].GV_CallerName[0])
		{
			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrFromUrl, "\"%s\" <sip:[%s]>",
						 gCall[zCall].GV_CallerName, gCall[zCall].GV_SipFrom);
			}
			else
			{
				sprintf (yStrFromUrl, "\"%s\" <sip:%s>",
						 gCall[zCall].GV_CallerName, gCall[zCall].GV_SipFrom);
			}
		}
		else
		{
			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrFromUrl, "sip:[%s]",
						 gCall[zOutboundCall].GV_SipFrom);
			}
			else
			{
				sprintf (yStrFromUrl, "sip:%s",
						 gCall[zOutboundCall].GV_SipFrom);
			}
		}
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"$SIP_FROM is set.  Overwriting 'From' string with (%s)", yStrFromUrl);
	}
	else if (gCall[zOutboundCall].GV_SipAuthenticationUser[0])
	{
		if (gCall[zCall].GV_CallerName[0])
		{
			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrFromUrl, "\"%s\" <sip:%s@[%s]>",
						 gCall[zCall].GV_CallerName,
						 gCall[zOutboundCall].GV_SipAuthenticationUser,
						 gHostIp);
			}
			else
			{
				sprintf (yStrFromUrl, "\"%s\" <sip:%s@%s>",
						 gCall[zCall].GV_CallerName,
						 gCall[zOutboundCall].GV_SipAuthenticationUser,
						 gHostIp);
			}
		}
		else
		{
			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrFromUrl, "sip:%s@[%s]",
						 gCall[zOutboundCall].GV_SipAuthenticationUser,
						 gHostIp);
			}
			else
			{
				sprintf (yStrFromUrl, "sip:%s@%s",
						 gCall[zOutboundCall].GV_SipAuthenticationUser,
						 gHostIp);
			}
		}
	}
//	else
	else if ( yStrFromUrl == NULL || yStrFromUrl[0] == '\0' )	// BT-44
	{
		if (gCall[zCall].GV_CallerName[0])
		{
			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrFromUrl, "\"%s\" <sip:arc@[%s]>",
						 gCall[zCall].GV_CallerName, gHostIp);
			}
			else
			{
				sprintf (yStrFromUrl, "\"%s\" <sip:arc@%s>",
						 gCall[zCall].GV_CallerName, gHostIp);
			}
		}
		else
		{
			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrFromUrl, "sip:arc@[%s]", gHostIp);
			}
			else
			{
				sprintf (yStrFromUrl, "sip:arc@%s", gHostIp);
			}
		}
	}

//fprintf(stderr, " %s(%d) %s\n", __func__, __LINE__, yStrFromUrl);

	if (gCall[zCall].GV_PAssertedIdentity[0] != '\0' && (invite != NULL))
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Setting P-Asserted-Identity = %s",
				   gCall[zCall].GV_PAssertedIdentity);
		osip_message_set_header (invite, "P-Asserted-Identity",
								 gCall[zCall].GV_PAssertedIdentity);

	}

	if (gCall[zCall].GV_Diversion[0] != '\0')
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Setting Diversion = %s", gCall[zCall].GV_Diversion);
		osip_message_set_header (invite, "Diversion",
								 gCall[zCall].GV_Diversion);
	}

	if (gCall[zCall].GV_CallInfo[0] != '\0')
	{
		osip_message_set_header (invite, "Call-Info",
								 gCall[zCall].GV_CallInfo);
	}

	//
	// enum will plow over the previous mess 
	//
	if ((msg.informat == E_164) && gSipEnableEnum)
	{
	char           *enum_ip = NULL;
	char           *enum_uri = NULL;
	struct ares_naptr_list_t *list = NULL;

		enum_ip = enum_convert (phoneNumber, gSipEnumTLD);

		if (enum_ip)
		{
			enum_lookup (&list, enum_ip, 0);
			if (list)
			{
				enum_match (list, ENUM_SERVICE_SIP, phoneNumber,
							&enum_uri);
				if (enum_uri)
				{
					snprintf (yStrDestination, sizeof (yStrDestination), "%s",
							  enum_uri);
				}
			}
		}
		if (enum_ip)
			free (enum_ip);
		if (enum_uri)
			free (enum_uri);
	}
	// end enum lookups

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Bridge Call B_LEG ", yStrDestination,
				  zOutboundCall);
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "yStrFromUrl=(%s)", yStrFromUrl);

	eXosip_lock (geXcontext);
	arc_add_authentication_info (gCall[zOutboundCall].GV_SipAuthenticationUser, gCall[zOutboundCall].GV_SipAuthenticationId, gCall[zOutboundCall].GV_SipAuthenticationPassword, "",	/*Algorith,  default MD5 */
								 gCall[zOutboundCall].
								 GV_SipAuthenticationRealm);

	if (strstr (yStrDestination, "sip:") == NULL)
	{
		if (gEnableIPv6Support != 0)
		{
			sprintf (gCall[zOutboundCall].bridgeDestination, "[%s]",
					 yStrDestination);
		}
		else
		{
			sprintf (gCall[zOutboundCall].bridgeDestination, "%s",
					 yStrDestination);
		}
	}
	else
	{
		if (gEnableIPv6Support != 0)
		{
			sprintf (gCall[zOutboundCall].bridgeDestination + 4, "[%s]",
					 yStrDestination);
		}
		else
		{
			sprintf (gCall[zOutboundCall].bridgeDestination + 4, "%s",
					 yStrDestination);
		}
	}

	if (gOutboundProxyRequired)
	{
	char            yStrTmpProxy[256];

		if (gEnableIPv6Support != 0)
		{
			sprintf (yStrTmpProxy, "sip:[%s]", gOutboundProxy);
		}
		else
		{
			sprintf (yStrTmpProxy, "sip:%s", gOutboundProxy);
		}

		yRc = eXosip_call_build_initial_invite (geXcontext, &invite, yStrTmpProxy,
												yStrFromUrl,
												yStrDestination, "");

#define GLOBAL_CROSSING

#ifdef GLOBAL_CROSSING
		if (1 && (yRc == 0))
		{
	char            newCallId[256];

			sprintf (newCallId, "%s@%s", invite->call_id->number, gHostIp);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Changed call id from %s to %s.",
					   invite->call_id->number, newCallId);

			osip_call_id_free (invite->call_id);

			invite->call_id = NULL;

			yRc = osip_message_set_call_id (invite, (const char *) newCallId);
		}
#endif
	}
	else
	{
#if 1
//RG added to check if the remote ip is known
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "yStrRemoteIPAddress=(%s)", yStrRemoteIPAddress);

	int             rc = 0;

		if (gEnableIPv6Support == 0 && !gCall[zCall].pAIdentity_ip[0])
		{
			rc = lookup_host (yStrRemoteIPAddress);
		}

		if (rc == -1 && msg.informat != E_164)
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL,
						  "Failed to bridge the call, could not build initial invite",
						  "", zCall);

			response.returnCode = -54;
			gCall[zCall].outboundRetCode = 54;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			eXosip_unlock (geXcontext);

			gCall[zCall].crossPort = -1;
			gCall[zCall].leg = LEG_A;

			gCall[zOutboundCall].crossPort = -1;
			gCall[zOutboundCall].leg = LEG_A;

			setCallState (zOutboundCall, CALL_STATE_CALL_CLOSED);

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);

		}
#endif

//fprintf(stderr, " %s(%d) yStrDestination=%s yStrFromUrl=%s \n", __func__, __LINE__, yStrDestination, yStrFromUrl);

		yRc = eXosip_call_build_initial_invite (geXcontext, &invite,
												yStrDestination,
												yStrFromUrl,
												gCall[zCall].GV_RouteHeader,
												"");

#ifdef GLOBAL_CROSSING
		if ((yRc == 0) && 1)	/* the if(1) was there already just checking the return code */
		{
	char            newCallId[256];

			sprintf (newCallId, "%s@%s", invite->call_id->number, gHostIp);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Changed call id from %s to %s.",
					   invite->call_id->number, newCallId);

			osip_call_id_free (invite->call_id);

			invite->call_id = NULL;

			yRc = osip_message_set_call_id (invite, (const char *) newCallId);
		}
#endif
	}

	if (yRc < 0)
	{

		__DDNDEBUG__ (DEBUG_MODULE_CALL,
					  "Failed to bridge the call, could not build initial invite",
					  "", zCall);

		response.returnCode = -1;
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);
		eXosip_unlock (geXcontext);

		if (msg.opcode == DMOP_BRIDGE_THIRD_LEG)
		{
			gCall[zCall].crossPort = popCrossPort;	//Put the original B Leg back
		}
		else
		{
			gCall[zCall].crossPort = -1;
		}

		gCall[zCall].leg = LEG_A;

		gCall[zOutboundCall].crossPort = -1;
		gCall[zOutboundCall].leg = LEG_A;

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return (NULL);
	}

	gCall[zCall].currentInvite = invite;

	sprintf (yStrLocalSdpPort, "%d",
			 LOCAL_STARTING_RTP_PORT + (zOutboundCall * 2));

	if (strstr (gCall[zCall].call_type, "Video"))
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Calling createBridgeSdpBody, crossPort=%d",
				   zCall[gCall].crossPort);

		createBridgeSdpBody (zOutboundCall, yStrLocalSdpPort,
							 //gCall[zCall].codecType, 
							 -1, gCall[zCall].telephonePayloadType, zCall);
	}
	else
	{
		createSdpBody (zOutboundCall, yStrLocalSdpPort,
					   gCall[zCall].codecType, -2,
					   gCall[zCall].telephonePayloadType);
	}

	osip_message_set_body (invite, gCall[zOutboundCall].sdp_body_local,
						   strlen (gCall[zOutboundCall].sdp_body_local));
	osip_message_set_content_type (invite, "application/sdp");

	/*Customer specific implementation */
#define CUSTOMER_TRIGGER_FILE_GLOBAL_CROSSING ".gc.trigger"
	if (access (CUSTOMER_TRIGGER_FILE_GLOBAL_CROSSING, F_OK) == 0)
	{
		/*P-Charge-Info is present: If A Leg had P-Charge-Info header, add "To" from incoming invite as outgoing P-Charge-Info */
		//if(gCall[zCall].GV_PChargeInfo[0])
		if (1)
		{
	char            yStrIncomingFromUser[256] = "\0";
	char            yStrTmpNewFrom[256] = "\0";
	char            yStrTmpContact[256] = "\0";

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, 3278, INFO,
					   "P-Charge-Info: Value in incoming INVITE:%s. Value in outbound INVITE:%s.",
					   zCall[gCall].GV_PChargeInfo, gCall[zCall].sipToStr);

			/*Add P-Charge-Info header */
	osip_to_t      *ptrTmpTo = NULL;

			osip_to_init (&ptrTmpTo);
			osip_to_parse (ptrTmpTo, gCall[zCall].sipToStr);

			if (ptrTmpTo && ptrTmpTo->url && ptrTmpTo->url->username)
			{
	char            yStrTmpPChargeHeader[256];

				sprintf (yStrTmpPChargeHeader, "<sip:%s@%s:%d>",
						 ptrTmpTo->url->username, gHostIp, gSipPort);

				osip_message_set_header (invite, "P-Charge-Info",
										 yStrTmpPChargeHeader);

				osip_to_free (ptrTmpTo);
			}

			/*END: Add P-Charge-Info header */

			/*Modify From field */
	osip_from_t    *ptrTmpFrom = NULL;

			osip_from_init (&ptrTmpFrom);
			osip_from_parse (ptrTmpFrom, gCall[zCall].sipFromStr);

			if (ptrTmpFrom && ptrTmpFrom->url && ptrTmpFrom->url->username)
			{

#if 0
				sprintf (yStrTmpNewFrom, "sip:%s@%s",
						 ptrTmpFrom->url->username, invite->from->url->host);

				if (invite->from->url->port)
				{
					sprintf (yStrTmpNewFrom, "sip:%s@%s:%s",
							 ptrTmpFrom->url->username,
							 invite->from->url->host,
							 invite->from->url->port);
				}

				osip_from_free (invite->from);
				invite->from = NULL;
				osip_message_set_from (invite, yStrTmpNewFrom);

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, 3278, INFO,
						   "P-Charge-Info: New From:%s", yStrTmpNewFrom);
#endif

#if 1
				sprintf (yStrIncomingFromUser, "%s",
						 ptrTmpFrom->url->username);

				if (invite->from->url && invite->from->url->username)
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, 3278,
							   INFO,
							   "P-Charge-Info: Removing from username:%s to replace with:%s",
							   invite->from->url->username,
							   yStrIncomingFromUser);

					osip_free (invite->from->url->username);
					invite->from->url->username = NULL;
				}
				invite->from->url->username = strdup (yStrIncomingFromUser);
#endif

				osip_from_free (ptrTmpFrom);
				ptrTmpFrom = NULL;
			}
			/*END: Modify From field */

			/*Modify Existing Contacts and the username from original invite */
	osip_contact_t *con =
		(osip_contact_t *) osip_list_get (&invite->contacts, 0);

			if (con && con->url && con->url->host)
			{
				//osip_free(con->url->username);
				//con->url->username = NULL;

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, 3278, INFO,
						   "P-Charge-Info: Value of default contact in B-Leg INVITE:%s@%s:%s",
						   con->url->username, con->url->host,
						   con->url->port);

				if (gCall[zCall].sipContactUsername[0])
				{
					sprintf (yStrTmpContact, "sip:%s@%s:%s",
							 gCall[zCall].sipContactUsername, con->url->host,
							 con->url->port);

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, 3278,
							   INFO,
							   "P-Charge-Info: Value of new contact in B-Leg INVITE:%s",
							   yStrTmpContact);

					while (1)
					{
						if (osip_list_remove (&invite->contacts, 0) <= 0)
						{
							break;
						}
					}

					osip_message_set_contact (invite, yStrTmpContact);

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, 3278,
							   INFO,
							   "P-Charge-Info: Value of New contact :%s",
							   yStrTmpContact);
				}
				else
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, 3278,
							   INFO,
							   "P-Charge-Info: Value of inbound contact couldn't be retrieved since contact username was null.");
				}
			}
			/*END: Modify Existing Contacts and the username from original invite */

			/*END: P-Charge-Info is present: If A Leg had P-Charge-Info header, add "To" from incoming invite as outgoing P-Charge-Info */
		}
#if 0
		else
		{
	char            yStrTmpFrom[256];
	char            yStrTmpContact[256];

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, 3278, INFO,
					   "P-Charge-Info: Not present in incoming INVITE. Value in outbound INVITE:%s.",
					   gCall[zCall].sipToStr);

			/*Add P-Charge-Info header */
			osip_message_set_header (invite, "P-Charge-Info",
									 gCall[zCall].sipToStr);

			/*Remove existing "from" and put the one from original invite */
			if (invite->from)
			{
				osip_from_free (invite->from);
				invite->from = NULL;
			}

			if (gCall[zCall].sipFrom && gCall[zCall].sipFrom->url
				&& gCall[zCall].sipFrom->url->host)
			{
				sprintf (yStrTmpFrom, "\"Unavailable\" <sip:Restricted@%s>",
						 gCall[zCall].sipFrom->url->host);
			}
			else
			{
				sprintf (yStrTmpFrom, "\"Unavailable\" <sip:Restricted@%s>",
						 gHostIp);
			}

			//osip_message_set_from(invite,   gCall[zCall].sipFromStr);
			osip_message_set_from (invite, yStrTmpFrom);
			/*END: Remove existing "from" and put the one from original invite */

			/*Empty Existing Contacts and the one from original invite */
			while (1)
			{
				if (osip_list_remove (&invite->contacts, 0) <= 0)
				{
					break;
				}
			}

			/*Add contact from initial INVITE */

			if (gCall[zCall].sipContact && gCall[zCall].sipContact->url
				&& gCall[zCall].sipContact->url->host)
			{
				sprintf (yStrTmpContact,
						 "\"Unavailable\" <sip:Restricted@%s>",
						 gCall[zCall].sipContact->url->host);
			}
			else
			{
				sprintf (yStrTmpContact,
						 "\"Unavailable\" <sip:Restricted@%s>", gHostIp);
			}

			//osip_message_set_contact(invite, gCall[zCall].sipContactStr);
			osip_message_set_contact (invite, yStrTmpContact);
			/*END: Empty Existing Contacts and the one from original invite */
		}
#endif

	}							/*DDN: Customer specific implementation */

	// BT-92
	arc_fixup_contact_header_with_displayname (__LINE__, invite, gCall[zCall].GV_CallerName, gContactIp, gSipPort, zCall);
	//arc_fixup_contact_header (answer, gContactIp, gSipPort, zCall);

	// DDN:20111212
	arc_insert_global_headers (zCall, invite);
    if ( gCall[zCall].GV_CallingParty != NULL )                              // BT-37
    {
        if ( arc_add_callID (invite, gCall[zCall].GV_CallingParty, zCall) != 0 )     // MR 5093
        {
            dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
                   "Failed to set caller ID to (%s).", gCall[zCall].GV_CallingParty);
        }

    }

	yRc = eXosip_call_send_initial_invite (geXcontext, invite);
	gCall[zOutboundCall].cid = yRc;
	eXosip_unlock (geXcontext);

	if (yRc < 0)
	{

		__DDNDEBUG__ (DEBUG_MODULE_CALL,
					  "Failed to bridge the call, could not send initial invite",
					  "", zCall);

		response.returnCode = -1;
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);

		if (msg.opcode == DMOP_BRIDGE_THIRD_LEG)
		{
			gCall[zCall].crossPort = popCrossPort;	//Put the original B Leg back
		}
		else
		{
			gCall[zCall].crossPort = -1;
		}

		gCall[zCall].leg = LEG_A;

		gCall[zOutboundCall].crossPort = -1;
		gCall[zOutboundCall].leg = LEG_A;

		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return (NULL);
	}

	gCall[zOutboundCall].lastError = CALL_NO_ERROR;

	response.returnCode = 0;

	while (gCall[zOutboundCall].callState != CALL_STATE_CALL_ANSWERED ||
		   gCall[zOutboundCall].remoteRtpPort <= 0)
	{
		usleep (10 * 1000);

//      dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
//          "DJB: gCall[%d].callState=%d   yCurrentTime=%ld  yConnectTime=%ld",
//          zOutboundCall, gCall[zOutboundCall].callState,
//          yCurrentTime, yConnectTime);
#if 0
		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
				   "bridge port(%d) A Leg(%d) callState of A(%d) popCrossPort=%d callState of popCrossPort(%d).",
				   zOutboundCall, zCall, gCall[zCall].callState, popCrossPort,
				   gCall[popCrossPort].callState);
#endif

		// fprintf(stderr, " %s() call state == %d\n", __func__, gCall[zOutboundCall].callState);

		if (gCall[zOutboundCall].callState == CALL_STATE_CALL_REDIRECTED)
		{
			dynVarLog (__LINE__, zOutboundCall, mod, REPORT_DETAIL, TEL_BASE,
					   INFO, "Outbound Call has been redirected");

			response.returnCode =
				-1 * mapSipErrorToArcError (zOutboundCall, 300);
			sprintf (response.message, "%d:%s", 22, "300 Multiple Choice");

			sprintf (gCall[zCall].redirectedContacts, "%s",
					 gCall[zOutboundCall].redirectedContacts);

			writeGenericResponseToApp (__LINE__, zCall, &response, mod);

			if (msg.opcode == DMOP_BRIDGE_THIRD_LEG)
			{
				gCall[zCall].crossPort = popCrossPort;	//Put the original B Leg back
			}
			else
			{
				gCall[zCall].crossPort = -1;
			}

			gCall[zCall].leg = LEG_A;

			gCall[zOutboundCall].crossPort = -1;
			gCall[zOutboundCall].leg = LEG_A;

			gCall[zOutboundCall].cid = -1;
			setCallState (zOutboundCall, CALL_STATE_CALL_RELEASED);

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}

		time (&yCurrentTime);

		if (yCurrentTime >= yConnectTime ||
			gCall[zOutboundCall].callState == CALL_STATE_CALL_REQUESTFAILURE
			|| gCall[zCall].GV_CancelOutboundCall == YES)
		{
			if (gCall[zCall].GV_CancelOutboundCall == YES)
			{
				response.returnCode = -55;
				sprintf (response.message, "%d:%s", 55, "Call Cancelled");
			}
			else if (gCall[zOutboundCall].callState == CALL_STATE_CALL_RINGING
					 || gCall[zOutboundCall].callState ==
					 CALL_STATE_CALL_BRIDGED
					 || gCall[zOutboundCall].callSubState ==
					 CALL_STATE_CALL_MEDIACONNECTED)
			{
				response.returnCode = -50;
				sprintf (response.message, "%d:%s", 50, "Ring no answer");
			}
			else
			{
				response.returnCode = -52;
				sprintf (response.message, "%d:%s", 52, "No ring back");
			}

			//response.returnCode = -50;
			//sprintf (response.message, "%d:%s", 50, "Ring no answer");

	struct Msg_SetGlobal yMsgSetGlobal;

			yMsgSetGlobal.opcode = DMOP_SETGLOBAL;
			yMsgSetGlobal.appPid = gCall[zCall].msgToDM.appPid;
			yMsgSetGlobal.appRef = 99999;
			yMsgSetGlobal.appPassword = zCall;
			yMsgSetGlobal.appCallNum = zCall;

			sprintf (yMsgSetGlobal.name, "$INTERNAL_BRIDGE_RINGING");
			yMsgSetGlobal.value = 0;
			notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &yMsgSetGlobal, mod);

			if (gCall[zOutboundCall].callState ==
				CALL_STATE_CALL_REQUESTFAILURE)
			{

				gCall[zOutboundCall].outboundRetCode =
					gCall[zOutboundCall].eXosipStatusCode;
				gCall[zCall].outboundRetCode =
					gCall[zOutboundCall].eXosipStatusCode;

				response.returnCode =
					-1 * mapSipErrorToArcError (zCall,
												gCall[zOutboundCall].
												eXosipStatusCode);

				sprintf (response.message, "%d:%s",
						 mapSipErrorToArcError (zCall,
												gCall[zOutboundCall].
												eXosipStatusCode),
						 gCall[zOutboundCall].eXosipReasonPhrase);
			}
			else
			{
				gCall[zCall].outboundRetCode = 408;	//Request Timeout
				gCall[zOutboundCall].outboundRetCode = 408;
			}

			/*DDN: 06/13 MR3543 */
			if (msg.opcode == DMOP_BRIDGE_THIRD_LEG)
			{
				if ((gCall[zOutboundCall].remoteRtpPort > 0
					 || gCall[zOutboundCall].callState ==
					 CALL_STATE_CALL_RINGING
					 || gCall[zOutboundCall].callState ==
					 CALL_STATE_CALL_BRIDGED))
				{

	int             parent_idx;

					// write back to app here 
					//response.returnCode = 0;
					//writeGenericResponseToApp (__LINE__, zCall, &response, mod);

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "appPid=%d appRef=%d",
							   gCall[zCall].msgToDM.appPid,
							   gCall[zCall].msgToDM.appRef);

					CROSSREF_INDEX_TO_ZCALL (parent_idx, zCall, gDynMgrId,
											 SPAN_WIDTH);
					SendMediaMgrPortSwitch (parent_idx, zOutboundCall,
											popCrossPort, zCall, 0);

					SendMediaManagerPortDisconnect (zOutboundCall, 1);
					gCall[zOutboundCall].crossPort = -1;
					gCall[zOutboundCall].thirdParty = 0;
					setCallState (zOutboundCall, CALL_STATE_CALL_CLOSED);
					gCallDelCrossRef (gCall, MAX_PORTS, zCall, zOutboundCall);

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "Sending message to media manager for third party"
							   " appCalNum=%d appCalNum1=%d appCalNum2=%d appCalNum3=%d",
							   parent_idx, zCall, popCrossPort,
							   zOutboundCall);
				}

				writeToRespShm (__LINE__, mod, tran_tabl_ptr, zOutboundCall, APPL_API,
								0);
				writeToRespShm (__LINE__, mod, tran_tabl_ptr, zOutboundCall,
								APPL_STATUS, STATUS_IDLE);
				writeToRespShm (__LINE__, mod, tran_tabl_ptr, zOutboundCall, APPL_PID,
								getpid ());
				updateAppName (__LINE__, zOutboundCall, zOutboundCall);	//DDN 06 13 2011  

				gCall[zCall].thirdParty = 0;
				gCall[zOutboundCall].thirdParty = 0;
				gCall[popCrossPort].thirdParty = 0;

				gCall[zCall].leg = LEG_A;
				gCall[zOutboundCall].leg = LEG_A;
				gCall[popCrossPort].leg = LEG_B;
			}

			writeGenericResponseToApp (__LINE__, zCall, &response, mod);

			if (msg.opcode == DMOP_BRIDGE_THIRD_LEG)
			{
				gCall[zCall].crossPort = popCrossPort;	//Put the original B Leg back
			}
			else
			{
				gCall[zCall].crossPort = -1;
			}

			gCall[zCall].leg = LEG_A;

			gCall[zOutboundCall].crossPort = -1;
			gCall[zOutboundCall].leg = LEG_A;

			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
					   "Sending SIP Message call_terminate (603).");

			eXosip_lock (geXcontext);
			eXosip_call_terminate (geXcontext, gCall[zOutboundCall].cid,
								   gCall[zOutboundCall].did);
			time (&(gCall[zOutboundCall].lastReleaseTime));
			eXosip_unlock (geXcontext);

			gCall[zOutboundCall].cid = -1;

			if (msg.opcode != DMOP_BRIDGE_THIRD_LEG)
			{
				writeDisconnectToBLeg (zCall);
			}

			setCallState (zOutboundCall, CALL_STATE_CALL_CLOSED);
			//gCall[zOutboundCall].callState = CALL_STATE_CALL_CLOSED;

			__DDNDEBUG__ (DEBUG_MODULE_CALL,
						  "Failed to bridge the call, B_LEG did not answer",
						  "", zCall);

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}
		else if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED
				 || gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
				 || gCall[zCall].callState == CALL_STATE_CALL_TERMINATE_CALLED
				 || gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
					   TEL_CALLER_DISCONNECTED, WARN,
					   "Failed to bridge(%d) the call, A Leg(%d) already disconnected.",
					   zOutboundCall, zCall);
			setCallSubState (zOutboundCall, CALL_STATE_CALL_CLOSED);

			if (msg.opcode == DMOP_BRIDGE_THIRD_LEG)
			{
				gCall[zCall].crossPort = popCrossPort;	//Put the original B Leg back
			}
			else
			{
				gCall[zCall].crossPort = -1;
			}

			gCall[zCall].leg = LEG_A;

			gCall[zOutboundCall].crossPort = -1;
			gCall[zOutboundCall].leg = LEG_A;

	struct Msg_SetGlobal yMsgSetGlobal;

			yMsgSetGlobal.opcode = DMOP_SETGLOBAL;
			yMsgSetGlobal.appPid = gCall[zCall].msgToDM.appPid;
			yMsgSetGlobal.appRef = 99999;
			yMsgSetGlobal.appPassword = zCall;
			yMsgSetGlobal.appCallNum = zCall;

			sprintf (yMsgSetGlobal.name, "$INTERNAL_BRIDGE_RINGING");
			yMsgSetGlobal.value = 0;
			notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &yMsgSetGlobal, mod);

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Failed to bridge the call",
						  "A Leg already disconnected.", zOutboundCall);

#if 1
			eXosip_lock (geXcontext);
			eXosip_call_terminate (geXcontext, gCall[zOutboundCall].cid,
								   gCall[zOutboundCall].did);
			time (&(gCall[zOutboundCall].lastReleaseTime));
			eXosip_unlock (geXcontext);
#endif

			gCall[zOutboundCall].cid = -1;

			if (msg.opcode != DMOP_BRIDGE_THIRD_LEG)
			{
				writeDisconnectToBLeg (zCall);
			}

			setCallState (zOutboundCall, CALL_STATE_CALL_CLOSED);
			//gCall[zOutboundCall].callState = CALL_STATE_CALL_CLOSED;

			pthread_detach (pthread_self ());
			pthread_exit (NULL);

			return (NULL);
		}
		else if (gCall[zOutboundCall].callState == CALL_STATE_CALL_RINGING)
		{
			if (gEarlyMediaSupported == 1 &&
				gCall[zOutboundCall].remoteRtpPort > 0 && !rtpDetailsSent)
			{
	struct MsgToDM  msgRtpDetails;

				/*Send Pre Initiate Message */
	struct Msg_InitiateCall yTmpMsgInitiate;
				memcpy (&yTmpMsgInitiate, &(msg),
						sizeof (struct Msg_InitiateCall));
				yTmpMsgInitiate.appCallNum2 = zOutboundCall;
				yTmpMsgInitiate.appCallNum1 = zCall;
				yTmpMsgInitiate.appCallNum = zCall;
				yTmpMsgInitiate.opcode = DMOP_MEDIACONNECT;
				yTmpMsgInitiate.appPid = msg.appPid;
				yRc =
					notifyMediaMgr (__LINE__, zCall,
									(struct MsgToDM *) &(yTmpMsgInitiate),
									mod);
				/*END: Send Pre Initiate Message */

				memcpy (&msgRtpDetails, &(gCall[zOutboundCall].msgToDM),
						sizeof (struct MsgToDM));
				gCall[zOutboundCall].codecType = gCall[zCall].codecType;
				gCall[zOutboundCall].full_duplex = gCall[zCall].full_duplex;
				gCall[zOutboundCall].telephonePayloadType =
					gCall[zCall].telephonePayloadType;
				msgRtpDetails.opcode = DMOP_RTPDETAILS;
				sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
						 gCall[zOutboundCall].leg,
						 gCall[zOutboundCall].remoteRtpPort,
						 gCall[zOutboundCall].localSdpPort,
						 (gCall[zOutboundCall].
						  telephonePayloadPresent) ? gCall[zOutboundCall].
						 telephonePayloadType : -99,
						 gCall[zOutboundCall].full_duplex,
						 gCall[zOutboundCall].codecType,
						 gCall[zOutboundCall].remoteRtpIp);
				yRc =
					notifyMediaMgr (__LINE__, zOutboundCall,
									(struct MsgToDM *) &(msgRtpDetails), mod);
				rtpDetailsSent = 1;
				setCallSubState (zOutboundCall,
								 CALL_STATE_CALL_MEDIACONNECTED);
				setCallSubState (zCall, CALL_STATE_CALL_MEDIACONNECTED);
			}
			if (msg.opcode == DMOP_BRIDGE_THIRD_LEG)
			{
	int             parent_idx;

				// write back to app here 
				//response.returnCode = 0;
				//writeGenericResponseToApp (__LINE__, zCall, &response, mod);
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "appPid=%d appRef=%d",
						   gCall[zCall].msgToDM.appPid,
						   gCall[zCall].msgToDM.appRef);

				setCallState (zOutboundCall, CALL_STATE_CALL_BRIDGED);
				CROSSREF_INDEX_TO_ZCALL (parent_idx, zCall, gDynMgrId,
										 SPAN_WIDTH);
				SendMediaMgrPortSwitch (parent_idx, zCall, zOutboundCall,
										popCrossPort, 0);

				gCall[zCall].thirdParty = 1;
				gCall[zOutboundCall].thirdParty = 1;
				gCall[popCrossPort].thirdParty = 1;

				gCall[zCall].leg = LEG_A;
				gCall[zOutboundCall].leg = LEG_B;
				gCall[popCrossPort].leg = LEG_B;

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "Sending message to media manager for third party"
						   " appCalNum=%d appCalNum1=%d appCalNum2=%d appCalNum3=%d",
						   parent_idx, zCall, zOutboundCall, popCrossPort);
			}
		}
#if 1
		else if (msg.opcode == DMOP_BRIDGE_THIRD_LEG)
		{
			if (gCall[popCrossPort].callState == CALL_STATE_CALL_CLOSED
				|| gCall[popCrossPort].callState == CALL_STATE_CALL_CANCELLED
				|| gCall[popCrossPort].callState ==
				CALL_STATE_CALL_TERMINATE_CALLED
				|| gCall[popCrossPort].callState == CALL_STATE_CALL_RELEASED)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
						   TEL_CALLER_DISCONNECTED, WARN,
						   "Failed to bridge(%d) the call, B Leg(%d) already disconnected.",
						   zOutboundCall, popCrossPort);

				setCallSubState (zOutboundCall, CALL_STATE_CALL_CLOSED);

				gCall[popCrossPort].leg = LEG_A;

				gCall[zOutboundCall].crossPort = -1;
				gCall[zOutboundCall].leg = LEG_A;

	struct Msg_SetGlobal yMsgSetGlobal;

				yMsgSetGlobal.opcode = DMOP_SETGLOBAL;
				yMsgSetGlobal.appPid = gCall[zCall].msgToDM.appPid;
				yMsgSetGlobal.appRef = 99999;
				yMsgSetGlobal.appPassword = zCall;
				yMsgSetGlobal.appCallNum = zCall;

				sprintf (yMsgSetGlobal.name, "$INTERNAL_BRIDGE_RINGING");
				yMsgSetGlobal.value = 0;
				notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &yMsgSetGlobal,
								mod);

				__DDNDEBUG__ (DEBUG_MODULE_CALL, "Failed to bridge the call",
							  "B Leg already disconnected.", zOutboundCall);

#if 1
				eXosip_lock (geXcontext);
				eXosip_call_terminate (geXcontext, gCall[zOutboundCall].cid,
									   gCall[zOutboundCall].did);
				time (&(gCall[zOutboundCall].lastReleaseTime));
				eXosip_unlock (geXcontext);
#endif

				gCall[zOutboundCall].cid = -1;

				if ((gCall[zOutboundCall].remoteRtpPort > 0
					 || gCall[zOutboundCall].callState ==
					 CALL_STATE_CALL_RINGING
					 || gCall[zOutboundCall].callState ==
					 CALL_STATE_CALL_BRIDGED))
				{

	int             parent_idx;

					// write back to app here 
					//response.returnCode = 0;
					//writeGenericResponseToApp (__LINE__, zCall, &response, mod);

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "appPid=%d appRef=%d",
							   gCall[zCall].msgToDM.appPid,
							   gCall[zCall].msgToDM.appRef);

					CROSSREF_INDEX_TO_ZCALL (parent_idx, zCall, gDynMgrId,
											 SPAN_WIDTH);
					//SendMediaMgrPortSwitch(parent_idx, zOutboundCall, popCrossPort, zCall);

					SendMediaManagerPortDisconnect (zOutboundCall, 1);
					gCall[zOutboundCall].crossPort = -1;
					gCall[zOutboundCall].thirdParty = 0;
					setCallState (zOutboundCall, CALL_STATE_CALL_CLOSED);
					gCallDelCrossRef (gCall, MAX_PORTS, zCall, zOutboundCall);

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "Sending message to media manager for third party"
							   " appCalNum=%d appCalNum1=%d appCalNum2=%d appCalNum3=%d",
							   parent_idx, zCall, popCrossPort,
							   zOutboundCall);
				}

				sleep (2);

				writeToRespShm (__LINE__, mod, tran_tabl_ptr, zOutboundCall, APPL_API,
								0);
				writeToRespShm (__LINE__, mod, tran_tabl_ptr, zOutboundCall,
								APPL_STATUS, STATUS_IDLE);
				writeToRespShm (__LINE__, mod, tran_tabl_ptr, zOutboundCall, APPL_PID,
								getpid ());
				updateAppName (__LINE__, zOutboundCall, zOutboundCall);	//DDN 06 13 2011  

				gCall[zCall].thirdParty = 0;
				gCall[zOutboundCall].thirdParty = 0;

				gCall[zCall].leg = LEG_A;
				gCall[zOutboundCall].leg = LEG_A;

				setCallState (zOutboundCall, CALL_STATE_CALL_CLOSED);

				response.appCallNum = popCrossPort;
				response.returnCode = -3;
				response.appRef = gCall[zCall].msgToDM.appRef - 1;
				response.opcode = DMOP_DISCONNECT;
				writeGenericResponseToApp (__LINE__, zCall, &response, mod);

				pthread_detach (pthread_self ());
				pthread_exit (NULL);

				return (NULL);
			}
		}
#endif

		if (gCall[zCall].callState != CALL_STATE_CALL_ACK
			&& gCall[zCall].isCallAnswered == NO)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Sending SIP Message 180 Ringing.");

			eXosip_lock (geXcontext);
			yRc = eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 180, NULL);
			eXosip_unlock (geXcontext);
		}

	}							/*END: while */

	eXosip_lock (geXcontext);

	eXosip_call_build_ack (geXcontext, gCall[zOutboundCall].did, &send_ack);
	if (send_ack != NULL)
	{
		arc_fixup_sip_request_line (send_ack, "", 0, zCall);
		arc_fixup_contact_header (send_ack, gContactIp, gSipPort, zCall);
		eXosip_call_send_ack (geXcontext, gCall[zOutboundCall].did, send_ack);
	}
	eXosip_unlock (geXcontext);

	gCall[zOutboundCall].appPid = msg.appPid;

	gCall[zCall].appPid = msg.appPid;

	if (msg.opcode == DMOP_BRIDGE_THIRD_LEG)
	{
	int             parent_idx;

		// write back to app here 
		//response.returnCode = 0;
		//writeGenericResponseToApp (__LINE__, zCall, &response, mod);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "appPid=%d appRef=%d",
				   gCall[zCall].msgToDM.appPid, gCall[zCall].msgToDM.appRef);

		setCallState (zOutboundCall, CALL_STATE_CALL_BRIDGED);
		CROSSREF_INDEX_TO_ZCALL (parent_idx, zCall, gDynMgrId, SPAN_WIDTH);
		SendMediaMgrPortSwitch (parent_idx, zCall, zOutboundCall,
								popCrossPort, 0);
		gCall[zCall].thirdParty = 1;
		gCall[zOutboundCall].thirdParty = 1;
		gCall[popCrossPort].thirdParty = 1;

		gCall[zCall].leg = LEG_A;
		gCall[zOutboundCall].leg = LEG_B;
		gCall[popCrossPort].leg = LEG_B;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Sending message to media manager for third party"
				   " appCalNum=%d appCalNum1=%d appCalNum2=%d appCalNum3=%d",
				   parent_idx, zCall, zOutboundCall, popCrossPort);

	}
	else
	{

		// normal bridge call 

		msg.appCallNum2 = zOutboundCall;
		msg.appCallNum1 = zCall;
		msg.appCallNum = zCall;
		yRc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msg), mod);

      dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
              "%d=notifyMediaMgr: opcode=%d appCallNum=%d "
              "appPid=%d appRef=%d appPassword=%d numRings=%d ipAddress=(%s) "
             "phoneNumber=(%s) whichOne=%d appCallNum1=%d appCallNum2=%d "
              "ani=(%s) informat=%d listenType=%d resourceType=(%s)",
              yRc, msg.opcode, msg.appCallNum, msg.appPid, msg.appRef, msg.appPassword,
              msg.numRings, ipAddress, phoneNumber, msg.whichOne, msg.appCallNum1,
              msg.appCallNum2, msg.ani, msg.informat, msg.listenType,  msg.resourceType);
	}

	sprintf (response.message, "%d", zOutboundCall);


#if 1 // DMOP_ BRIDGE CONNECTED 
    // writing DMOP_CALL_CONNECTED to application API, 
    // Joe S.  Wed Jul 15 05:39:12 EDT 2015
    int pop_opcode = response.opcode;

    response.opcode = DMOP_BRIDGE_CONNECTED;
    response.appCallNum = zOutboundCall;
    dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
               "Writing DMOP_CALL_CONNECTED response from BridgeCall.");
    writeGenericResponseToApp (__LINE__, zCall, &response, mod);

    response.opcode = pop_opcode;

#endif    // end DMOP_CALL_CONNECTED 


//  yRc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msg), mod);

	if (gCall[zOutboundCall].remoteRtpPort > 0 && !rtpDetailsSent)
	{

	struct MsgToDM  msgRtpDetails;

		memcpy (&msgRtpDetails, &(gCall[zOutboundCall].msgToDM),
				sizeof (struct MsgToDM));

		if (gCall[zOutboundCall].leg == LEG_B
			&& gCall[zOutboundCall].crossPort >= 0)
		{
	int             yALegPort = gCall[zOutboundCall].crossPort;

			gCall[zOutboundCall].codecType = gCall[yALegPort].codecType;

			gCall[zOutboundCall].full_duplex = gCall[yALegPort].full_duplex;

			gCall[zOutboundCall].telephonePayloadType =
				gCall[yALegPort].telephonePayloadType;
		}

		msgRtpDetails.opcode = DMOP_RTPDETAILS;

		sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
				 gCall[zOutboundCall].leg,
				 gCall[zOutboundCall].remoteRtpPort,
				 gCall[zOutboundCall].localSdpPort,
				 (gCall[zOutboundCall].
				  telephonePayloadPresent) ? gCall[zOutboundCall].
				 telephonePayloadType : -99, gCall[zOutboundCall].full_duplex,
				 gCall[zOutboundCall].codecType,
				 gCall[zOutboundCall].remoteRtpIp);

		yRc =
			notifyMediaMgr (__LINE__, zOutboundCall,
							(struct MsgToDM *) &(msgRtpDetails), mod);
		rtpDetailsSent = 1;
	}

	pthread_detach (pthread_self ());
	pthread_exit (NULL);
	return (NULL);

}								/*END: void * bridgeCallThread */

///This is the function we use to transfer calls and is fired whenever we see a DMOP_TRANSFERCALL.
void           *
transferCallThread (void *z)
{
	char            mod[] = { "transferCallThread" };
	struct MsgToDM *ptrMsgToDM;
	struct Msg_InitiateCall msg;
	struct MsgToApp response;
	struct MsgToDM  gDisconnectStruct = { DMOP_DISCONNECT, -1, -1, -1 };
	int             yRc;
	int             zCall;
	int             waitCount = 0;
	int             ringCountIn100msUnits;
	char            message[MAX_LOG_MSG_LENGTH];
	time_t          yCurrentTime, yConnectTime;
	//increased by Murali
	//char            yStrDestination[512] = "";
	char            yStrDestination[1024] = "";
	char            yStrFromUrl[512] = "";
	char            yStrLocalSdpPort[10] = "";
    char            yStrFromHost[128] = ""; // MR 4970

	// 
	osip_from_t    *replaces_from;
	osip_to_t      *replaces_to;

	osip_message_t *invite = NULL;

	char           *yStrBuiltMessage = NULL;
	size_t          yIntBuiltMessageLen = 0;

	char            referToSend[4056] = "";

	//By Murali
	//char			referTo[128];
	//char			referToUser[128] = "";
	char			referTo[1024];
	char			referToUser[1024] = "";

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "", 0);


	time (&yCurrentTime);
	time (&yConnectTime);

	msg = *(struct Msg_InitiateCall *) z;

	zCall = msg.appCallNum;
	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	gCall[zCall].outboundRetCode = 0;

	yConnectTime = yConnectTime + 2 + msg.numRings * 6;
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Waiting for call to be connected for at least %d seconds. [ numRings=%d; connectTime=%d ]",
			   yConnectTime - yCurrentTime, msg.numRings, yConnectTime);

	char            yStrTmpIPAddress[512];

	char           *pch;

	char			ipAddress[512];
	char			phoneNumber[512];

	char	yStrUserPhone[32];

	if( (gCall[zCall].remoteAgent & USER_AGENT_AVAYA_CM) ||
	    (gCall[zCall].remoteAgent & USER_AGENT_AVAYA_SM) )
	{
		sprintf(yStrUserPhone, "%s", "");
	}
	else
	{
		sprintf(yStrUserPhone, "%s", ";user=phone");
	}

	__DDNDEBUG__ (DEBUG_MODULE_CALL, msg.ipAddress, msg.phoneNumber, zCall);

	if (gCall[zCall].GV_DefaultGateway[0] == '\0')
	{
		sprintf (gCall[zCall].GV_DefaultGateway, "%s", gDefaultGateway);
	}

	if (gCall[zCall].remoteRtpIp && gCall[zCall].remoteRtpIp[0])
	{
	int             yIntRemoteRtpIpLen = strlen (gCall[zCall].remoteRtpIp);

		if (gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] == '\r' ||
			gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] == '\n')
		{
			gCall[zCall].remoteRtpIp[yIntRemoteRtpIpLen - 1] = 0;
		}

	}
	if (strstr (msg.ipAddress, "FILE:") != 0)
	{
		if ( (yRc = getParmFromFile(zCall, "ipAddress", msg.ipAddress, ipAddress)) != 0 )
		{
			response.returnCode = -1;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}
	}
	else
	{
		sprintf(ipAddress, "%s", msg.ipAddress);
	}

	if (strstr (msg.phoneNumber, "FILE:") != 0)
	{
		if ( (yRc = getParmFromFile(zCall, "phoneNumber", msg.phoneNumber, phoneNumber)) != 0 )
		{
			response.returnCode = -1;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}
	}
	else
	{
		sprintf(phoneNumber, "%s", msg.phoneNumber);
	}
    dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
               "ipAddress=(%s), msg.phoneNumber=(%s), numRings=%d, GV_DefaultGateway=(%s), msg.informat=%d, gDefaultGateway=(%s)",
               ipAddress, phoneNumber, msg.numRings, gCall[zCall].GV_DefaultGateway,
               msg.informat, gDefaultGateway);

	sprintf (yStrTmpIPAddress, "%s", ipAddress);
	pch = strstr (yStrTmpIPAddress, "@");
	if (strstr (yStrTmpIPAddress, "sip:"))
	{
		if (pch == NULL)
		{
			if (msg.informat == NONE)
			{

				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "%s@[%s]%s",
							 phoneNumber, gCall[zCall].GV_DefaultGateway, yStrUserPhone);
				}
				else
				{
					sprintf (yStrDestination, "%s@%s%s",
							 phoneNumber, gCall[zCall].GV_DefaultGateway, yStrUserPhone);
				}
			}
			else if (ipAddress != NULL && ipAddress[0] != '\0')
			{
				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "%s@[%s]%s",
							 phoneNumber, ipAddress, yStrUserPhone);
				}
				else
				{
					sprintf (yStrDestination, "%s@%s%s",
							 phoneNumber, ipAddress, yStrUserPhone);
				}
			}
			else
			{
				sprintf (yStrDestination, "%s%s", phoneNumber, yStrUserPhone);
			}
		}
		else
		{
			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrDestination, "[%s]", ipAddress);
			}
			else
			{
				sprintf (yStrDestination, "%s", ipAddress);
			}
		}
	}
	else
	{
		if (pch == NULL)
		{
			if (msg.informat == NONE)
			{
				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "sip:%s@[%s]%s",
							 phoneNumber, gCall[zCall].GV_DefaultGateway, yStrUserPhone);
				}
				else
				{
					sprintf (yStrDestination, "sip:%s@%s%s",
							 phoneNumber, gCall[zCall].GV_DefaultGateway, yStrUserPhone);
				}
			}
			else if (ipAddress != NULL || ipAddress[0] != '\0')
			{

				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "sip:%s@[%s%s",
							 ipAddress, gCall[zCall].remoteSipIp, yStrUserPhone);
				}
				else
				{
					sprintf (yStrDestination, "sip:%s@%s%s",
							 ipAddress, gCall[zCall].remoteSipIp, yStrUserPhone);
				}

			}
			else if (phoneNumber != NULL || phoneNumber[0] != '\0')
			{
				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "sip:%s@[%s]%s",
							 phoneNumber, ipAddress, yStrUserPhone);
				}
				else
				{
					sprintf (yStrDestination, "sip:%s@%s%s",
							 phoneNumber, ipAddress, yStrUserPhone);
				}
			}
			else
			{
				if (gEnableIPv6Support != 0)
				{
					sprintf (yStrDestination, "sip:[%s]%s",
							 phoneNumber, yStrUserPhone);
				}
				else
				{
					sprintf (yStrDestination, "sip:%s%s",
							 phoneNumber, yStrUserPhone);
				}
			}
		}
		else
		{
			if (gEnableIPv6Support != 0)
			{
				sprintf (yStrDestination, "sip:%s", ipAddress);
			}
			else
			{
				sprintf (yStrDestination, "sip:%s", ipAddress);
			}
		}
	}

	// 
	// this will overwrite all of the above if the informat is set to E_164
	//

	if ((msg.informat == E_164) && gSipEnableEnum)
	{

	char           *enum_ip = NULL;
	char           *enum_uri = NULL;
	struct ares_naptr_list_t *list = NULL;

		enum_ip = enum_convert (phoneNumber, gSipEnumTLD);

		if (enum_ip)
		{
			enum_lookup (&list, enum_ip, 0);
			if (list)
			{
				enum_match (list, ENUM_SERVICE_SIP, phoneNumber,
							&enum_uri);
				if (enum_uri)
				{
					snprintf (yStrDestination, sizeof (yStrDestination), "%s",
							  enum_uri);
				}
			}
		}

		if (enum_ip)
			free (enum_ip);
		if (enum_uri)
			free (enum_uri);
	}
	// end enum lookups

	// is this a consultive transfer ? 
	// if so add ?Replaces=bridged call id;to-tag=c-tag;from-tag=b-tag
	// 

	if (gCall[zCall].crossPort > -1
		&& (gCall[gCall[zCall].crossPort].callState ==
			CALL_STATE_CALL_ANSWERED))
	{

	char            temp[600];

//#define SONUS_XXX

#ifdef SONUS_XXX
	int             yTmpCrossPort = zCall;

		zCall = gCall[zCall].crossPort;
#else
	int             yTmpCrossPort = gCall[zCall].crossPort;
#endif

	char            yStrEscapedCallId[256];

		if (strchr (gCall[yTmpCrossPort].sipCallId, '@'))
		{
	char            yStrCallIdNumber[256];
	char            yStrCallIdHost[256];

			sscanf (gCall[yTmpCrossPort].sipCallId, "%[^@]@%s",
					yStrCallIdNumber, yStrCallIdHost);
			sprintf (yStrEscapedCallId, "%s%%40%s", yStrCallIdNumber,
					 yStrCallIdHost);

		}
		else
		{
			sprintf (yStrEscapedCallId, "%s", gCall[yTmpCrossPort].sipCallId);
		}

		if (strchr (yStrDestination, '?'))
		{
			snprintf (temp, sizeof (temp),
					  "%s;Replaces=%s%%3Bto-tag%%3D%s%%3Bfrom-tag%%3D%s",
					  yStrDestination, yStrEscapedCallId,
					  gCall[yTmpCrossPort].tagToValue,
					  gCall[yTmpCrossPort].tagFromValue);
		}
		else
		{
			snprintf (temp, sizeof (temp),
					  "%s?Replaces=%s%%3Bto-tag%%3D%s%%3Bfrom-tag%%3D%s",
					  yStrDestination, yStrEscapedCallId,
					  gCall[yTmpCrossPort].tagToValue,
					  gCall[yTmpCrossPort].tagFromValue);
		}

		snprintf (yStrDestination, sizeof (yStrDestination), "%s", temp);
	}

	// end consultive transfer 

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Transferring this call to",
				  yStrDestination, zCall);

    if ( gCall[zCall].GV_SipFrom[0] )   // MR 5093
    {
        if (gEnableIPv6Support != 0)
        {
            sprintf (yStrFromUrl, "sip:[%s]", gCall[zCall].GV_SipFrom);
        }
        else
        {
            sprintf (yStrFromUrl, "sip:%s", gCall[zCall].GV_SipFrom);
        }
    }
    else
    {
        if (gEnableIPv6Support != 0)
        {
            sprintf (yStrFromUrl, "sip:arc@[%s]", gHostIp);
        }
        else
        {
            sprintf (yStrFromUrl, "sip:arc@%s", gHostIp);
        }
    }

	osip_message_t *refer = NULL;

	if (gCall[zCall].transferAAI && gCall[zCall].transferAAI[0] != '\0')
	{
		if (strcasecmp (gTransferAAIEncoding, "hex") == 0)
		{
			char	yTmpTransferAAI[1024] = "\0";

			arc_convert_ascii_to_hex (yTmpTransferAAI,
									  gCall[zCall].transferAAI);

			if( (gCall[zCall].remoteAgent & USER_AGENT_AVAYA_CM) ||
			    (gCall[zCall].remoteAgent & USER_AGENT_AVAYA_SM) ||
			    (gCall[zCall].remoteAgent & USER_AGENT_AVAYA_XM) )
			{
				if( gTransferAAIPrefix[0] != '\0' )
				{
					sprintf (yStrDestination, "%s?%s=%s%s%%3Bencoding%%3Dhex",
						yStrDestination, gTransferAAIParamName, gTransferAAIPrefix, yTmpTransferAAI);

				}
				else
				{
					sprintf (yStrDestination, "%s?%s=%s%%3Bencoding%%3Dhex",
						yStrDestination, gTransferAAIParamName, yTmpTransferAAI);
				}
			}
			else
			{
				sprintf (yStrDestination, "%s?%s=%s;encoding=hex",
					 yStrDestination, gTransferAAIParamName, yTmpTransferAAI);
			}
		}
		else
		{
			sprintf (yStrDestination, "%s?%s=%s", yStrDestination,
					 gTransferAAIParamName, gCall[zCall].transferAAI);
		}

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, 3278, INFO,
				   "yStrDestination=(%s)", yStrDestination);
//fprintf(stderr, " %s(%d) yStrDestination=%s\n", __func__, __LINE__, yStrDestination);
	}

#if 1
	char            tempStrDest[600] = "";

	sprintf (tempStrDest, "%s", yStrDestination);
	memset (yStrDestination, 0, sizeof (yStrDestination));
	sprintf (yStrDestination, "<%s>", tempStrDest);
#endif

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "yStrDestination=(%s)", yStrDestination);

    /*
      Ready the state machine and fire a message 
      yRc 
    */

    // MR 4970
	if (  gCall[zCall].callDirection == INBOUND )
	{
		if ( gCall[zCall].sipFromStr[0] != '\0' )  			// MR 5012
		{
			sprintf(yStrFromHost, "%s", gCall[zCall].sipFromHost);
		}
	}

    if ( referToUser[0] == '\0' )
    {
        if ( strstr(yStrDestination, "sip:") != NULL )
        {
            char *p;
			//By Murali
            //char tmpStr[512];
            char tmpStr[1024];

            sprintf(tmpStr, "%s", yStrDestination);
            p = strstr(tmpStr, "sip:");
            p += 4;
            sprintf(referToUser, "%s", p);
            if ( (p = strchr(referToUser, '@')) != NULL )
            {
                *p = '\0';
            }
        }
    }

    if ( (gCall[zCall].GV_DefaultGateway[0] == '\0')  &&
	     (gCall[zCall].callDirection == INBOUND ) &&
         ( ( yStrFromHost[0] == '\0' ) ||
           ( strcmp(yStrFromHost, "anonymous.invalid") == 0 )  ||
           ( strstr(yStrFromHost, "invalid" ) != NULL )
         )
       )
    {
        sprintf(referTo, "%s@%s", referToUser, gCall[zCall].pAIdentity_ip);
        yRc = OutboundCallReferHandler(__LINE__, NULL, zCall, yStrDestination,  yStrFromUrl, OUTBOUND_REFER_OP_INIT, referTo);
    }
    else
    {
        yRc = OutboundCallReferHandler(__LINE__, NULL, zCall, yStrDestination,  yStrFromUrl, OUTBOUND_REFER_OP_INIT, NULL);
    }
    // END = MR 4970

	if (yRc != 0 ||
		gCall[zCall].callState == CALL_STATE_CALL_CLOSED ||
		gCall[zCall].callState == CALL_STATE_CALL_CANCELLED ||
		gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
	{
		response.returnCode = -3;
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);
		return (NULL);
	}

	if (yRc < 0)
	{
	//    fprintf(stderr, " %s() bailing at line=%d yRc=%d\n", __func__, __LINE__, yRc);
		response.returnCode = -1;
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);
		pthread_detach (pthread_self ());
		pthread_exit (NULL);
		return (NULL);
	}

	gCall[zCall].lastError = CALL_NO_ERROR;

	response.returnCode = 0;

	setCallState (zCall, CALL_STATE_CALL_TRANSFER_ISSUED);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "TransferCall: Waiting for call to be transfered for at least %d seconds.",
			   yConnectTime - yCurrentTime);

	while (gCall[zCall].callState != CALL_STATE_CALL_TRANSFERCOMPLETE &&
		   gCall[zCall].callState != CALL_STATE_CALL_TRANSFERCOMPLETE_CLOSED)
	{
		usleep (10 * 1000);

		// BT-171
		if(gCall[zCall].canExitTransferCallThread == 1)
		{
			break;
		}

		time (&yCurrentTime);

		if (yCurrentTime >= yConnectTime || gCall[zCall].callState == CALL_STATE_CALL_TRANSFERFAILURE)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						   "yCurrentTime(%d) >= yConnectTime(%d) || gCall[%d].callState = %d [ CALL_STATE_CALL_TRANSFERFAILURE (%d) ]",
							yCurrentTime, yConnectTime, zCall, gCall[zCall].callState, CALL_STATE_CALL_TRANSFERFAILURE);

			__DDNDEBUG__ (DEBUG_MODULE_CALL,
						  "yCurrentTime >= yConnectTime || CALL_STATE_CALL_TRANSFERFAILURE",
						  "", zCall);

			if (yCurrentTime >= yConnectTime)
			{

				if (gCall[zCall].callState == CALL_STATE_CALL_RINGING 
                    || gCall[zCall].callState == CALL_STATE_CALL_TRANSFER_RINGING
                   )
				{
					response.returnCode = -50;
					sprintf (response.message, "%d:%s", 50, "Ring no answer");
				}
				else
				{
					response.returnCode = -52;
					sprintf (response.message, "%d:%s", 52, "No ring back");
				}
			}
			else
			{
				response.returnCode = -50;
				sprintf (response.message, "%d:%s", 50, "Ring no answer");
			}

			if (gCall[zCall].callState == CALL_STATE_CALL_TRANSFERFAILURE)
			{
				gCall[zCall].outboundRetCode = gCall[zCall].eXosipStatusCode;

				response.returnCode =
					-1 * mapSipErrorToArcError (zCall,
												gCall[zCall].
												eXosipStatusCode);
				sprintf (response.message, "%d:%s",
						 mapSipErrorToArcError (zCall,
												gCall[zCall].
												eXosipStatusCode),
						 gCall[zCall].eXosipReasonPhrase);
			}
			else
			{
				gCall[zCall].outboundRetCode = 408;	//Request Timeout
			}

			writeGenericResponseToApp (__LINE__, zCall, &response, mod);

			/* Drop B Leg if it was Consultation transfer */
			if (gCall[zCall].crossPort > -1)
			{
	int             yBLegPort = gCall[zCall].crossPort;

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "Dropping B Leg (%d) of the consultation transfer.",
						   yBLegPort);

				eXosip_lock (geXcontext);
				eXosip_call_terminate (geXcontext, gCall[yBLegPort].cid,
									   gCall[yBLegPort].did);
				time (&(gCall[yBLegPort].lastReleaseTime));
				eXosip_unlock (geXcontext);

				gDisconnectStruct.appCallNum = yBLegPort;
				gDisconnectStruct.opcode = DMOP_DISCONNECT;
				gDisconnectStruct.appPid = gCall[yBLegPort].appPid;
				gDisconnectStruct.appPassword = gCall[yBLegPort].appPassword;

				yRc =
					notifyMediaMgr (__LINE__, yBLegPort,
									(struct MsgToDM *) &gDisconnectStruct,
									mod);

				gDisconnectStruct.appCallNum = -1;
				gDisconnectStruct.appPid = -1;
				gDisconnectStruct.appPassword = -1;

				clearPort (yBLegPort);

				writeToRespShm (__LINE__, mod, tran_tabl_ptr, yBLegPort, APPL_API, 0);
				writeToRespShm (__LINE__, mod, tran_tabl_ptr, yBLegPort, APPL_STATUS,
								STATUS_IDLE);
				writeToRespShm (__LINE__, mod, tran_tabl_ptr, yBLegPort, APPL_PID,
								getpid ());
				updateAppName (__LINE__, yBLegPort, yBLegPort);
			}

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}

	}							/*END: while */

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Cross port %d. [cs=%d]", gCall[zCall].crossPort, gCall[zCall].callState);

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "CALL_STATE", "",
				  gCall[zCall].callState);

	sleep (1);

	response.returnCode = 0;
	sprintf (response.message, "%s", "0:Success");
	writeGenericResponseToApp (__LINE__, zCall, &response, mod);

	if (gCall[zCall].callState == CALL_STATE_CALL_TRANSFERCOMPLETE_CLOSED)
	{
		setCallState (zCall, CALL_STATE_CALL_CLOSED);
		//gCall[zCall].callState = CALL_STATE_CALL_CLOSED;

		if (gCall[zCall].permanentlyReserved == 1)
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Changing the status to init",
						  "", zCall);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Setting port num=%d to permanentlyReserved", zCall);

			writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
			writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS,
							STATUS_INIT);
			//writeToRespShm(mod, tran_tabl_ptr, zCall, APPL_SIGNAL, 1);
		}
		else
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Changing the status to idle",
						  "", zCall);

			writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
			writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS,
							STATUS_IDLE);
			//writeToRespShm(mod, tran_tabl_ptr, zCall, APPL_SIGNAL, 1);
		}

		if (gCall[zCall].permanentlyReserved == 1)
		{
	       // struct timeb    yTmpTimeb;
			struct timeval	yTmpTimeb;

		//	ftime (&yTmpTimeb);
			yRc =
				addToTimerList (__LINE__, zCall, MP_OPCODE_STATIC_APP_GONE,
								gCall[zCall].msgToDM, yTmpTimeb, -1, -1, -1);
//fprintf(stderr, " %s() adding static app gone for zcall=%d\n", __func__, zCall);

			if (yRc < 0)
			{
				;
			}
		}
		else if (gCall[zCall].leg == LEG_A)
		{

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Registering to kill", "",
						  gCall[zCall].msgToDM.appPid);
	//struct timeb    yTmpTimeb;
	struct timeval	yTmpTimeb;

		//	ftime (&yTmpTimeb);
			gettimeofday(&yTmpTimeb, NULL);

			yTmpTimeb.tv_sec += gCall[zCall].GV_KillAppGracePeriod;

			yRc = addToTimerList (__LINE__, zCall,
								  MP_OPCODE_TERMINATE_APP,
								  gCall[zCall].msgToDM,
								  yTmpTimeb, -1, -1, -1);

#if 0
			if (yRc >= 0)
			{
				yTmpTimeb.time += 3;
				addToTimerList (__LINE__, zCall, MP_OPCODE_KILL_APP,
								gCall[zCall].msgToDM, yTmpTimeb, -1, -1, -1);
			}
#endif

			__DDNDEBUG__ (DEBUG_MODULE_MEMORY, "addToTimerList", "returned ",
						  yRc);
			if (yRc < 0)
			{
				;
			}
		}

		gDisconnectStruct.appCallNum = zCall;
		gDisconnectStruct.opcode = DMOP_DISCONNECT;
		gDisconnectStruct.appPid = gCall[zCall].appPid;
		gDisconnectStruct.appPassword = gCall[zCall].appPassword;

		yRc =
			notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &gDisconnectStruct,
							mod);

		gDisconnectStruct.appCallNum = -1;
		gDisconnectStruct.appPid = -1;
		gDisconnectStruct.appPassword = -1;

	}

	/* Drop B Leg if it was Consultation transfer */
	if (gCall[zCall].crossPort > -1)
	{
	int             yBLegPort = gCall[zCall].crossPort;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Dropping B Leg (%d) of the consultation transfer.",
				   yBLegPort);

		eXosip_lock (geXcontext);
		eXosip_call_terminate (geXcontext, gCall[yBLegPort].cid, gCall[yBLegPort].did);
		time (&(gCall[yBLegPort].lastReleaseTime));
		eXosip_unlock (geXcontext);

		gDisconnectStruct.appCallNum = yBLegPort;
		gDisconnectStruct.opcode = DMOP_DISCONNECT;
		gDisconnectStruct.appPid = gCall[yBLegPort].appPid;
		gDisconnectStruct.appPassword = gCall[yBLegPort].appPassword;

		yRc =
			notifyMediaMgr (__LINE__, yBLegPort, (struct MsgToDM *) &gDisconnectStruct,
							mod);

		gDisconnectStruct.appCallNum = -1;
		gDisconnectStruct.appPid = -1;
		gDisconnectStruct.appPassword = -1;

		clearPort (yBLegPort);

		writeToRespShm (__LINE__, mod, tran_tabl_ptr, yBLegPort, APPL_API, 0);
		writeToRespShm (__LINE__, mod, tran_tabl_ptr, yBLegPort, APPL_STATUS,
						STATUS_IDLE);
		writeToRespShm (__LINE__, mod, tran_tabl_ptr, yBLegPort, APPL_PID, getpid ());
		updateAppName (__LINE__, yBLegPort, yBLegPort);
	}

	pthread_detach (pthread_self ());
	pthread_exit (NULL);
	return (NULL);

}								/*END: void * transferCallThread */

///This is the function we use to RESACT certain ports and is fired whenever we see a DMOP_RESACT_STATUS.
int
process_DMOP_RESACT_STATUS (int zCall, struct MsgToDM *lpMsgToDM)
{
	int             rc;
	char            mod[] = { "process_DMOP_RESACT_STATUS" };
	char            tmpData[MAX_APP_MESSAGE_DATA * 4];
	char           *yStrTok = NULL;
	char           *tmpBuffer;
	char            lhs[4];
	char            rhs[2];
	int             port;
	int             value;
	char            yErrMsg[128];
	int             lgcErr = 0;
	int             lccErr = 0;
	FILE           *fp;
	char            tmpFName[256];

	memset ((char *) tmpData, '\0', sizeof (tmpData));

	if (strncmp (lpMsgToDM->data, "FILE=", 5) == 0)
	{
		sprintf (tmpFName, "%s", lpMsgToDM->data + 5);
		if ((fp = fopen (tmpFName, "r")) == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR,
					   "Failed to open file (%s), [%d, %s]  Unable to perform resource status.",
					   tmpFName, errno, strerror (errno));

			return (-1);
		}

		fscanf (fp, "%s", tmpData);

		fclose (fp);

		arc_unlink (zCall, mod, tmpFName);
	}
	else
	{
		sprintf (tmpData, "%s", lpMsgToDM->data);
	}

	if (tmpData[0] == 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_DATA, ERR,
				   "Failed to perform resource status. DMOP_RESACT_STATUS had no data in it.");
		return (-1);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "DMOP_RESACT_STATUS data (%s)", lpMsgToDM->data);

	tmpBuffer = (char *) strtok_r ((char *) tmpData, ",", &yStrTok);

	while ((tmpBuffer != NULL) && (tmpBuffer[0] != 0))
	{
		sprintf (lhs, "%s", "\0");
		sprintf (rhs, "%s", "\0");

		sscanf (tmpBuffer, "%[^=]=%s", lhs, rhs);
		port = atoi (lhs);
		value = atoi (rhs);

		if (value == 1)
		{

			if (gCall[zCall].permanentlyReserved == 1)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "Setting port num=%d to permanentlyReserved.",
						   zCall);

				writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
				writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS,
								STATUS_INIT);
			}
			else
			{
				writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
				writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS,
								STATUS_IDLE);
			}

			gCall[zCall].portState = PORT_STATE_ON;
		}
		else if (value == 0)
		{
			writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
			writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS,
							STATUS_OFF);
			gCall[zCall].portState = PORT_STATE_OFF;
		}

		tmpBuffer = (char *) strtok_r ((char *) NULL, ",", &yStrTok);
	}

	return (0);

}								/*END: int process_DMOP_RESACT_STATUS */

int
cb_udp_snd_message_external (int zCall, osip_message_t * sip, char *message)
{
#if 0
	char           *mod = { "cb_udp_snd_message_external" };
	int             len = 0;
	size_t          length = 0;
	static int      num = 0;
	struct addrinfo *addrinfo;
	struct __eXosip_sockaddr addr;
	int             i;
	char           *host = NULL;

	//char * host = sip->req_uri->host;
	int             port = 5060;
	char            yStrPort[10];
	char            yStrHost[256];
	char            yStrUsername[256];
	char            yStrUrlToParse[256];

	/*DDN Added 20100702 */
	osip_uri_t     *sipReferDest = NULL;

	length = strlen (message);

	/*DDN: Removed hardcoded port and hardcoded "GV_SipAsAddress + 5" */
	if (osip_uri_init (&sipReferDest) == -1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "MR2875: Failed to init uri structure.");

		return -1;
	}

	if (strstr (gCall[zCall].GV_SipAsAddress, "sip:") == NULL)
	{
		sprintf (yStrUrlToParse, "sip:%s", gCall[zCall].GV_SipAsAddress);
	}
	else
	{
		sprintf (yStrUrlToParse, "%s", gCall[zCall].GV_SipAsAddress);
	}
	if (osip_uri_parse (sipReferDest, yStrUrlToParse) == -1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "MR2875: Failed to parse uri for %s.",
				   gCall[zCall].GV_SipAsAddress);

		if (sipReferDest)
		{
			osip_uri_free (sipReferDest);
			sipReferDest = NULL;
		}

		return -1;
	}

	host = sipReferDest->host;

	if (sipReferDest->port)
	{
		sprintf (yStrPort, "%s", sipReferDest->port);
	}
	else
	{
		sprintf (yStrPort, "%s", "5060");
	}

	if (yStrPort && yStrPort[0])
	{
		port = atoi (yStrPort);
	}
	else
	{
		port = 5060;
	}

	if (port < 0)
	{
		port = 5060;
	}
	i = eXosip_get_addrinfo (geXcontext, &addrinfo, host, port, IPPROTO_UDP);
	if (i != 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Failed to get addr info for host(%s) and port (%d).",
				   host, port);

		if (sipReferDest)
		{
			osip_uri_free (sipReferDest);
			sipReferDest = NULL;
		}
		return -1;
	}

	memcpy (&addr, addrinfo->ai_addr, addrinfo->ai_addrlen);

	len = addrinfo->ai_addrlen;

	freeaddrinfo (addrinfo);

	if (sipReferDest)
	{
		osip_uri_free (sipReferDest);
		sipReferDest = NULL;
	}

#if 1
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Sending external REFER (%s).", message);
#endif

	if (0 > sendto (eXosip.j_socket, (const void *) message, length, 0,
					(struct sockaddr *) &addr, len /* sizeof(addr) */ ))
	{
		if (ECONNREFUSED == errno)
		{
			return 1;
		}
		else
		{
			/*
			 * SIP_NETWORK_ERROR;
			 */
			return -1;
		}
	}
#endif
	return 0;

}								/*END: int cb_udp_snd_message_external */

int
cb_udp_snd_message_external (int zCall, osip_message_t * sip)
{
#if 0
	char           *mod = { "cb_udp_snd_message_external" };
	int             len = 0;
	size_t          length = 0;
	static int      num = 0;
	struct addrinfo *addrinfo;
	struct __eXosip_sockaddr addr;
	char           *message = NULL;
	int             i;
	char           *host = NULL;

	//char * host = sip->req_uri->host;
	int             port = 5060;

	if (sip->req_uri && sip->req_uri->host)
	{
		host = sip->req_uri->host;
		port = osip_atoi (sip->req_uri->port);
	}
	else
	{
		host = gCall[zCall].remoteSipIp;
		port = gCall[zCall].remoteSipPort;
	}

	if (gCall[zCall].remoteContactIp[0])
	{
		host = gCall[zCall].remoteContactIp;

		if (gCall[zCall].remoteContactPort > -1)
		{
			port = gCall[zCall].remoteContactPort;
		}
	}

	if (port < 0)
	{
		port = 5060;
	}

	if (eXosip.j_socket == 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Failed to get jsocket for host(%s) and port (%d).", host,
				   port);

		return -1;
	}

	i = eXosip_get_addrinfo (geXcontext, &addrinfo, host, port, IPPROTO_UDP);
	if (i != 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Failed to get addr info for host(%s) and port (%d).",
				   host, port);
		return -1;
	}

	memcpy (&addr, addrinfo->ai_addr, addrinfo->ai_addrlen);

	len = addrinfo->ai_addrlen;

	freeaddrinfo (addrinfo);

	i = osip_message_to_str (sip, &message, &length);

	if (i != 0 || length <= 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Failed osip_message_to_str.");
		return -1;
	}

#if 1
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "processing %s.", message);
#endif

	if (0 > sendto (eXosip.j_socket, (const void *) message, length, 0,
					(struct sockaddr *) &addr, len /* sizeof(addr) */ ))
	{
		if (ECONNREFUSED == errno)
		{
			osip_free (message);
			return 1;
		}
		else
		{
			/*
			 * SIP_NETWORK_ERROR; 
			 */
			osip_free (message);
			return -1;
		}
	}

	osip_free (message);

#endif

	return 0;

}/*END: int cb_udp_snd_message_external */

int
process_DMOP_SENDKEEPALIVE(int zCall)
{
	int rc = -1;

	char            mod[] = { "process_DMOP_SENDKEEPALIVE" };

	osip_message_t *options = NULL;

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Sending keep alive.", "", zCall);

	eXosip_lock (geXcontext);
	rc = eXosip_call_build_options (geXcontext, gCall[zCall].did, &options);
	eXosip_unlock (geXcontext);

	if(rc == 0)
	{
#if 0
		char           *yStrSipMessageBuffer = NULL;
		size_t          yIntSipMessageLen = 0;
		int             yIntOsipMessageToStrRc = -1;

		osip_message_set_content_type (options, "application/text");
		osip_message_set_body (options, "keep alive", strlen ("keep alive"));

		yIntOsipMessageToStrRc =
			osip_message_to_str (options,
								 &yStrSipMessageBuffer,
								 (size_t *) &yIntSipMessageLen);

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Sending keep alive.", yStrSipMessageBuffer, zCall);

#endif
		eXosip_lock (geXcontext);
		rc = eXosip_call_send_request (geXcontext, gCall[zCall].did, options);

		if (rc != 0)
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Failed to send keep alive.", "Error in eXosip_call_send_message", zCall);
		}
#if 0
		cb_udp_snd_message_external (zCall, options);
#endif
		eXosip_unlock (geXcontext);
	}
	else
	{
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Failed to build keep alive.", "Error in eXosip_call_build_info", zCall);
		;
	}

	options = NULL;
	return (0);

}//end: int process_DMOP_SENDKEEPALIVE

int
process_DMOP_SENDVFU (int zCall, struct MsgToDM *lpMsgToDM)
{
	int             yIntLen = 0;
	int             rc = -1;
	char            yStrVFUString[256];
	osip_message_t *info = NULL;

	memset (yStrVFUString, 0, 256);

#if 0
	sprintf (yStrVFUString, "%s",
			 "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
			 "<media_control>"
			 "<vc_primitive>"
			 "<to_encoder>"
			 "<picture_fast_update/>"
			 "</to_encoder>" "</vc_primitive>" "</media_control>");
#endif

	sprintf (yStrVFUString, "%s",
			 "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
			 "<media_control>"
			 "<vc_primitive>"
			 "<to_encoder>"
			 "<picture_fast_update>"
			 "</picture_fast_update>"
			 "</to_encoder>" "</vc_primitive>" "</media_control>");

	yIntLen = strlen (yStrVFUString);

	eXosip_lock (geXcontext);

	rc = eXosip_call_build_info (geXcontext, gCall[zCall].did, &info);

	if (rc == 0)
	{
		osip_message_set_content_type (info, "application/media_control+xml");

		osip_message_set_body (info, yStrVFUString, yIntLen);

		cb_udp_snd_message_external (zCall, info);

		osip_message_free (info);
		info = NULL;
	}

	eXosip_unlock (geXcontext);

	return (0);

}								/*END: int process_DMOP_SENDVFU */

//#ifdef CALEA
int
process_DMOP_GETPORTDETAILFORANI (char *zData)
{
	char            mod[] = "process_DMOP_GETPORTDETAILFORANI";
	char            lAni[128] = "";
	char            respCaleaFifo[64] = "/tmp/caleaAppRespFifo";
	int             respCaleaFifoFd = -1;
	struct MsgToApp yMsgToApp;
	int             rc = 0;
	int             lFoundAni = 0;
	int             i = 0;

	yMsgToApp.opcode = DMOP_GETPORTDETAILFORANI;

	sprintf (lAni, "%s", zData);

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "ArcCalea: Looking for ani=%s", lAni);

	for (i = gStartPort; i < gEndPort; i++)
	{
		if (canContinue (mod, i) &&
			gCall[i].callState != CALL_STATE_IDLE &&
			strcmp (gCall[i].ani, lAni) == 0)
		{
			dynVarLog (__LINE__, i, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "ArcCalea: Found Match for ani=%s, callState=%d.",
					   gCall[i].ani, gCall[i].callState);

			lFoundAni = 1;

			if (gCall[i].callState == CALL_STATE_CALL_BRIDGED ||
				gCall[i].callState == CALL_STATE_CALL_INITIATE_ISSUED)
			{

	char            remoteIP[256] = "";

				sprintf (remoteIP, "%s", gCall[i].remoteRtpIp);

	char           *yTmpCr = strchr (remoteIP, '\r');

				if (yTmpCr != NULL)
				{
					yTmpCr[0] = 0;
				}

	char            remoteCrossPortIP[256] = "";

				sprintf (remoteCrossPortIP, "%s", gCall[i].remoteRtpIp);
	char           *yTmpCrossCr = strchr (remoteCrossPortIP, '\r');

				if (yTmpCr != NULL)
				{
					yTmpCr[0] = 0;
				}

				//sprintf(yMsgToApp.message, "PORT_NUM=%d", i);
#if 1
				sprintf (yMsgToApp.message,
						 "PORT_NUM=%d\n"
						 "IN_RTP_PORT=%d\n"
						 "OUT_RTP_PORT=%d\n"
						 "AUDIO_CODEC=%s\n"
						 "REMOTE_IP=%s\n"
						 "PORT_B_NUM=%d\n"
						 "IN_B_RTP_PORT=%d\n"
						 "OUT_B_RTP_PORT=%d\n"
						 "AUDIO_B_CODEC=%s\n"
						 "REMOTE_B_IP=%s\n"
						 "B_DNIS=%s\n"
						 "DYNMGR_ID=%d",
						 i,
						 gCall[i].remoteRtpPort,
						 gCall[i].localSdpPort,
						 gCall[i].audioCodecString,
						 remoteIP,
						 gCall[i].crossPort,
						 gCall[gCall[i].crossPort].remoteRtpPort,
						 gCall[gCall[i].crossPort].localSdpPort,
						 gCall[gCall[i].crossPort].audioCodecString,
						 remoteCrossPortIP,
						 gCall[gCall[i].crossPort].dnis, gDynMgrId);
#endif
			}
			else
			{
	char            remoteIP[256] = "";

				sprintf (remoteIP, "%s", gCall[i].remoteRtpIp);

	char           *yTmpCr = strchr (remoteIP, '\r');

				if (yTmpCr != NULL)
				{
					yTmpCr[0] = 0;
				}
				//sprintf(yMsgToApp.message, "PORT_NUM=%d", i);
#if 1
				sprintf (yMsgToApp.message,
						 "PORT_NUM=%d\n"
						 "IN_RTP_PORT=%d\n"
						 "OUT_RTP_PORT=%d\n"
						 "AUDIO_CODEC=%s\n"
						 "REMOTE_IP=%s\n"
						 "DYNMGR_ID=%d",
						 i,
						 gCall[i].remoteRtpPort,
						 gCall[i].localSdpPort,
						 gCall[i].audioCodecString, remoteIP, gDynMgrId);
#endif
			}

	FILE           *fp;
	char            yStrFileName[128];

			sprintf (yStrFileName, "/tmp/portMessage.%d", gDynMgrId);

			if ((fp = fopen (yStrFileName, "w+")) == NULL)
			{
				dynVarLog (__LINE__, i, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
						   ERR,
						   "ArcCalea: Failed to open file (%s), [%d, %s] "
						   "Unable to write port information to application.",
						   yStrFileName, errno, strerror (errno));

			}
			else
			{
				fprintf (fp, "%s", yMsgToApp.message);
				fclose (fp);
				sprintf (yMsgToApp.message, "%s", yStrFileName);
			}

			if ((respCaleaFifoFd = open (respCaleaFifo, O_WRONLY)) >= 0)
			{
				rc = write (respCaleaFifoFd, (char *) (&yMsgToApp),
							sizeof (struct MsgToApp));
				dynVarLog (__LINE__, i, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						   "ArcCalea: Wrote(%s) %d bytes to fifo(%d).",
						   yMsgToApp.message, rc, respCaleaFifoFd);
				close (respCaleaFifoFd);
			}
			else
			{
				dynVarLog (__LINE__, i, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
						   ERR,
						   "ArcCalea: Fail to open fifo(%s) for writing.  rc=%d.  [%d, %s] "
						   "Unable to write port information to application.",
						   respCaleaFifo, respCaleaFifoFd, errno,
						   strerror (errno));
			}

			break;
		}
	}
	if (lFoundAni == 0)
	{
		dynVarLog (__LINE__, i, mod, REPORT_DETAIL, TEL_DATA_NOT_FOUND, ERR,
				   "ArcCalea: Failed to find port for ANI=%s.  "
				   "Unable to write port information to application.", lAni);
	}

	return(0);
}								// END: process_DMOP_GETPORTDETAILFORANI()

int
process_DMOP_RTPPORTDETAIL ()
{
char            mod[] = "parseRtpFile";
FILE           *rtpFile = NULL;
char            path[128] = "";
char            buf[128] = "";
int             zCall;
int             yAudioPort;
char            portType[128] = "";

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Inside process_DMOP_RTPPORTDETAIL.");

	sprintf (path, "portDetail.%d", gDynMgrId);
	rtpFile = fopen (path, "r");
	if (rtpFile != NULL)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Successfully opened file(%s) to retrieve rtp port details.",
				   path);
		while (fgets (buf, sizeof (buf) - 1, rtpFile))
		{
			sscanf (buf, "%d_%d", &zCall, &yAudioPort);
			gCall[zCall].localSdpPort = yAudioPort;
//			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//					   "Buf=%s, audio(%d)",
//					   buf, yAudioPort);

		}
		fclose (rtpFile);
		unlink (path);
	}
	gIsRtpPortSet = YES;

	return(0);
}

/*------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
int
process_DMOP_BLASTDIAL (int zCall)
{
	char            mod[] = "process_DMOP_BLASTDIAL";
	int             rc;
	int             i;

	gCall[zCall].isInBlastDial = 1;
	//memcpy (&response,            &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	memcpy (&gMsgBlastDial, &(gCall[zCall].msgToDM),
			sizeof (struct Msg_BlastDial));

	rc = pthread_create (&gCall[zCall].lastTimeThreadId,
						 &gThreadAttr,
						 blastDialThread, (void *) &gMsgBlastDial);
	if (rc != 0)
	{
		processThreadCreationFailure (mod, "blastDialThread",
									  &(gCall[zCall].msgToDM), rc);
	}

	return (0);
}								//  END: process_DMOP_BLASTDIAL()

///This is the function we use to fire the initiateCallThread() whenever we see a DMOP_INITIATECALL.
int
process_DMOP_INITIATECALL (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_INITIATECALL" };
	struct Msg_InitiateCall msgInitiateCall;
// MR 4655
	struct Msg_InitiateCall msgCallIssued;
// END: MR 4655

	struct MsgToApp response;
	char            yStrErrMsg[MAX_LOG_MSG_LENGTH];
	int             yTimeout = 0;
    char ringEventTime[256];

	memcpy (&msgInitiateCall, &(gCall[zCall].msgToDM),
			sizeof (struct Msg_InitiateCall));
	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));
// MR 4655 
	memcpy (&msgCallIssued, &msgInitiateCall, sizeof (struct Msg_InitiateCall));
	msgCallIssued.opcode = DMOP_CALL_INITIATED;
	msgCallIssued.informat = 1;   // Call is in initiated state
// END: MR 4655

    getRingEventTime (ringEventTime);
    sprintf (gCall[zCall].lastEventTime, "%s", ringEventTime);

	/*
	 * Step 1:  If it is for Bridge / Transfer,  call appropriate functions
	 */

	if (msgInitiateCall.whichOne == 2)
	{

		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Firing", "bridgeCallThread", -1);
		/*
		 * Step 2:  If it is for Initiate, continue
		 */

		setCallState (zCall, CALL_STATE_CALL_INITIATE_ISSUED);

		//gCall[zCall].callState = CALL_STATE_CALL_INITIATE_ISSUED;

		yRc = pthread_create (&gCall[zCall].lastTimeThreadId,
							  &gThreadAttr,
							  bridgeCallThread,
							  (void *) &(gCall[zCall].msgToDM));
		if (yRc != 0)
		{
			processThreadCreationFailure (mod, "bridgeCallThread",
										  &(gCall[zCall].msgToDM), yRc);
		}
	}
	else
	{
		/*
		 * Step 2:  If it is for Initiate, continue
		 */

		__DDNDEBUG__ (DEBUG_MODULE_CALL, "Firing", "initiateCallThread", -1);

		setCallState (zCall, CALL_STATE_CALL_INITIATE_ISSUED);
// MR 4655 
		yRc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &msgCallIssued, mod);
// END: MR 4655
		//gCall[zCall].callState = CALL_STATE_CALL_INITIATE_ISSUED;

		yRc = pthread_create (&gCall[zCall].lastTimeThreadId,
							  &gThreadAttr,
							  initiateCallThread,
							  (void *) &(gCall[zCall].msgToDM));

		if (yRc != 0)
		{
			processThreadCreationFailure (mod, "initiateCallThread",
										  &(gCall[zCall].msgToDM), yRc);
		}
	}

	return (0);

}								/*END: int process_DMOP_INITIATECALL */

int
process_DMOP_CONFERENCESTART (int zCall)
{
	int             yRc = -1;
	char            mod[] = "";

	return -1;
}

int
process_DMOP_CONFERENCEADDUSER (int zCall)
{
	int             yRc = -1;
	char            mod[] = "";

	return -1;
}

int
process_DMOP_CONFERENCEREMOVEUSER (int zCall)
{
	int             yRc = -1;
	char            mod[] = "";

	return -1;
}

int
process_DMOP_CONFERENCESTOP (int zCall)
{
	int             yRc = -1;
	char            mod[] = "";

	MSG_CONF_STOP  *yTmpConferenceStop =
		(MSG_CONF_STOP *) & (gCall[zCall].msgToDM);

	if (strcmp (yTmpConferenceStop->confId, gCall[zCall].confID) == 0)
	{
		memset (gCall[zCall].confID, 0, sizeof (gCall[zCall].confID));
		gCall[zCall].confUserCount = 0;

		for (int i = 0; i < MAX_CONF_PORTS; i++)
		{
			gCall[zCall].conf_ports[i] = -1;
		}
	}
	return 0;
}

int
process_DMOP_CONFERENCESTART_RESPONSE (int zCall)
{
	int             yRc = -1;
	char            mod[] = "";

	return -1;
}

int
process_DMOP_CONFERENCEADDUSER_RESPONSE (int zCall)
{
	int             yRc = -1;
	char            mod[] = "";

	return -1;
}

int
process_DMOP_CONFERENCEREMOVEUSER_RESPONSE (int zCall)
{
	int             yRc = -1;
	char            mod[] = "";

	return -1;
}

int
process_DMOP_CONFERENCESTOP_RESPONSE (int zCall)
{
	int             yRc = -1;
	char            mod[] = "";

	return -1;
}

///This is the function we use to fire the listenCallThread() whenever we see a DMOP_LISTENCALL.
int
process_DMOP_LISTENCALL (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_LISTENCALL" };
	struct Msg_ListenCall msgListenCall;
	struct MsgToApp response;
	char            yStrErrMsg[MAX_LOG_MSG_LENGTH];
	int             yTimeout = 0;

	memcpy (&msgListenCall, &(gCall[zCall].msgToDM),
			sizeof (struct Msg_ListenCall));
	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	if (msgListenCall.informat == INTERNAL_PORT)
	{
	int             yIntBLeg = atoi (msgListenCall.phoneNumber);

		if (yIntBLeg <= -1 || yIntBLeg > MAX_PORTS)
		{
			return (-1);
		}

		yRc = notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);

		return (yRc);
	}

	setCallState (zCall, CALL_STATE_CALL_INITIATE_ISSUED);
	//gCall[zCall].callState = CALL_STATE_CALL_INITIATE_ISSUED;

	yRc = pthread_create (&gCall[zCall].lastTimeThreadId,
						  &gThreadAttr,
						  listenCallThread, (void *) &(gCall[zCall].msgToDM));
	if (yRc != 0)
	{
		processThreadCreationFailure (mod, "listenCallThread",
									  &(gCall[zCall].msgToDM), yRc);
	}

	return (0);

}								/*END: int process_DMOP_LISTENCALL */

///This is the function we use to notify Media Manager of an audio channel connect whenever we see a DMOP_BRIDGECONNECT.
int
process_DMOP_BRIDGECONNECT (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_BRIDGECONNECT" };

	int             yIntBLeg = gCall[zCall].crossPort;

	if (yIntBLeg <= -1 || yIntBLeg > MAX_PORTS)
	{
		return (-1);
	}

	if (yIntBLeg > -1 && yIntBLeg < MAX_PORTS)
	{
		gCall[yIntBLeg].appPid = gCall[zCall].appPid;
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "gCall[%d-bleg].appPid=%d, gCall[%d-aleg].appPid=%d.",
			   yIntBLeg, gCall[yIntBLeg].appPid, zCall, gCall[zCall].appPid);

	setCallState (zCall, CALL_STATE_CALL_BRIDGED);
	//gCall[zCall].callState = CALL_STATE_CALL_BRIDGED;
	setCallState (gCall[zCall].crossPort, CALL_STATE_CALL_BRIDGED);
	//gCall[gCall[zCall].crossPort].callState = CALL_STATE_CALL_BRIDGED;

	yRc = notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);

	return (yRc);

}								/*END: int process_DMOP_BRIDGECONNECT */

int
process_DMOP_BRIDGE_THIRD_LEG (int zCall)
{

	int             rc = 0;
	int             legs[3] = { -1, -1, -1 };
	enum legs_e
	{ A = 0, B, C };
	char            mod[] = "process_DMOP_BRIDGE_THIRD_LEG";

	//struct Msg_InitiateCall *appMsg = (struct Msg_InitiateCall *) &gCall[zCall].msgToDM;

	struct MsgToApp response;

	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	if (zCall > -1 && zCall <= MAX_PORTS)
	{
		legs[A] = zCall;
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PORT, ERR,
				   "Invalid port number received (%d). Unable to bridge third leg; returning failure.",
				   zCall);
		writeGenericResponseToApp (__LINE__, zCall, (struct MsgToApp *) &response, mod);
		return (-1);
	}

	// call has to be in bridge call with crossport otherwise fail
	legs[B] = gCall[zCall].crossPort;
	if ((legs[B] < 0) || (legs[B] >= MAX_PORTS))
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PORT, ERR,
				   "Incorrect crossport received (%d), Unable to bridge third leg; returning failure.",
				   gCall[zCall].crossPort);
		writeGenericResponseToApp (__LINE__, zCall, (struct MsgToApp *) &response, mod);
		return (-1);
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Attempting to bridge third leg; "
			   "leg[a]=%d leg[b]=%d leg[c]=%d.", legs[A], legs[B], legs[C]);

	// start a bridge call thread here 

	rc = process_DMOP_INITIATECALL (zCall);

	if (rc != 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   " process_DMOP_INITATECALL(%d) returned %d", zCall, rc);
	}

	// rc = notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), (char *)__func__);

	return rc;
}

///This is the function we use to fire the transferCallThread() whenever we see a DMOP_TRANSFERCALL.
int
process_DMOP_TRANSFERCALL (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_TRANSFERCALL" };

	struct Msg_InitiateCall yMsgInitiateCall;

	memcpy (&yMsgInitiateCall, &gCall[zCall].msgToDM,
			sizeof (struct Msg_InitiateCall));

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "", "", -1);

	/*
	 * Step 2:  If it is for Initiate, continue
	 */

	setCallState (zCall, CALL_STATE_CALL_TRANSFER_ISSUED);
	//gCall[zCall].callState = CALL_STATE_CALL_TRANSFER_ISSUED;

	/*
	 * SSX START
	 */

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Initiating a TEL_TransferCall", "",
				  yMsgInitiateCall.listenType);

	if (yMsgInitiateCall.listenType == BLIND)
	{
		__DDNDEBUG__ (DEBUG_MODULE_CALL, "This is a BLIND Transfer Call", "",
					  zCall);

		yRc =
			pthread_create (&gCall[zCall].lastTimeThreadId, &gThreadAttr,
							transferCallThread,
							(void *) &(gCall[zCall].msgToDM));
	}
	else if (yMsgInitiateCall.listenType == LISTEN_ALL)
	{
		__DDNDEBUG__ (DEBUG_MODULE_CALL,
					  "Initiating the DIAL_OUT of the SUPERVISED Transfer Call",
					  "", zCall);

		yRc =
			pthread_create (&gCall[zCall].lastTimeThreadId, &gThreadAttr,
							transferCallThread,
							(void *) &(gCall[zCall].msgToDM));

	}
	else if (yMsgInitiateCall.listenType == DIAL_OUT)
	{
		__DDNDEBUG__ (DEBUG_MODULE_CALL,
					  "Initiating the DIAL_OUT of the SUPERVISED Transfer Call",
					  "", zCall);

		yRc =
			pthread_create (&gCall[zCall].lastTimeThreadId, &gThreadAttr,
							transferCallThread,
							(void *) &(gCall[zCall].msgToDM));

	}
	else if (yMsgInitiateCall.listenType == CONNECT)
	{
		__DDNDEBUG__ (DEBUG_MODULE_CALL,
					  "Initiating the CONNECT of the SUPERVISED Transfer Call",
					  "", zCall);

		yRc =
			pthread_create (&gCall[zCall].lastTimeThreadId, &gThreadAttr,
							transferCallThread,
							(void *) &(gCall[zCall].msgToDM));
	}
	else
	{
		__DDNDEBUG__ (DEBUG_MODULE_CALL,
					  "Initiating the default Transfer Call, BLIND", "",
					  zCall);

		yRc =
			pthread_create (&gCall[zCall].lastTimeThreadId, &gThreadAttr,
							transferCallThread,
							(void *) &(gCall[zCall].msgToDM));
	}

	if (yRc != 0)
	{
		processThreadCreationFailure (mod, "transferCallThread",
									  &(gCall[zCall].msgToDM), yRc);
	}

	return (yRc);

}								/*END: int process_DMOP_TRANSFERCALL */

///This is the function we use to notify Responsibility whenever we see a DMOP_STARTFAXAPP.
int
process_DMOP_STARTFAXAPP (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_STARTFAXAPP" };

	struct MsgToRes yMsgToRes;
	struct Msg_StartFaxApp yMsgFaxApp;

	char            dnis[256];
	char            ani[256];

	memcpy (&yMsgFaxApp, &(gCall[zCall].msgToDM),
			sizeof (struct Msg_StartFaxApp));
	memcpy (&yMsgToRes, &(gCall[zCall].msgToDM), sizeof (struct MsgToRes));

	sprintf (ani, "%s", "\0");
	sprintf (dnis, "%s", "\0");

	clearPort (zCall);

	setCallState (zCall, CALL_STATE_CALL_NEW);
	gCall[zCall].callState = CALL_STATE_CALL_NEW;
	gCall[zCall].lastError = CALL_NO_ERROR;

	sprintf (gCall[zCall].dnis, "%s", yMsgFaxApp.dnis);
	sprintf (gCall[zCall].ani, "%s", yMsgFaxApp.ani);

	sprintf (yMsgToRes.ani, "%s", yMsgFaxApp.ani);
	sprintf (yMsgToRes.dnis, "%s", yMsgFaxApp.dnis);

	sprintf (yMsgToRes.data, "%s", yMsgFaxApp.data);

	yMsgToRes.opcode = RESOP_START_FAX_APP;
	yMsgToRes.appCallNum = zCall;

	yMsgToRes.appPassword = gCall[zCall].appPassword;

	yRc = writeToResp (mod, (void *) &yMsgToRes);

	return (yRc);

}								/*END: int process_DMOP_STARTFAXAPP */

///This is the function we use to notify Responsibility whenever we see a DMOP_STARTOCSAPP.
int
process_DMOP_STARTOCSAPP (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_STARTOCSAPP" };

	struct MsgToRes yMsgToRes;
	struct Msg_StartOcsApp yMsgOcsApp;

	char            dnis[256];
	char            ani[256];

	memcpy (&yMsgOcsApp, &(gCall[zCall].msgToDM),
			sizeof (struct Msg_StartOcsApp));
	memcpy (&yMsgToRes, &(gCall[zCall].msgToDM), sizeof (struct MsgToRes));

	sprintf (ani, "%s", "\0");
	sprintf (dnis, "%s", "\0");


	sprintf (yMsgToRes.ani, "%s", yMsgOcsApp.ani);
	sprintf (yMsgToRes.dnis, "%s", yMsgOcsApp.dnis);
	sprintf (yMsgToRes.data, "%s", yMsgOcsApp.data);
	yMsgToRes.opcode = RESOP_START_OCS_APP;
    yMsgToRes.appPassword = gCall[zCall].appPassword;
	yMsgToRes.appRef = 0;

	if ( gCall[zCall].callState == CALL_STATE_RESERVED_NEW_CALL )
	{
      	yMsgToRes.appRef = -1;
		dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Port %d is in call state CALL_STATE_RESERVED_NEW_CALL (bLeg).  Rejecting OCS request.", zCall);
	}
	else
	{
		clearPort (zCall);

		yMsgToRes.appCallNum = zCall;
		setCallState (zCall, CALL_STATE_CALL_NEW);
		gCall[zCall].callState = CALL_STATE_CALL_NEW;
		gCall[zCall].lastError = CALL_NO_ERROR;
	
		sprintf (gCall[zCall].dnis, "%s", yMsgOcsApp.dnis);
		sprintf (gCall[zCall].ani, "%s", yMsgOcsApp.ani);
		yMsgToRes.appCallNum = zCall;
	
		if(gShutDownFlag == 1)
		{
			dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			   "gShutDownFlag is set, sending a -1 Back to Resp for a STARTOCS app");
			yMsgToRes.appRef = -1;
		}
	}

	yRc = writeToResp (mod, (void *) &yMsgToRes);

	return (yRc);

}								/*END: int process_DMOP_STARTOCSAPP */

//This is the function we use to notify Responsibility whenever we see a DMOP_STARTNTOCSAPP.
int process_DMOP_STARTNTOCSAPP (int zCall)
{
	int yRc = 0;
	char mod[] = "process_DMOP_STARTNTOCSAPP";
	
	struct MsgToRes yMsgToRes;
	struct Msg_StartOcsApp yMsgOcsApp;

	char dnis[256];
	char ani[256];

	memcpy (&yMsgOcsApp, &(gCall[zCall].msgToDM), sizeof (struct Msg_StartOcsApp));
	memcpy (&yMsgToRes, &(gCall[zCall].msgToDM), sizeof (struct MsgToRes));

	sprintf (ani, "%s", "\0");
	sprintf (dnis, "%s", "\0");
	
	clearPort(zCall);
	
	setCallState(zCall, CALL_STATE_CALL_NEW);
	gCall[zCall].callState = CALL_STATE_CALL_NEW;
	gCall[zCall].lastError = CALL_NO_ERROR;

	sprintf (gCall[zCall].dnis, "%s", yMsgOcsApp.dnis);
	sprintf (gCall[zCall].ani, "%s", yMsgOcsApp.ani);

	sprintf (yMsgToRes.ani, "%s", yMsgOcsApp.ani);
	sprintf (yMsgToRes.dnis, "%s", yMsgOcsApp.dnis);

	sprintf (yMsgToRes.data, "%s", yMsgOcsApp.data);

	yMsgToRes.opcode = RESOP_START_NTOCS_APP_DYN;
	yMsgToRes.appCallNum = zCall;

	yMsgToRes.appPassword = gCall[zCall].appPassword;

    if(gShutDownFlag == 1){
     dynVarLog (__LINE__, zCall, (char *)__func__, REPORT_VERBOSE, TEL_BASE, INFO,
			   "gShutDownFlag is set, sending a -1 Back to Resp for a STARTOCS app");
      yMsgToRes.appRef = -1;
    }

	yRc = writeToResp (mod, (void *) &yMsgToRes);

	return (yRc);

}/*END: int process_DMOP_STARTNTOCSAPP */

int
startMediaManagerThreads (int zCall, char *mod)
{
	int             yRc;

	switch (gCall[zCall].sendRecvStatus)
	{
	case SENDONLY:

		if (gCall[zCall].remoteRtpPort > 0)
		{
	struct MsgToDM  msgRtpDetails;
			memcpy (&msgRtpDetails, &(gCall[zCall].msgToDM),
					sizeof (struct MsgToDM));
			gCall[zCall].isInputThreadStarted = 1;

			msgRtpDetails.opcode = DMOP_RTPDETAILS;
			sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
					 gCall[zCall].leg,
					 gCall[zCall].remoteRtpPort,
					 gCall[zCall].localSdpPort,
					 gCall[zCall].telephonePayloadType,
					 RECVONLY,
					 gCall[zCall].codecType, gCall[zCall].remoteRtpIp);

			yRc =
				notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msgRtpDetails),
								mod);

		}

		break;
	case RECVONLY:
		if (gCall[zCall].remoteRtpPort > 0)
		{
	struct MsgToDM  msgRtpDetails;
			memcpy (&msgRtpDetails, &(gCall[zCall].msgToDM),
					sizeof (struct MsgToDM));
			gCall[zCall].isOutputThreadStarted = 1;

			msgRtpDetails.opcode = DMOP_RTPDETAILS;
			sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
					 gCall[zCall].leg,
					 gCall[zCall].remoteRtpPort,
					 gCall[zCall].localSdpPort,
					 gCall[zCall].telephonePayloadType,
					 SENDONLY,
					 gCall[zCall].codecType, gCall[zCall].remoteRtpIp);

			yRc =
				notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msgRtpDetails),
								mod);

		}

		break;

	case SENDRECV:
		if (gCall[zCall].remoteRtpPort > 0)
		{
	struct MsgToDM  msgRtpDetails;
			memcpy (&msgRtpDetails, &(gCall[zCall].msgToDM),
					sizeof (struct MsgToDM));
			gCall[zCall].isInputThreadStarted = 1;
			gCall[zCall].isOutputThreadStarted = 1;

			msgRtpDetails.opcode = DMOP_RTPDETAILS;
			sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
					 gCall[zCall].leg,
					 gCall[zCall].remoteRtpPort,
					 gCall[zCall].localSdpPort,
					 gCall[zCall].telephonePayloadType,
					 SENDRECV,
					 gCall[zCall].codecType, gCall[zCall].remoteRtpIp);

			yRc =
				notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msgRtpDetails),
								mod);

		}

		break;

	case INACTIVE:
	default:
		break;
	}
	return(0);
}

///This is the function we use to intialize Telecom whenever we see a DMOP_INITTELECOM.
int
process_DMOP_INITTELECOM (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_INITTELECOM" };
	char            message[MAX_LOG_MSG_LENGTH];
	struct MsgToDM  msg = gCall[zCall].msgToDM;
	struct MsgToApp response;

	struct Msg_InitTelecom *ptrMsgInit =
		(struct Msg_InitTelecom *) &(gCall[zCall].msgToDM);

	sprintf (message, "{op:%d,c#:%d,pid:%d,ref:%d,pw:%d", msg.opcode,
			 msg.appCallNum, msg.appPid, msg.appRef, msg.appPassword);

	dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"Processing %s. dropCallThreadId=%d", message,
		gCall[zCall].dropCallThreadId);

	if (gCall[zCall].dropCallThreadId == -99)		// BT-244
	{
		gCall[zCall].dropCallThreadId = 0;  // just incase; down want it to core on pthread_kill()
	}
	if ((gCall[zCall].dropCallThreadId != 0) && (pthread_kill(gCall[zCall].dropCallThreadId, 0) == 0))
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN,
			"DropCall thread (%d) is still running. Setting flag exit dropCallThread.",
		gCall[zCall].dropCallThreadId);
		gCall[zCall].dropCallThreadId = -99;
	}

	if (gCall[zCall].responseFifo[0])
	{
		if (gCall[zCall].responseFifoFd > 0)
		{
			close (gCall[zCall].responseFifoFd);
		}
		arc_unlink (zCall, mod, gCall[zCall].responseFifo);
	}
	gCall[zCall].responseFifoFd = -1;

	if (gCall[zCall].permanentlyReserved)
	{
		clearPort (zCall);
	}
	response.opcode = msg.opcode;
	response.appCallNum = msg.appCallNum;
	response.appPid = msg.appPid;
	response.appRef = msg.appRef;
	response.returnCode = 0;
	response.appPassword = msg.appPassword;

	gCall[zCall].appPid = msg.appPid;
	gCall[zCall].appPassword = msg.appPassword;

	if (ptrMsgInit->killApp == NO)
	{
		gCall[zCall].canKillApp = ptrMsgInit->killApp;
	}
	else
	{
		gCall[zCall].canKillApp = YES;
	}

	sprintf (gCall[zCall].responseFifo, "%s", ptrMsgInit->responseFifo);

    if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED                    // MR 4951
                 || gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
                 || gCall[zCall].callState == CALL_STATE_CALL_TERMINATE_CALLED
                 || gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
    {
        gCall[zCall].cid = -1;
        gCall[zCall].did = -1;
        dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_SIP_SIGNALING, ERR,
                    "Call was cancelled by the remote end.  Returning failure.  tid=%d.", gCall[zCall].tid);
        __DDNDEBUG__ (DEBUG_MODULE_SIP, "Call was cancelled by the remote end.  Returning failure.", "tid is ", gCall[zCall].tid);
        response.returnCode = -1;
        sprintf (response.message, "%s", "TEL_InitTelecom failed - call canceled by remote end.");
    
        writeGenericResponseToApp (__LINE__, zCall, &response, mod);
        writeCDR ("process_CALL_NEW", zCall, "RC", 20014, -1);  //DDN_TODO: SNMP SetRequest
        memset (gCall[zCall].GV_CustData1, 0,
                sizeof (gCall[zCall].GV_CustData1));
        
        return(-1);
    }

	/*
	 * Write Call info to a file so that MediaMgr can read it
	 */
	{
	FILE           *yFpCallInfo = NULL;
	char            yStrFileCallInfo[100];

		sprintf (yStrFileCallInfo, "%s/callInfo.%d", gCacheDirectory, zCall);

		yFpCallInfo = fopen (yStrFileCallInfo, "w+");

		if (yFpCallInfo == NULL)
		{
			dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_NORMAL,
					   TEL_FILE_IO_ERROR, ERR,
					   "Failed to open call info file (%s) for MediaMgr. [%d, %s] "
					   "No call information will be written.",
					   yStrFileCallInfo, errno, strerror (errno));
		}
		else
		{
			//if(gCall[zCall].GV_SipURL != NULL && gCall[zCall].GV_SipURL[0] != '\0')
			if (gCall[zCall].sdp_body_remote != NULL
				&& gCall[zCall].sdp_body_remote[0] != '\0')
			{
	int             yRc =
		getCallInfoFromSdp (zCall, gCall[zCall].sdp_body_remote);

				if (yRc == -1)
				{
					sprintf (gCall[zCall].GV_CustData1, "%s", "415");
					sprintf (gCall[zCall].GV_CustData2,
							 "AUDIO_CODEC=%s"
							 "&FRAME_RATE=%d",
							 gCall[zCall].audioCodecString,
							 -1);
				}

//              parseSdpMessage(zCall, __LINE__, gCall[zCall].sdp_body_remote, remote_sdp);

				dynVarLog (__LINE__, msg.appCallNum, mod, REPORT_VERBOSE,
						   TEL_BASE, INFO,
						   "gCall[zCall].audioCodecString=%s",   //, gCall[zCall].videoCodecString=%s",
						   gCall[zCall].audioCodecString);
//						   gCall[zCall].videoCodecString);

			}
			fprintf (yFpCallInfo, "ANI=%s\n", gCall[zCall].ani);
			fprintf (yFpCallInfo, "DNIS=%s\n", gCall[zCall].dnis);
			fprintf (yFpCallInfo, "OCN=%s\n", gCall[zCall].ocn);
			fprintf (yFpCallInfo, "RDN=%s\n", gCall[zCall].rdn);

			fprintf (yFpCallInfo, "CALL_TYPE=%s\n", gCall[zCall].call_type);
			fprintf (yFpCallInfo, "AUDIO_CODEC=%s\n",
					 gCall[zCall].audioCodecString);
			fprintf (yFpCallInfo, "ANNEXB=%d\n", gCall[zCall].isG729AnnexB);

			if (strstr (gCall[zCall].audioCodecString, "729") != NULL)
			{
				if (gCall[zCall].isG729AnnexB == YES)
				{
					sprintf (gCall[zCall].audioCodecString, "%s", "729b");
				}
				else
				{
					sprintf (gCall[zCall].audioCodecString, "%s", "729a");
				}
			}
			/*
			 * DDN_TODO: Add other call info to send to media mgr here
			 */
			fclose (yFpCallInfo);
		}
	}

	notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);

#if 1

	/*If Early Media is set then send 183 with sdp in it.
	   And Notify MediaMgr to start input and output thread */
	if (gInboundEarlyMedia == 1 && gCall[zCall].tid > -1)
	{
	osip_message_t *answer = NULL;
	int             yRc;
	char            yStrLocalSdpPort[15];

		sprintf (yStrLocalSdpPort, "%d", gCall[zCall].localSdpPort);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Sending SIP Message 183.");

		eXosip_lock (geXcontext);
		yRc = eXosip_call_build_answer (geXcontext, gCall[zCall].tid, 183, &answer);
		if (yRc < 0)
		{
			return (yRc);
		}

		if (gCall[zCall].PRACKSendRequire)
		{
			insertPRACKHeaders (zCall, answer);
		}

		if (gCall[zCall].remoteRtpPort > 0)
		{
				yRc = createSdpBody (zCall,
									 yStrLocalSdpPort,
									 gCall[zCall].codecType,
									 -2, gCall[zCall].telephonePayloadType);
		}
		else
		{
			yRc = createSdpBody (zCall, yStrLocalSdpPort, -1, -1, -1);
		}

		osip_message_set_body (answer, gCall[zCall].sdp_body_local,
							   strlen (gCall[zCall].sdp_body_local));
		osip_message_set_content_type (answer, "application/sdp");

		yRc = eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 183, answer);
		eXosip_unlock (geXcontext);

		startMediaManagerThreads (zCall, mod);
	}
#endif

	return (yRc);

}								/*END: int process_DMOP_INITTELECOM */

///This is the function we use to get global variables whenever we see a DMOP_GETGLOBAL.
int
process_DMOP_GETGLOBAL (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_GETGLOBAL" };

	struct MsgToApp response;

	struct Msg_GetGlobal yMsgGetGlobal;

	memcpy (&yMsgGetGlobal,
			&(gCall[zCall].msgToDM), sizeof (struct Msg_GetGlobal));

	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "%s [%s]", mod, yMsgGetGlobal.name);

	response.returnCode = 0;

	if (strcmp (yMsgGetGlobal.name, "$OUTBOUND_RET_CODE") == 0
		&& gCall[zCall].outboundRetCode != 0)
	{
		sprintf (response.message, "%d", gCall[zCall].outboundRetCode);

		response.opcode = gCall[zCall].msgToDM.opcode;
		response.appPid = gCall[zCall].msgToDM.appPid;
		response.appRef = gCall[zCall].msgToDM.appRef;
		response.appCallNum = gCall[zCall].msgToDM.appCallNum;
		response.appPassword = gCall[zCall].msgToDM.appPassword;
		response.returnCode = 0;

		writeGenericResponseToApp (__LINE__, zCall, (struct MsgToApp *) &response, mod);
	}
	else if (strcmp (yMsgGetGlobal.name, "$MAX_CALL_DURATION") == 0)
	{
		sprintf (response.message, "%d", gCall[zCall].GV_CallDuration);

		response.opcode = gCall[zCall].msgToDM.opcode;
		response.appPid = gCall[zCall].msgToDM.appPid;
		response.appRef = gCall[zCall].msgToDM.appRef;
		response.appCallNum = gCall[zCall].msgToDM.appCallNum;
		response.appPassword = gCall[zCall].msgToDM.appPassword;
		response.returnCode = 0;

		writeGenericResponseToApp (__LINE__, zCall, (struct MsgToApp *) &response, mod);
	}
	else
	{
		notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
	}

	return yRc;

}								/*END: int process_DMOP_GETGLOBAL */

///This is the function we use to get global string variables whenever we see a DMOP_GETGLOBALSTRING.
int
process_DMOP_GETGLOBALSTRING (int zCall)
{
	int             yRc = 0;
	char            mod[] = { "process_DMOP_GETGLOBALSTRING" };

	int             lCall = zCall;

	struct MsgToApp response;

	struct Msg_GetGlobalString yMsgGetGlobalString;

	memcpy (&yMsgGetGlobalString,
			&(gCall[lCall].msgToDM), sizeof (struct Msg_GetGlobalString));

	memcpy (&response, &(gCall[lCall].msgToDM), sizeof (struct MsgToApp));

	dynVarLog (__LINE__, lCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "%s [%s]", mod, yMsgGetGlobalString.name);

	response.returnCode = 0;

	if (strcmp (yMsgGetGlobalString.name, "$SIP_URI_VOICEXML") == 0)
	{
		sprintf (response.message, "%s", gCall[lCall].GV_SipUriVoiceXML);

		response.opcode = gCall[lCall].msgToDM.opcode;
		response.appPid = gCall[lCall].msgToDM.appPid;
		response.appRef = gCall[lCall].msgToDM.appRef;
		response.appCallNum = gCall[lCall].msgToDM.appCallNum;
		response.appPassword = gCall[lCall].msgToDM.appPassword;
		response.returnCode = 0;

		writeGenericResponseToApp (__LINE__, lCall, (struct MsgToApp *) &response, mod);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$ORIGINAL_ANI") == 0)
	{
		sprintf (response.message, "%s", gCall[zCall].originalAni);

		response.opcode = gCall[zCall].msgToDM.opcode;
		response.appPid = gCall[zCall].msgToDM.appPid;
		response.appRef = gCall[zCall].msgToDM.appRef;
		response.appCallNum = gCall[zCall].msgToDM.appCallNum;
		response.appPassword = gCall[zCall].msgToDM.appPassword;
		response.returnCode = 0;

		writeGenericResponseToApp (__LINE__, zCall, (struct MsgToApp *) &response, mod);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$TEL_TRANSFER_AAI") == 0)
	{
		if (gCall[zCall].remoteAAI == NULL ||
			gCall[zCall].remoteAAI[0] == '\0')
		{
			sprintf (response.message, "%s", "UNDEFINED");
		}
		else
		{
			sprintf (response.message, "%s", gCall[zCall].remoteAAI);
		}
		response.opcode = gCall[zCall].msgToDM.opcode;
		response.appPid = gCall[zCall].msgToDM.appPid;
		response.appRef = gCall[zCall].msgToDM.appRef;
		response.appCallNum = gCall[zCall].msgToDM.appCallNum;
		response.appPassword = gCall[zCall].msgToDM.appPassword;
		response.returnCode = 0;

		writeGenericResponseToApp (__LINE__, zCall, (struct MsgToApp *) &response, mod);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$ORIGINAL_DNIS") == 0)
	{
		sprintf (response.message, "%s", gCall[zCall].originalDnis);

		response.opcode = gCall[zCall].msgToDM.opcode;
		response.appPid = gCall[zCall].msgToDM.appPid;
		response.appRef = gCall[zCall].msgToDM.appRef;
		response.appCallNum = gCall[zCall].msgToDM.appCallNum;
		response.appPassword = gCall[zCall].msgToDM.appPassword;
		response.returnCode = 0;

		writeGenericResponseToApp (__LINE__, zCall, (struct MsgToApp *) &response, mod);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$PLATFORM_TYPE") == 0)
	{
		sprintf (response.message, "%s", "CARDLESS_SIP");

		response.opcode = gCall[zCall].msgToDM.opcode;
		response.appPid = gCall[zCall].msgToDM.appPid;
		response.appRef = gCall[zCall].msgToDM.appRef;
		response.appCallNum = gCall[zCall].msgToDM.appCallNum;
		response.appPassword = gCall[zCall].msgToDM.appPassword;
		response.returnCode = 0;

		writeGenericResponseToApp (__LINE__, zCall, (struct MsgToApp *) &response, mod);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$SIP_HEADER:USER-TO-USER") == 0){
        // return last user to user header here unencoded -- 
        if(gCall[zCall].remote_UserToUser[0] != '\0'){ 
		  sprintf (response.message, "%s", gCall[zCall].remote_UserToUser);
		  response.returnCode = 0;
        } else {
		  sprintf (response.message, "%s", "NOTDEFINED");
		  response.returnCode = -1;
        }
        response.opcode = gCall[zCall].msgToDM.opcode;
		response.appPid = gCall[zCall].msgToDM.appPid;
		response.appRef = gCall[zCall].msgToDM.appRef;
		response.appCallNum = gCall[zCall].msgToDM.appCallNum;
		response.appPassword = gCall[zCall].msgToDM.appPassword;
		writeGenericResponseToApp (__LINE__, zCall, (struct MsgToApp *) &response, mod);
    }
	else if (strcmp (yMsgGetGlobalString.name, "$SIP_MESSAGE") == 0)
	{
	FILE           *fp;
	char            yStrFileName[128];

		sprintf (yStrFileName, "%s/sipMessage.%d.%d", DEFAULT_FILE_DIR,
				 gCall[lCall].appPid, zCall);

		if ((fp = fopen (yStrFileName, "w+")) == NULL)
		{
			dynVarLog (__LINE__, lCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR,
					   "Failed to open file (%s). [%d, %s] Unable to retrieve $SIP_MESSAGE.",
					   yStrFileName, errno, strerror (errno));

			response.returnCode = -1;
		}
		else
		{
			fprintf (fp, "%s", gCall[lCall].sipMessage);
			fclose (fp);
			sprintf (response.message, "%s", yStrFileName);
			response.returnCode = 0;
		}

		//sprintf(response.message, "%s", gCall[zCall].sipMessage);

		response.opcode = gCall[lCall].msgToDM.opcode;
		response.appPid = gCall[lCall].msgToDM.appPid;
		response.appRef = gCall[lCall].msgToDM.appRef;
		response.appCallNum = gCall[lCall].msgToDM.appCallNum;
		response.appPassword = gCall[lCall].msgToDM.appPassword;
		response.returnCode = 0;

		writeGenericResponseToApp (__LINE__, lCall, (struct MsgToApp *) &response, mod);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$SIP_REDIRECTED_CONTACTS") ==
			 0)
	{
	FILE           *fp;
	char            yStrFileName[128];

		sprintf (yStrFileName, "%s/sipMessage.%d.%d", DEFAULT_FILE_DIR,
				 gCall[lCall].appPid, zCall);

		if ((fp = fopen (yStrFileName, "w+")) == NULL)
		{
			dynVarLog (__LINE__, lCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR,
					   "Failed to open file (%s). [%d, %s] Unable to retreive $SIP_REDIRECTED_CONTACTS",
					   yStrFileName, errno, strerror (errno));

			response.returnCode = -1;
		}
		else
		{
			fprintf (fp, "%s", gCall[lCall].redirectedContacts);
			fclose (fp);
			sprintf (response.message, "%s", yStrFileName);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Writing SIP_REDIRECTED_CONTACTS = (%s).",
					   gCall[lCall].redirectedContacts);
		}

		//sprintf(response.message, "%s", gCall[zCall].sipMessage);

		response.opcode = gCall[lCall].msgToDM.opcode;
		response.appPid = gCall[lCall].msgToDM.appPid;
		response.appRef = gCall[lCall].msgToDM.appRef;
		response.appCallNum = gCall[lCall].msgToDM.appCallNum;
		response.appPassword = gCall[lCall].msgToDM.appPassword;
		response.returnCode = 0;

		writeGenericResponseToApp (__LINE__, lCall, (struct MsgToApp *) &response, mod);
	}
	else if (strcmp (yMsgGetGlobalString.name, "$INBOUND_CALL_PARM") == 0)
	{
	FILE           *fp;
	char            yStrFileName[128];
	char            yTempBuffer[512];

		yTempBuffer[0] = '\0';

		sprintf (yStrFileName, "%s/inboundCallParm.%d.%d", DEFAULT_FILE_DIR,
				 gCall[zCall].appPid, zCall);

		if ((fp = fopen (yStrFileName, "w+")) == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR,
					   "Failed to open file (%s). [%d, %s] Unable to retrieve $INBOUND_CALL_PARM.",
					   yStrFileName, errno, strerror (errno));

			response.returnCode = -1;
		}
		else
		{
	int             yIntTmpSamplingRate = 8000;

			switch (gCall[zCall].codecType)
			{
			case 18:
				yIntTmpSamplingRate = 1000;
				break;

			default:
				yIntTmpSamplingRate = 8000;
			}

			memset (gCall[zCall].GV_InboundCallParm, 0,
					sizeof (gCall[zCall].GV_InboundCallParm));

#if 1
			if (gCall[zCall].isCallAnswered == YES)
			{
				memset (gCall[zCall].call_type, 0, 50);
				sprintf (gCall[zCall].call_type, "%s", "Voice-Only");
			}
#endif

			sprintf (gCall[zCall].GV_InboundCallParm,
					 "ANI=%s"
					 "&DNIS=%s"
					 "&OCN=%s"
					 "&RDN=%s"
					 "&AUDIO_CODEC=%s"
					 "&FRAME_RATE=%d"
					 "&CALL_TYPE=%s"
					 "&AUDIO_SAMPLING_RATE=%d"
					 "&PRESENT_RESTRICT=%d",
					 gCall[zCall].ani,
					 gCall[zCall].dnis,
					 gCall[zCall].ocn,
					 gCall[zCall].rdn,
					 gCall[zCall].audioCodecString,
					 -1,
					 gCall[zCall].call_type,
					 yIntTmpSamplingRate, gCall[zCall].presentRestrict);

			memset (yTempBuffer, 0, sizeof (yTempBuffer));
			sprintf (yTempBuffer, "%s", gCall[zCall].GV_InboundCallParm);

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Writing INBOUND_CALL_PARM",
						  gCall[zCall].GV_InboundCallParm, zCall);

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Writing INBOUND_CALL_PARAM = (%s).", yTempBuffer);

			fprintf (fp, "%s", yTempBuffer);

			fclose (fp);

			sprintf (response.message, "%s", yStrFileName);
		}

		response.opcode = gCall[lCall].msgToDM.opcode;
		response.appPid = gCall[lCall].msgToDM.appPid;
		response.appRef = gCall[lCall].msgToDM.appRef;
		response.appCallNum = gCall[lCall].msgToDM.appCallNum;
		response.appPassword = gCall[lCall].msgToDM.appPassword;
		response.returnCode = 0;

		writeGenericResponseToApp (__LINE__, lCall, (struct MsgToApp *) &response, mod);

	}
	else
	{
		notifyMediaMgr (__LINE__, lCall, &(gCall[lCall].msgToDM), mod);
	}

	return yRc;

}								/*int */

int
process_DMOP_RECVFAX (int zCall, void *z)
{
	char            mod[] = { "process_DMOP_RECVFAX" };
	struct MsgToDM *ptrMsgToDM;
	struct Msg_RecvFax msg;
	struct MsgToApp response;
	int             yRc;

	time_t          yCurrentTime, yConnectTime;
	char            yStrDestination[500];
	char            yStrFromUrl[256];
	char            yStrLocalSdpPort[10];
	char            yStrFaxSdp[4000];

	osip_message_t *invite_t38 = NULL;

	gCall[zCall].isFaxCall = 1;

	msg = *(struct Msg_RecvFax *) z;

	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	sprintf (yStrDestination, "%s", gCall[zCall].originalAni);
	sprintf (yStrFromUrl, "sip:arc@%s", gHostIp);

	eXosip_lock (geXcontext);

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Sending Fax to ", yStrDestination,
				  zCall);

	if (gOutboundProxyRequired)
	{

	char            yStrTmpProxy[256];

		sprintf (yStrTmpProxy, "sip:%s", gOutboundProxy);

		yRc = eXosip_call_build_initial_invite (geXcontext, &invite_t38,
												yStrTmpProxy,
												yStrFromUrl,
												yStrDestination, "");
	}
	else
	{
		yRc = eXosip_call_build_initial_invite (geXcontext, &invite_t38,
												yStrDestination,
												yStrFromUrl,
												gCall[zCall].GV_RouteHeader,
												"");
	}

	if (yRc < 0)
	{
		response.returnCode = -1;
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);
		eXosip_unlock (geXcontext);
		return -1;
	}

	eXosip_unlock (geXcontext);

	sprintf (yStrLocalSdpPort, "%d", gCall[zCall].remoteRtpPort);

	yRc = createSdpBody (zCall, yStrLocalSdpPort, -1, -1, -1);

	osip_message_set_body (invite_t38, gCall[zCall].sdp_body_local,
						   strlen (gCall[zCall].sdp_body_local));
	osip_message_set_content_type (invite_t38, "application/sdp");

	if (gCall[zCall].remoteRtpPort > 0)
	{
		yRc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msg), mod);
	}

	return 0;
}

///This is the function we use to set global variables whenever we see a DMOP_SETGLOBAL.
int
process_DMOP_SETGLOBAL (int zCall)
{
	int             yRc = 1;
	char            mod[] = { "process_DMOP_SETGLOBAL" };

	struct MsgToApp response;

	struct Msg_SetGlobal yMsgSetGlobal;

	memcpy (&yMsgSetGlobal, &(gCall[zCall].msgToDM),
			sizeof (struct Msg_SetGlobal));

	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "%s [%s=%d]", mod, yMsgSetGlobal.name, yMsgSetGlobal.value);

	response.returnCode = 0;

	if (strcmp (yMsgSetGlobal.name, "$KILL_APP_GRACE_PERIOD") == 0)
	{
		gCall[zCall].GV_KillAppGracePeriod = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$PLAY_BACK_BEEP_INTERVAL") == 0)
	{
		gCall[zCall].GV_PlayBackBeepInterval = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$MAX_CALL_DURATION") == 0)
	{
		gCall[zCall].GV_CallDuration = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$MAX_PAUSE_TIME") == 0)
	{
		gCall[zCall].GV_MaxPauseTime = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$SR_DTMF_ONLY") == 0)
	{
		gCall[zCall].GV_SRDtmfOnly = (yMsgSetGlobal.value);
	}
	else if (strcmp (yMsgSetGlobal.name, "$CALL_PROGRESS") == 0)
	{
		gCall[zCall].GV_CallProgress = (yMsgSetGlobal.value);
#ifdef ACU_LINUX
		sprintf (gSTonesFifo, "%s/%s", gFifoDir, STONES_CP_FIFO);
		if (access (gSTonesFifo, F_OK) < 0)
		{
			gCall[zCall].GV_CallProgress = 0;
		}
#endif

	}
	else if (strcmp (yMsgSetGlobal.name, "$CANCEL_OUTBOUND_CALL") == 0)
	{
		yRc = 0;
		gCall[zCall].GV_CancelOutboundCall = (yMsgSetGlobal.value);
	struct MsgToApp response;

		response.opcode = gCall[zCall].msgToDM.opcode;
		response.appCallNum = gCall[zCall].msgToDM.appCallNum;
		response.appPid = gCall[zCall].msgToDM.appPid;
		response.appRef = gCall[zCall].msgToDM.appRef;
		response.returnCode = 0;

		writeGenericResponseToApp (__LINE__, zCall, &response, mod);
	}
	else
	{
#if 0
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE, ERR,
				   "Failed set global var, invalid name (%s).",
				   yMsgSetGlobal.name);

		response.returnCode = -1;
#endif
	}

	return yRc;

}								/*END: int process_DMOP_SETGLOBAL */

///This is the function we use to set global string variables whenever we see a DMOP_SETGLOBALSTRING.
int
process_DMOP_SETGLOBALSTRING (int zCall)
{
	char            mod[] = { "process_DMOP_SETGLOBALSTRING" };
	int             yRc = 0;
	int             sendResponseBack = 1;
	struct Msg_SetGlobalString yMsgSetGlobalString;

	memcpy (&yMsgSetGlobalString,
			&(gCall[zCall].msgToDM), sizeof (struct Msg_SetGlobalString));

	if (strcmp (yMsgSetGlobalString.name, "$SIP_PREFERRED_CODEC") == 0)
	{
		if (strcmp (yMsgSetGlobalString.value, "PCMU") == 0 ||
			strcmp (yMsgSetGlobalString.value, "G729") == 0 ||
			strcmp (yMsgSetGlobalString.value, "G722") == 0 ||
			strcmp (yMsgSetGlobalString.value, "PCMA") == 0 ||
			strcmp (yMsgSetGlobalString.value, "GSM") == 0 ||
			strcmp (yMsgSetGlobalString.value, "SPEEX") == 0 ||
			strcmp (yMsgSetGlobalString.value, "AMR") == 0)
		{
			memset (gCall[zCall].GV_PreferredCodec, 0,
					sizeof (gCall[zCall].GV_PreferredCodec));
			//memset(gPreferredCodec, 0, sizeof(gPreferredCodec));
			sprintf (gCall[zCall].GV_PreferredCodec, "%s",
					 yMsgSetGlobalString.value);

			//sprintf(gPreferredCodec, "%s",
			//  yMsgSetGlobalString.value);
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Set [%d].GV_PreferredCodec=(%s), gPreferredCodec=(%s).",
					   zCall, gCall[zCall].GV_PreferredCodec,
					   gPreferredCodec);
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PARM,
					   ERR,
					   "Failed to set SIP_PREFERRED_CODEC, Invalid SIP_PREFERRED_CODEC value %s, Dropping the call.",
					   yMsgSetGlobalString.value);
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE,
						   WARN,
						   "Sending SIP Message 415 Unsupported Media Type.");

				eXosip_lock (geXcontext);
				yRc = eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 415, NULL);
				eXosip_unlock (geXcontext);

				sprintf (gCall[zCall].GV_CustData2,
						 "AUDIO_CODEC=%s"
						 "&FRAME_RATE=%d",
						 gCall[zCall].audioCodecString,
						 -1);

				writeCDR ("process_CALL_NEW", zCall, "RC", 20014, -1);	//DDN_TODO: SNMP SetRequest

			}
	struct MsgToApp response;

			response.opcode = gCall[zCall].msgToDM.opcode;
			response.appCallNum = gCall[zCall].msgToDM.appCallNum;
			response.appPid = gCall[zCall].msgToDM.appPid;
			response.appRef = gCall[zCall].msgToDM.appRef;
			memset (response.message, 0, sizeof (response.message));
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			return -1;
		}

		yRc =
			parseSdpMessage (zCall, __LINE__, gCall[zCall].sdp_body_remote_new,
							 gCall[zCall].remote_sdp);
		if (yRc)
		{
			gCall[zCall].codecMissMatch = YES;
		}
		else
		{
			gCall[zCall].codecMissMatch = NO;
		}
	}
	else if (strcmp (yMsgSetGlobalString.name, "$SIP_AUTH_USER") == 0)
	{
		sprintf (gCall[zCall].GV_SipAuthenticationUser, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$SIP_AUTH_PASS") == 0)
	{
		sprintf (gCall[zCall].GV_SipAuthenticationPassword, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$SIP_AUTH_IDENTITY") == 0)
	{
		sprintf (gCall[zCall].GV_SipAuthenticationId, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$SIP_AUTH_REALM") == 0)
	{
		sprintf (gCall[zCall].GV_SipAuthenticationRealm, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$SIP_FROM") == 0)
	{
		sprintf (gCall[zCall].GV_SipFrom, "%s", yMsgSetGlobalString.value);
//		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//			"DJB: Set SIP_FROM to (%s)", gCall[zCall].GV_SipFrom);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$CALLING_PARTY") == 0)
	{

		sprintf (gCall[zCall].GV_CallingParty, "%s@%s:%d",    // BT-37
				 yMsgSetGlobalString.value, gHostIp, gSipPort);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$SIP_OUTBOUND_GATEWAY") == 0)
	{
		sprintf (gCall[zCall].GV_DefaultGateway, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strcmp (yMsgSetGlobalString.name, "$SIP_AS_ADDRESS") == 0)
	{
		sprintf (gCall[zCall].GV_SipAsAddress, "%s",
				 yMsgSetGlobalString.value);
	}
	else if (strncmp
			 (yMsgSetGlobalString.name, "$SIP_HEADER:",
			  strlen ("$SIP_HEADER:")) == 0)
	{
	char            tempVal[4096] = "";

		if (strstr (yMsgSetGlobalString.value, "FILE:") != 0)
		{
	FILE           *fp;
	char            yStrFileName[128];

			sprintf (yStrFileName, "%s", yMsgSetGlobalString.value);
			sscanf (yMsgSetGlobalString.value, "FILE:%s", yStrFileName);

			if ((fp = fopen (yStrFileName, "r")) == NULL)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
						   TEL_FILE_IO_ERROR, ERR,
						   "Failed to open file (%s), [%d, %s] Unable to set $SIP_HEADER.",
						   yStrFileName, errno, strerror (errno));
			}
			else
			{
				struct stat     lStat;

				stat (yStrFileName, &lStat);
				fread (tempVal, lStat.st_size, 1, fp);

				fclose (fp);

				arc_unlink (zCall, mod, yStrFileName);
			}
		}

		if (tempVal[0] == '\0')
		{
			sprintf (tempVal, "%s", yMsgSetGlobalString.value);
		}

		if (strcmp (yMsgSetGlobalString.name, "$SIP_HEADER:USER-TO-USER") == 0)
		{
			sprintf (gCall[zCall].transferAAI, "%s", tempVal);
		}
		else if (strcmp (yMsgSetGlobalString.name, "$SIP_HEADER:UUI") == 0)
		{
			sprintf (gCall[zCall].transferAAI, "%s", tempVal);
		}
		else if (strcmp (yMsgSetGlobalString.name, "$SIP_HEADER:ROUTE_HEADER")
				 == 0)
		{
			sprintf (gCall[zCall].GV_RouteHeader, "%s", tempVal);
		}
		else if (strcmp
				 (yMsgSetGlobalString.name,
				  "$SIP_HEADER:P_ASSERTED_IDENTITY") == 0)
		{
			if (tempVal[0] != '<')
			{
				sprintf (gCall[zCall].GV_PAssertedIdentity, "<%s>", tempVal);
			}
			else
			{
				sprintf (gCall[zCall].GV_PAssertedIdentity, "%s", tempVal);
			}
		}
		else if (strcmp (yMsgSetGlobalString.name, "$SIP_HEADER:CALL_INFO") ==
				 0)
		{
			if (tempVal[0] != '<')
			{
				sprintf (gCall[zCall].GV_CallInfo, "<%s>", tempVal);
			}
			else
			{
				sprintf (gCall[zCall].GV_CallInfo, "%s", tempVal);
			}
		}
		else if (strcmp (yMsgSetGlobalString.name, "$SIP_HEADER:DIVERSION") ==
				 0)
		{
			if (tempVal[0] != '<')
			{
				sprintf (gCall[zCall].GV_Diversion, "<%s>", tempVal);
			}
			else
			{
				sprintf (gCall[zCall].GV_Diversion, "%s", tempVal);
			}
		}
		else if (strcmp (yMsgSetGlobalString.name, "$SIP_HEADER:CALLER_NAME")
				 == 0)
		{
			sprintf (gCall[zCall].GV_CallerName, "%s", tempVal);
		}
	}
	else if (strcmp (yMsgSetGlobalString.name, "$TEL_TRANSFER_AAI") == 0)
	{
		sprintf (gCall[zCall].transferAAI, "%s", yMsgSetGlobalString.value);
	}
	else
	{
		notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
		sendResponseBack = 0;
	}

	if (sendResponseBack == 1)
	{

	struct MsgToApp response;

		response.opcode = gCall[zCall].msgToDM.opcode;
		response.appCallNum = gCall[zCall].msgToDM.appCallNum;
		response.appPid = gCall[zCall].msgToDM.appPid;
		response.appRef = gCall[zCall].msgToDM.appRef;
		response.returnCode = 0;
		memset (response.message, 0, sizeof (response.message));
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);
	}

	return (0);

}								/*END: int process_DMOP_SETGLOBALSTRING */

///The following function returns us a string representation for the opcode we receive on the requestFifo.
int
getOpcodeString (int zOpcode, char *zOut)
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
	case DMOP_BRIDGE_THIRD_LEG:
		sprintf (zOut, "%s", "DMOP_BRIDGE_THIRD_LEG");
		break;

	case DMOP_RESACT_STATUS:
		sprintf (zOut, "%s", "DMOP_RESACT_STATUS");
		break;

	case DMOP_INITIATECALL:
		sprintf (zOut, "%s", "DMOP_INITIATECALL");
		break;

	case DMOP_LISTENCALL:
		sprintf (zOut, "%s", "DMOP_LISTENCALL");
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

	case DMOP_PLAYMEDIA:
		sprintf (zOut, "%s", "DMOP_PLAYMEDIA");
		break;

	case DMOP_RECORDMEDIA:
		sprintf (zOut, "%s", "DMOP_RECORDMEDIA");
		break;

	case DMOP_PLAYMEDIAVIDEO:
		sprintf (zOut, "%s", "DMOP_PLAYMEDIAVIDEO");
		break;

	case DMOP_PLAYMEDIAAUDIO:
		sprintf (zOut, "%s", "DMOP_PLAYMEDIAAUDIO");
		break;

	case DMOP_SENDFAX:
		sprintf (zOut, "%s", "DMOP_SENDFAX");
		break;

	case DMOP_SRRECOGNIZEFROMCLIENT:
		sprintf (zOut, "%s", "DMOP_SRRECOGNIZEFROMCLIENT");
		break;

	case DMOP_SRRECOGNIZE:
		sprintf (zOut, "%s", "DMOP_SRRECOGNIZE");
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
		sprintf (zOut, "%s", "DMOP_REJECTCALL");
		break;

	case DMOP_DROPCALL:
		sprintf (zOut, "%s", "DMOP_DROPCALL");
		break;

	case DMOP_APPDIED:
		sprintf (zOut, "%s", "DMOP_APPDIED");
		break;

	case DMOP_SHUTDOWN:
		sprintf (zOut, "%s", "DMOP_SHUTDOWN");
		break;

	case DMOP_STARTOCSAPP:
		sprintf (zOut, "%s", "DMOP_STARTOCSAPP");
		break;

    case DMOP_STARTNTOCSAPP:
        sprintf (zOut, "%s", "DMOP_STARTNTOCSAPP");
        break;

	case DMOP_STARTFAXAPP:
		sprintf (zOut, "%s", "DMOP_STARTFAXAPP");
		break;

	case DMOP_REREGISTER:
		sprintf (zOut, "%s", "DMOP_REREGISTER");
		break;

	case DMOP_HEARTBEAT:
		sprintf (zOut, "%s", "DMOP_HEARTBEAT");
		break;

	case DMOP_SENDVFU:
		sprintf (zOut, "%s", "DMOP_SENDVFU");
		break;

	case DMOP_SPEAKMRCPTTS:
		sprintf (zOut, "%s", "DMOP_SPEAKMRCPTTS");
		break;

	case DMOP_TTS_COMPLETE_SUCCESS:
		sprintf (zOut, "%s", "DMOP_TTS_COMPLETE_SUCCESS");
		break;

	case DMOP_TTS_COMPLETE_FAILURE:
		sprintf (zOut, "%s", "DMOP_TTS_COMPLETE_FAILURE");
		break;

	case DMOP_TTS_COMPLETE_TIMEOUT:
		sprintf (zOut, "%s", "DMOP_TTS_COMPLETE_TIMEOUT");
		break;

	case DMOP_STOPREGISTER:
		sprintf (zOut, "%s", "DMOP_STOPREGISTER");
		break;

	case DMOP_BLASTDIAL:
		sprintf (zOut, "%s", "DMOP_BLASTDIAL");
		break;

#ifdef ACU_LINUX
	case DMOP_END_CALL_PROGRESS:
		sprintf (zOut, "%s", "DMOP_END_CALL_PROGRESS");
		break;

	case DMOP_START_CALL_PROGRESS_RESPONSE:
		sprintf (zOut, "%s", "DMOP_START_CALL_PROGRESS_RESPONSE");
		break;
#endif

	case DMOP_FAX_PROCEED:
		sprintf (zOut, "%s", "DMOP_FAX_PROCEED");
		break;

	case DMOP_FAX_COMPLETE:
		sprintf (zOut, "%s", "DMOP_FAX_COMPLETE");
		break;

	case DMOP_FAX_PLAYTONE:
		sprintf (zOut, "%s", "DMOP_FAX_PLAYTONE");
		break;

	case DMOP_FAX_STOPTONE:
		sprintf (zOut, "%s", "DMOP_FAX_STOPTONE:");
		break;

	case DMOP_CONFERENCE_START:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_START");
		break;
	case DMOP_CONFERENCE_ADD_USER:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_ADD_USER");
		break;
	case DMOP_CONFERENCE_REMOVE_USER:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_REMOVE_USER");
		break;
	case DMOP_CONFERENCE_STOP:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_STOP");
		break;
	case DMOP_CONFERENCE_PLAY_AUDIO:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_PLAY_AUDIO");
		break;
	case DMOP_CONFERENCE_START_RESPONSE:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_START_RESPONSE");
		break;
	case DMOP_CONFERENCE_ADD_USER_RESPONSE:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_ADD_USER_RESPONSE");
		break;
	case DMOP_CONFERENCE_REMOVE_USER_RESPONSE:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_REMOVE_USER_RESPONSE");
		break;
	case DMOP_CONFERENCE_STOP_RESPONSE:
		sprintf (zOut, "%s", "DMOP_CONFERENCE_STOP_RESPONSE");
		break;
	case DMOP_INSERTDTMF:
		sprintf (zOut, "%s", "DMOP_INSERTDTMF");
		break;

//#ifdef CALEA
	case DMOP_GETPORTDETAILFORANI:
		sprintf (zOut, "%s", "DMOP_GETPORTDETAILFORANI");
		break;
	case DMOP_STARTRECORDSILENTLY:
		sprintf (zOut, "%s", "DMOP_STARTRECORDSILENTLY");
		break;
	case DMOP_STOPRECORDSILENTLY:
		sprintf (zOut, "%s", "DMOP_STOPRECORDSILENTLY");
		break;
//#endif
	case DMOP_PORT_REQUEST_PERM:
		sprintf (zOut, "%s", "DMOP_PORT_REQUEST_PERM");
		break;

#ifdef VOICE_BIOMETRICS
    case DMOP_VERIFY_CONTINUOUS_SETUP:
        sprintf (zOut, "%s", "DMOP_VERIFY_CONTINUOUS_SETUP");
        break;
    case DMOP_VERIFY_CONTINUOUS_GET_RESULTS:
        sprintf (zOut, "%s", "DMOP_VERIFY_CONTINUOUS_GET_RESULTS");
        break;
#endif // VOICE_BIOMETRICS


	default:
		sprintf (zOut, "%s", "UNKNOWN");
	}

	return (0);

}								/*int getOpcodeString */

///This function is used to read requests send to Call Manager on the request fifo.
void           *
readAndProcessAppRequests (void *z)
{
	static char     mod[] = "readAndProcessAppRequests";
	int             rc, yRc;
	int             ts;
	int             zCall;
	char            yMsg[MAX_LOG_MSG_LENGTH];
    // this was added to catch a memory leak - joe 
    // I don't see the need to malloc this 
	struct MsgToDM  MToDM;
	struct MsgToDM *ptrMsgToDM = &MToDM;
	pthread_attr_t  thread_attr;
	char            yStrOpcode[256];
	int             yLastErrno = 0;
	int             ySleepTime = 10;
	struct MsgToDM  gDisconnectStruct = { DMOP_DISCONNECT, -1, -1, -1 };

	time_t          yTmpTime;
	time_t          yLastHeartBeat;
	int             yIntBeatTime = 0;

	rc = openRequestFifo ();
	if (rc < 0)
	{
		arc_exit (-1, "");
	}

	rc = pthread_attr_init (&thread_attr);
	if (rc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "Failed to intialize thread attributes. rc=%d. [%d, %s]"
				   " Exiting.", rc, errno, strerror (errno));

		sleep (EXIT_SLEEP);
		arc_exit (-1, "");
	}

	rc = pthread_attr_setdetachstate (&thread_attr, PTHREAD_CREATE_DETACHED);
	if (rc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_BASE, ERR,
				   "Failed to set thread detach state. rc=%d. [%d, %s]"
				   "Exiting.", rc, errno, strerror (errno));

		sleep (EXIT_SLEEP);
		arc_exit (-1, "");
	}

	time (&yLastHeartBeat);

	dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, INFO,
			   "Waiting for app requests. time(%d)", yLastHeartBeat);

	while (gCanReadRequestFifo)
	{

		ySleepTime = 10;
		if (yLastErrno == 0)
		{
			ySleepTime = 1;
		}


		time (&yTmpTime);

		usleep (ySleepTime * 1000);

		if (yTmpTime - yLastHeartBeat >= 100)
		{

			dynVarLog (__LINE__, -1, mod, REPORT_NORMAL,
					   TEL_PROCESS_NOT_FOUND, ERR,
					   "Media Manager(%d, pid %d) has been down for (%d) seconds. Exiting.",
					   gDynMgrId, gMediaMgrPid, (yTmpTime - yLastHeartBeat));

			gCanReadSipEvents = 0;
			gCanReadRequestFifo = 0;
			gMediaMgrPid = -1;
		}

		//ptrMsgToDM = (struct MsgToDM *) osip_malloc (sizeof (struct MsgToDM));

		/*
		 * Read the next API statement from the fifo 
		 */

		rc = read (gRequestFifoFd, ptrMsgToDM, sizeof (struct MsgToDM));
		if (rc == -1)
		{
			yLastErrno = errno;
			if (errno == EAGAIN)
			{
				continue;
			}

			dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR, "Failed to read request from (%s). [%d, %s]",
					   gRequestFifo, errno, strerror (errno));

			continue;
		}

		yLastErrno = 0;

		zCall = ptrMsgToDM->appCallNum;

		getOpcodeString (ptrMsgToDM->opcode, yStrOpcode);

		if (ptrMsgToDM->opcode != DMOP_HEARTBEAT)
//       if(1)
		{

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Received %d bytes. msg={op:%d(%s),c#:%d,pid:%d,ref:%d,pw:%d}",
					   rc,
					   ptrMsgToDM->opcode,
					   yStrOpcode,
					   ptrMsgToDM->appCallNum,
					   ptrMsgToDM->appPid,
					   ptrMsgToDM->appRef, ptrMsgToDM->appPassword);
		}
		else
		{
			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Got Media Manager HEARTBEAT",
						  "", gDynMgrId);
		}

//		if (ptrMsgToDM->opcode == DMOP_INITIATECALL)
//		{
//	struct Msg_InitiateCall *yTmpInitiateCall =
//		(struct Msg_InitiateCall *) ptrMsgToDM;
//
//			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//					   "TEL_Initiate/TEL_BridgeCall details: "
//					   "num1(%d), num2(%d), whichOne(%d), phone(%s), ip(%s),"
//					   "ani(%s),"
//					   "informat(%d),"
//					   "listenType(%d),"
//					   "resource(%s).",
//					   yTmpInitiateCall->appCallNum1,
//					   yTmpInitiateCall->appCallNum2,
//					   yTmpInitiateCall->whichOne,
//					   yTmpInitiateCall->phoneNumber,
//					   yTmpInitiateCall->ipAddress,
//					   yTmpInitiateCall->ani,
//					   yTmpInitiateCall->informat,
//					   yTmpInitiateCall->listenType,
//					   yTmpInitiateCall->resourceType);
//		}
//
		if (ptrMsgToDM->opcode == DMOP_PORT_REQUEST_PERM)
		{
			rc = reserveStaticPorts ((struct Msg_ReservePort *) ptrMsgToDM);
			gotStaticPortRequest = 1;

			while (isDynMgrReady == 0)
			{
				sleep (1);
			}

			continue;
		}
		else if (ptrMsgToDM->opcode == DMOP_RTPPORTDETAIL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
					   "Processing DMOP_RTPPORTDETAIL.");
			process_DMOP_RTPPORTDETAIL ();

			continue;
		}
//#ifdef CALEA
		else if (ptrMsgToDM->opcode == DMOP_GETPORTDETAILFORANI)
		{

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Processing DMOP_GETPORTDETAILFORANI.");
			if (canContinue (mod, zCall))
			{
				process_DMOP_GETPORTDETAILFORANI (ptrMsgToDM->data);
			}
			continue;
		}
		else if (ptrMsgToDM->opcode == DMOP_STARTRECORDSILENTLY)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Processing DMOP_STARTRECORDSILENTLY.");

			if (canContinue (mod, zCall))
			{
				notifyMediaMgr (__LINE__, zCall, ptrMsgToDM, mod);
			}

			continue;
		}
		else if (ptrMsgToDM->opcode == DMOP_BRIDGE_THIRD_LEG)
		{

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Processing DMOP_BRIDGE_THIRD_LEG.");
		}
		else if (ptrMsgToDM->opcode == DMOP_STOPRECORDSILENTLY)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Processing DMOP_STOPRECORDSILENTLY.");

			notifyMediaMgr (__LINE__, zCall, ptrMsgToDM, mod);

			continue;
		}
//#endif
		else if (ptrMsgToDM->opcode == DMOP_SHUTDOWN)	/*DMOP_SHUTDOWN from ArcSipRedirector */
		{

			gShutDownFromRedirector = 1;
            // added  Fri Oct 30 20:53:24 EDT 2015 JOES to signal shutdown 
			gShutDownFlag = 1;
			if ( gShutDownFlagTime == 0 )
			{
				time (&gShutDownFlagTime);
			}

			registerToRedirector((char *)__func__, UNREG_FROM_REDIRECTOR);
			continue;
		}
		else if (ptrMsgToDM->opcode == DMOP_REREGISTER)
		{
			if (gSipRedirection)
			{
				//registerToRedirector(mod);
				struct timeval	yTmpTimeb;
			//	struct timeb    yTmpTimeb;

			//	ftime (&yTmpTimeb);
				gettimeofday(&yTmpTimeb, NULL);

				// yTmpTimeb.time += 2;
				yTmpTimeb.tv_sec += 20;

				yRc = addToTimerList (__LINE__, zCall,
									  MP_OPCODE_REREGISTER,
									  gCall[0].msgToDM,
									  yTmpTimeb, -1, -1, -1);
			}

			continue;
		}
		else if (ptrMsgToDM->opcode == DMOP_STOPREGISTER)
		{
			gShutdownRegistrations = 1;
			continue;
		}
		else if (ptrMsgToDM->opcode == DMOP_HEARTBEAT)
		{
			gReceivedHeartBeat = 1;
			time (&yLastHeartBeat);
			continue;
		}
		else if (zCall < gStartPort || zCall > gEndPort)
		{
			continue;
		}

		//memcpy (&(gCall[zCall].msgToDM), ptrMsgToDM, sizeof (struct MsgToDM));


		rc = validateAppRequestParameters (mod, ptrMsgToDM);
		if (rc != 0)
		{
			/*
			 * Error message written in subroutine 
			 */

	struct MsgToApp msgToApp;

			msgToApp.opcode = ptrMsgToDM->opcode;
			msgToApp.appPid = ptrMsgToDM->appPid;
			msgToApp.appRef = ptrMsgToDM->appRef;
			msgToApp.appCallNum = ptrMsgToDM->appCallNum;
			msgToApp.appPassword = ptrMsgToDM->appPassword;
			msgToApp.returnCode = -1;

			writeGenericResponseToApp (__LINE__, zCall,
									   (struct MsgToApp *) &msgToApp, mod);

			continue;
		}

		memcpy (&(gCall[zCall].msgToDM), ptrMsgToDM, sizeof (struct MsgToDM));


		switch (gCall[zCall].msgToDM.opcode)
		{
		case DMOP_PORT_REQUEST_PERM:
			{
				rc = reserveStaticPorts ((struct Msg_ReservePort *)
										 &(gCall[zCall].msgToDM));
				gotStaticPortRequest = 1;

				while (isDynMgrReady == 0)
				{
					sleep (1);
				}
				if (rc != 0)
				{
					continue;
				}
			}
			break;

		case DMOP_TRANSFERCALL:
			{
				rc = process_DMOP_TRANSFERCALL (zCall);
			}
			break;

		case DMOP_RESACT_STATUS:
			{
				rc = process_DMOP_RESACT_STATUS (zCall,
												 &(gCall[zCall].msgToDM));
			}
			break;

		case DMOP_INITIATECALL:
			{
	struct Msg_InitiateCall *yTmpInitiateCall =
		(struct Msg_InitiateCall *) &(gCall[zCall].msgToDM);

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "TEL_Initiate/TEL_BridgeCall details: "
						   "num1(%d), num2(%d), phone(%s), numRings(%d), whichOne(%d), "
						   "ip(%s), ani(%s), informat(%d),"
						   "listenType(%d), resource(%s),",
						   yTmpInitiateCall->appCallNum1,
						   yTmpInitiateCall->appCallNum2,
						   yTmpInitiateCall->phoneNumber,
						   yTmpInitiateCall->numRings,
						   yTmpInitiateCall->whichOne,
						   yTmpInitiateCall->ipAddress, yTmpInitiateCall->ani,
						   yTmpInitiateCall->informat,
						   yTmpInitiateCall->listenType,
						   yTmpInitiateCall->resourceType);

				rc = process_DMOP_INITIATECALL (zCall);
			}
			break;

		case DMOP_LISTENCALL:
			{
	struct Msg_ListenCall *yTmpListenCall =
		(struct Msg_ListenCall *) &(gCall[zCall].msgToDM);

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "TEL_ListenCall details: " "party(%d)," "num1(%d),"
						   "num2(%d)," "phone(%s)," "ip(%s)," "ani(%s),"
						   "informat(%d)," "listenType(%d)," "resource(%s),",
						   yTmpListenCall->appCallNum1,
						   yTmpListenCall->appCallNum2,
						   yTmpListenCall->whichOne,
						   yTmpListenCall->phoneNumber,
						   yTmpListenCall->ipAddress, yTmpListenCall->ani,
						   yTmpListenCall->informat,
						   yTmpListenCall->listenType,
						   yTmpListenCall->resourceType);

				rc = process_DMOP_LISTENCALL (zCall);
			}
			break;
		case DMOP_BLASTDIAL:
			{
				rc = process_DMOP_BLASTDIAL (zCall);
			}
			break;

		case DMOP_CONFERENCE_START:
			{
	MSG_CONF_START *yTmpConferenceStart =
		(MSG_CONF_START *) & (gCall[zCall].msgToDM);

				rc = process_DMOP_CONFERENCESTART (zCall);
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_CONFERENCE_ADD_USER:
			{
	MSG_CONF_ADD_USER *yTmpConfAddUser =
		(MSG_CONF_ADD_USER *) & (gCall[zCall].msgToDM);

				sprintf (gCall[zCall].confID, "%s", yTmpConfAddUser->confId);

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Inside DMOP_CONFERENCE_ADD_USER confId=%s",
						   gCall[zCall].confID);

				for (int i = 0; i < MAX_PORTS; i++)
				{
					if (strcmp (gCall[zCall].confID, gCall[i].confID) == 0)
					{
						if (gCall[i].confUserCount < MAX_CONF_PORTS)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "CONF: confID=%s, portnum=%d, confUserCount=%d.",
									   gCall[i].confID, i,
									   gCall[i].confUserCount);
							gCall[i].conf_ports[gCall[i].confUserCount] =
								zCall;
							gCall[i].confUserCount++;
							gCall[zCall].confUserCount =
								gCall[i].confUserCount;
						}
					}
				}

				rc = process_DMOP_CONFERENCEADDUSER (zCall);
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_CONFERENCE_PLAY_AUDIO:
			{
	MSG_CONF_PLAY_AUDIO *yTmpConfPlayAudio =
		(MSG_CONF_PLAY_AUDIO *) & (gCall[zCall].msgToDM);

				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_INSERTDTMF:
			{
	MSG_INSERT_DTMF *yTmpInsertDtmf =
		(MSG_INSERT_DTMF *) & (gCall[zCall].msgToDM);

				if (yTmpInsertDtmf->portNum == -1)
				{
					for (int i = 0; i < MAX_PORTS; i++)
					{
						if (!canContinue (mod, i))
						{
							continue;
						}
						else if (strcmp
								 (yTmpInsertDtmf->confId,
								  gCall[i].confID) == 0 && i != zCall)
						{
	MSG_INSERT_DTMF tempMsgToDM;

							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "DMOP_INSERTDTMF for port(%d).", i);

							tempMsgToDM.portNum = i;
							tempMsgToDM.appCallNum = i;
							tempMsgToDM.opcode = DMOP_INSERTDTMF;
							tempMsgToDM.appPid = gCall[i].msgToDM.appPid;
							tempMsgToDM.appRef = gCall[i].msgToDM.appRef;
							sprintf (tempMsgToDM.confId, "%s",
									 gCall[i].confID);
							sprintf (tempMsgToDM.dtmf, "%s",
									 yTmpInsertDtmf->dtmf);
							tempMsgToDM.sendResp = 0;

							notifyMediaMgr (__LINE__, i, (MsgToDM *) & (tempMsgToDM),
											mod);

						}
					}
	struct MsgToApp response;

					response.opcode = DMOP_INSERTDTMF;
					response.appCallNum = yTmpInsertDtmf->appCallNum;
					response.appPid = gCall[zCall].msgToDM.appPid;
					response.appRef = gCall[zCall].msgToDM.appRef;
					response.returnCode = 0;
					sprintf (response.message, "%s", "\0");

					writeGenericResponseToApp (__LINE__, zCall, &response, mod);
				}
				else
				{
					yTmpInsertDtmf->sendResp = 1;
					notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
				}

			}
			break;

		case DMOP_CONFERENCE_SEND_DATA:
			{
	MSG_CONF_SEND_DATA *yTmpConfSendData =
		(MSG_CONF_SEND_DATA *) & (gCall[zCall].msgToDM);

				for (int i = 0; i < MAX_PORTS; i++)
				{
					if (!canContinue (mod, zCall))
					{
						continue;
					}
					else if (strcmp
							 (yTmpConfSendData->confId, gCall[i].confID) == 0)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Message(%s) for port(%d).",
								   yTmpConfSendData->confOpData, i);

	struct MsgToApp response;

						response.opcode = DMOP_CONFERENCE_SEND_DATA;
						response.appCallNum = i;
						response.appPid = gCall[i].msgToDM.appPid;
						response.appRef = gCall[i].msgToDM.appRef;
						response.returnCode = 0;

						sprintf (response.message, "%s",
								 yTmpConfSendData->confOpData);

						writeGenericResponseToApp (__LINE__, i, &response, mod);

					}
				}

			}
			break;

		case DMOP_CONFERENCE_REMOVE_USER:
			{
	MSG_CONF_REMOVE_USER *yTmpConfRemoveUser =
		(MSG_CONF_REMOVE_USER *) & (gCall[zCall].msgToDM);

				for (int i = 0; i < MAX_PORTS; i++)
				{
					if (strcmp (gCall[zCall].confID, gCall[i].confID) == 0)
					{
						if (gCall[i].confUserCount < MAX_CONF_PORTS)
						{
							gCall[i].conf_ports[gCall[i].confUserCount] = -1;
							gCall[i].confUserCount--;
							if (gCall[i].confUserCount < 0)
							{
								gCall[i].confUserCount = 0;
							}
						}
					}
				}

				gCall[zCall].confUserCount = 0;

				memset (gCall[zCall].confID, 0, sizeof (gCall[zCall].confID));

				rc = process_DMOP_CONFERENCEREMOVEUSER (zCall);
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_CONFERENCE_STOP:
			{
				rc = process_DMOP_CONFERENCESTOP (zCall);
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_CONFERENCE_START_RESPONSE:
			{
	MSG_CONF_START_RESPONSE *yTmpConferenceStartResponse =
		(MSG_CONF_START_RESPONSE *) & (gCall[zCall].msgToDM);

				rc = process_DMOP_CONFERENCESTART_RESPONSE (zCall);
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_CONFERENCE_ADD_USER_RESPONSE:
			{
	MSG_CONF_ADD_USER_RESPONSE *yTmpConfAddUserResponse =
		(MSG_CONF_ADD_USER_RESPONSE *) & (gCall[zCall].msgToDM);

				rc = process_DMOP_CONFERENCEADDUSER_RESPONSE (zCall);
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_CONFERENCE_REMOVE_USER_RESPONSE:
			{
	MSG_CONF_REMOVE_USER_RESPONSE *yTmpConfRemoveUserResponse =
		(MSG_CONF_REMOVE_USER_RESPONSE *) & (gCall[zCall].msgToDM);

				rc = process_DMOP_CONFERENCEREMOVEUSER_RESPONSE (zCall);
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_CONFERENCE_STOP_RESPONSE:
			{
	MSG_CONF_STOP_RESPONSE *yTmpConferenceStopResponse =
		(MSG_CONF_STOP_RESPONSE *) & (gCall[zCall].msgToDM);

				gCall[zCall].msgToDM.appPid = gCall[zCall].appPid;

				rc = process_DMOP_CONFERENCESTOP_RESPONSE (zCall);
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_BRIDGECONNECT:
			{
				rc = process_DMOP_BRIDGECONNECT (zCall);
			}
			break;

		case DMOP_BRIDGE_THIRD_LEG:
			{
				rc = process_DMOP_BRIDGE_THIRD_LEG (zCall);
				if (rc != 0)
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, DYN_BASE,
							   ERR,
							   " process_DMOP_BRIDGE_THIRD_LEG(zCall=%d) returned error: rc = %d",
							   zCall, rc);
				}
			}
			break;

		case DMOP_ANSWERCALL:
			{
		    	if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED                        	// BT-98
		                 || gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
		                 || gCall[zCall].callState == CALL_STATE_CALL_TERMINATE_CALLED
		                 || gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
			    {   
					struct MsgToApp response;

            		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_SIP_SIGNALING, INFO,
	                    "Call was cancelled by the remote end.  No attempt to send answer will be made.  tid=%d.  "
					    "Not starting answerCall thread.", gCall[zCall].tid);

					response.opcode = gCall[zCall].msgToDM.opcode;
					response.appCallNum = gCall[zCall].msgToDM.appCallNum;
					response.appPid = gCall[zCall].msgToDM.appPid;
					response.appRef = gCall[zCall].msgToDM.appRef;
					response.returnCode = -1;
					response.appPassword = gCall[zCall].msgToDM.appPassword;

        			writeGenericResponseToApp (__LINE__, zCall, &response, mod);

					clearPort (zCall);
					break;
				}

				rc = pthread_create (&gCall[zCall].lastTimeThreadId,
									 &thread_attr, answerCallThread,
									 (void *) &(gCall[zCall].msgToDM));

				if (rc != 0)
				{
					processThreadCreationFailure (mod,
												  "answerCallThread",
												  &(gCall[zCall].msgToDM),
												  rc);
				}
			}
			break;

		case DMOP_STARTOCSAPP:
			{
				  rc = process_DMOP_STARTOCSAPP (zCall);
			}
			break;

		case DMOP_STARTNTOCSAPP:
            {
                  rc = process_DMOP_STARTNTOCSAPP (zCall);
            }
            break;

		case DMOP_STARTFAXAPP:
			{
			rc = process_DMOP_STARTFAXAPP (zCall);
			}
			break;

		case DMOP_INITTELECOM:
			{
			rc = process_DMOP_INITTELECOM (zCall);
			//notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_RECORD:
			{
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
				break;
			}
		case DMOP_SENDVFU:
			{
				process_DMOP_SENDVFU (zCall, &(gCall[zCall].msgToDM));
				break;
			}

		case DMOP_RECORDMEDIA:
			{
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
				break;
			}

		case DMOP_PLAYMEDIA:
			{
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_PLAYMEDIAVIDEO:
		case DMOP_PLAYMEDIAAUDIO:
		case DMOP_SPEAKMRCPTTS:
		case DMOP_TTS_COMPLETE_SUCCESS:
		case DMOP_TTS_COMPLETE_FAILURE:
		case DMOP_TTS_COMPLETE_TIMEOUT:
			{
				if (gCall[zCall].msgToDM.opcode == DMOP_SPEAKMRCPTTS)
				{
	struct Msg_SpeakMrcpTts *lpSpeak =
		(struct Msg_SpeakMrcpTts *) &(gCall[zCall].msgToDM);
				}

				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

			/*Not currently used in SIPIVR */
		case DMOP_RECVFAX:
			{
				//rc = process_DMOP_RECVFAX (zCall, &(gCall[zCall].msgToDM));
				gCall[zCall].faxGotAPI = 1;
				gCall[zCall].faxData.isFaxCall = 1;

				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);

				if (gCall[zCall].faxAlreadyGotReinvite)
				{
					gCall[zCall].sendFaxReinvite = 0;
				}
				else
				{
					gCall[zCall].sendFaxReinvite = 1;
				}

				if (gCall[zCall].faxServerSpecialFifo[0] == 0 &&
					gCall[zCall].faxDelayTimerStarted == 0)
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE,
							   INFO, "Adding to 4s timer.");
	//struct timeb    yTmpTimeb;
	struct timeval	yTmpTimeb;

					gCall[zCall].faxDelayTimerStarted = 1;
					gettimeofday(&yTmpTimeb, NULL);
				//	ftime (&yTmpTimeb);
					yTmpTimeb.tv_sec += 4;
					yRc = addToTimerList (__LINE__, zCall,
										  MP_OPCODE_CHECK_FAX_DELAY,
										  gCall[zCall].msgToDM,
										  yTmpTimeb, -1, -1, -1);
				}
			}
			break;
			/*END: Not currently used in SIPIVR */

		case DMOP_SPEAK:
			{
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_FAX_PLAYTONE:
			{
	struct Msg_Fax_PlayTone *lpPlayTone =
		(struct Msg_Fax_PlayTone *) &(gCall[zCall].msgToDM);
				if (lpPlayTone->sendOrRecv == 0)
				{
					arc_fax_playtone (zCall, ARC_FAX_TONE_CNG);	// send
				}
				else
				{
					arc_fax_playtone (zCall, ARC_FAX_TONE_CED);	// recv
				}
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_SENDFAX:
			{
				gCall[zCall].faxGotAPI = 1;
				if (gCall[zCall].faxData.isFaxCall == 0)
				{
					gCall[zCall].faxData.isFaxCall = 1;

					if (gCall[zCall].faxAlreadyGotReinvite)
					{
						gCall[zCall].sendFaxReinvite = 0;
					}
					else
					{
						gCall[zCall].sendFaxReinvite = 1;
					}
				}

				if (gCall[zCall].isFaxReserverResourceCalled == 1 &&
					gCall[zCall].faxRtpPort == -1)
				{
					gCall[zCall].sendFaxOnProceed = 1;
					memcpy (&(gCall[zCall].msgSendFax),
							&(gCall[zCall].msgToDM), sizeof (struct MsgToDM));
				}
				else
				{
					struct timeval	yTmpTimeb;
					gettimeofday(&yTmpTimeb, NULL);

					gCall[zCall].sendFaxOnProceed = 0;
					notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE,
							   INFO,
							   "Got DMOP_SENDFAX, .faxServerSpecialFifo=%s.",
							   gCall[zCall].faxServerSpecialFifo);

					if (gCall[zCall].faxServerSpecialFifo[0] == 0 &&
						gCall[zCall].faxDelayTimerStarted == 0)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO, "Adding to 4s timer.");

						gCall[zCall].faxDelayTimerStarted = 1;
					//	ftime (&yTmpTimeb);
						gettimeofday(&yTmpTimeb, NULL);
						yTmpTimeb.tv_sec += 4;
						yRc = addToTimerList (__LINE__, zCall,
											  MP_OPCODE_CHECK_FAX_DELAY,
											  gCall[zCall].msgToDM,
											  yTmpTimeb, -1, -1, -1);
					}
				}
			}
			break;
			/*END: Not currently used in SIPIVR */

		case DMOP_STOPACTIVITY:
			{
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_DROPCALL:
			{
                removeFromTimerList (zCall, MP_OPCODE_SESSION_EXPIRES_TERMINATE_APP);
				if (gCall[zCall].callState != CALL_STATE_CALL_CLOSED
					&& gCall[zCall].callState != CALL_STATE_CALL_CANCELLED
					&& gCall[zCall].callState != CALL_STATE_CALL_RELEASED)
				{

	struct Msg_DropCall msgDropCall;
					memcpy (&msgDropCall, &(gCall[zCall].msgToDM),
							sizeof (struct Msg_DropCall));

					if (msgDropCall.allParties == 2
						&& gCall[zCall].crossPort >= 0
						&& (gCall[zCall].callState == CALL_STATE_CALL_BRIDGED
							|| gCall[zCall].callSubState ==
							CALL_STATE_CALL_MEDIACONNECTED)
						&& gCall[zCall].leg == LEG_A)
					{
	int             yBLegPort = gCall[zCall].crossPort;

						setCallState (yBLegPort, CALL_STATE_CALL_CLOSED);
						//gCall[yBLegPort].callState = CALL_STATE_CALL_CLOSED;

						gDisconnectStruct.appCallNum = yBLegPort;
						gDisconnectStruct.opcode = DMOP_DISCONNECT;
						gDisconnectStruct.appPid = gCall[zCall].appPid;
						gDisconnectStruct.appPassword =
							gCall[yBLegPort].appPassword;

						notifyMediaMgr (__LINE__, yBLegPort,
										(struct MsgToDM *) &gDisconnectStruct,
										mod);

						gDisconnectStruct.appCallNum = -1;
						gDisconnectStruct.appPid = -1;
						gDisconnectStruct.appPassword = -1;

						__DDNDEBUG__ (DEBUG_MODULE_CALL,
									  "Terminating the call on B leg", "",
									  yBLegPort);

						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Sending SIP Message BYE.");

						eXosip_lock (geXcontext);
						eXosip_call_terminate (geXcontext, gCall[yBLegPort].cid,
											   gCall[yBLegPort].did);
						time (&(gCall[yBLegPort].lastReleaseTime));
						eXosip_unlock (geXcontext);

						gCall[yBLegPort].prevAppPid = -1;

						break;
					}

					if (msgDropCall.allParties == 1)
					{
						if (gCall[zCall].crossPort >= 0 &&
							(gCall[zCall].callState == CALL_STATE_CALL_BRIDGED
							 || gCall[zCall].callSubState ==
							 CALL_STATE_CALL_MEDIACONNECTED)
							&& gCall[zCall].leg == LEG_A)
						{
	int             yBLegPort = gCall[zCall].crossPort;

							setCallState (yBLegPort, CALL_STATE_CALL_CLOSED);
							//gCall[yBLegPort].callState = CALL_STATE_CALL_CLOSED;

							gDisconnectStruct.appCallNum = yBLegPort;
							gDisconnectStruct.opcode = DMOP_DISCONNECT;
							gDisconnectStruct.appPid = gCall[zCall].appPid;
							gDisconnectStruct.appPassword =
								gCall[zCall].appPassword;
							snprintf (gDisconnectStruct.data,
									  sizeof (gDisconnectStruct.data), "%s",
									  "false");

							notifyMediaMgr (__LINE__, yBLegPort,
											(struct MsgToDM *)
											&gDisconnectStruct, mod);

							gDisconnectStruct.appCallNum = -1;
							gDisconnectStruct.appPid = -1;
							gDisconnectStruct.appPassword = -1;
							gDisconnectStruct.data[0] = '\0';

							writeToRespShm (__LINE__, mod, tran_tabl_ptr, yBLegPort,
											APPL_API, 0);
							writeToRespShm (__LINE__, mod, tran_tabl_ptr, yBLegPort,
											APPL_STATUS, STATUS_IDLE);
							writeToRespShm (__LINE__, mod, tran_tabl_ptr, yBLegPort,
											APPL_PID, getpid ());
							updateAppName (__LINE__, yBLegPort, yBLegPort);

							__DDNDEBUG__ (DEBUG_MODULE_CALL,
										  "Terminating the call on B leg", "",
										  yBLegPort);

							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Sending SIP Message BYE.");

							eXosip_lock (geXcontext);
							eXosip_call_terminate (geXcontext, gCall[yBLegPort].cid,
												   gCall[yBLegPort].did);
							time (&(gCall[yBLegPort].lastReleaseTime));
							eXosip_unlock (geXcontext);

							gCall[yBLegPort].prevAppPid = -1;
						}
					}

					dropCall (mod, zCall, 0, &(gCall[zCall].msgToDM));

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Got DROPCALL notifying Media Manger");
					notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);

					if (gCall[zCall].leg == LEG_B)
					{
						gCall[zCall].prevAppPid = -1;
						gCall[zCall].appPid = -1;
					}
				}
			}
			break;

		case DMOP_REJECTCALL:
			{
	struct Msg_RejectCall *pMsgRejectCall;
	struct MsgToApp response;
	int             yIntCauseCode = 0;
	osip_message_t *answer = NULL;

				response.opcode = gCall[zCall].msgToDM.opcode;
				response.appCallNum = gCall[zCall].msgToDM.appCallNum;
				response.appPid = gCall[zCall].msgToDM.appPid;
				response.appRef = gCall[zCall].msgToDM.appRef;
				response.returnCode = 0;

				writeGenericResponseToApp (__LINE__, zCall, &response, mod);

				pMsgRejectCall =
					(struct Msg_RejectCall *) &(gCall[zCall].msgToDM);

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "Call=%d, dnis=(%s), ani=(%s). Rejecting the call w/ %s",
						   pMsgRejectCall->appCallNum, gCall[zCall].dnis,
						   gCall[zCall].ani, pMsgRejectCall->causeCode);

				//writeCDR (mod, zCall, "RC", 20014, -1);       //DDN_TODO: SNMP SetRequest

				/*  
				 *  We have already sent RTP details to media mgr, 
				 *  To cleanup that data, notify again
				 */

				mapArcCauseCodeToSipError (pMsgRejectCall->causeCode,
										   &yIntCauseCode);

				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);

				if (gCall[zCall].callState != CALL_STATE_CALL_CLOSED &&
					gCall[zCall].callState != CALL_STATE_CALL_CANCELLED &&
					gCall[zCall].callState != CALL_STATE_CALL_RELEASED)
				{

#if	 0
					eXosip_lock (geXcontext);
					yRc =
						eXosip_call_build_answer (geXcontext, gCall[zCall].tid,
												  yIntCauseCode, &answer);
					eXosip_unlock (geXcontext);
#endif
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Sending SIP Message %d.",
							   yIntCauseCode);

					eXosip_lock (geXcontext);
					yRc =
						eXosip_call_send_answer (geXcontext, gCall[zCall].tid,
												 yIntCauseCode, NULL);
					eXosip_unlock (geXcontext);

#if	 0
					eXosip_lock (geXcontext);
					eXosip_answer_call (gCall[zCall].did, yIntCauseCode, 0);
					eXosip_unlock (geXcontext);
#endif
				}
			}
			break;

		case DMOP_CANTFIREAPP:
			{
	struct Msg_CantFireApp *pCantFireAppMsg;
// MR #4325 
	int						whyCantFireApp = 404;
// END: MR #4325

				pCantFireAppMsg =
					(struct Msg_CantFireApp *) &(gCall[zCall].msgToDM);

// MR #4325 
				//changed by Murali on 210526
				//changed the returnCode of -2 to for Maximum instance reached
				//changed the returnCode of -3 to for Maximum licensed instances exceeds
				//if ( pCantFireAppMsg->returnCode == -2 )
				if ( pCantFireAppMsg->returnCode == -3 )
				{
					whyCantFireApp = gSipOutOfLicenseResponse;
				}
// END: MR #4325
				
	/*
	 * Changed by Murali on 20210517 for the Nagios alert 
	   (CANTFIREAPP - ArcIPResp could not fire the app. Rejecting the call.)
	 * Madhav wants to log the error with detail message
	 * pCantFireAppMsg->returnCode == -1 //find_application failed -  No such entry in scheduling table.
	 * pCantFireAppMsg->returnCode == -2 // no more licenses
	 * pCantFireAppMsg->returnCode == -7 //launchApp failed - Can't start application xxxx, [2, No such file or directory] - scheduling table is good but application could not find in Te dir.
	 */
				if( pCantFireAppMsg->returnCode == -1 ) //find_application failed -  No such entry in scheduling table.
				{
					sprintf( yMsg, "No such entry in scheduling table for DNIS <%s>.", pCantFireAppMsg->dnis);
				}
				else if( pCantFireAppMsg->returnCode == -2 ) // no more licenses
				{
					sprintf( yMsg, "no more licenses.");
				}
				else if( pCantFireAppMsg->returnCode == -7 ) //file not found
				{                     //launchApp failed - Can't start application xxxx, [2, No such file or directory] - scheduling table is good but application not found in Te dir.
					sprintf( yMsg, "LaunchApp failed: application %s not found.", pCantFireAppMsg->data);
				}
				else
				{
					sprintf( yMsg, "check schedule file and application");
				}
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_CANT_FIRE_APP, ERR,
							"dnis=(%s), ani=(%s), returnCode=(%d,%d). "
							"ArcIPResp could not fire the app. Rejecting the call. (%s)",
							pCantFireAppMsg->dnis,
							pCantFireAppMsg->ani,
							pCantFireAppMsg->returnCode,
							whyCantFireApp,
							yMsg);

				/*
				 *  We have already sent RTP details to media mgr,
				 *  To cleanup that data, notify again
				 */
				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);

				if (!gWritePCForEveryCall)
				{
					writeCDR ("process_DMOP_CANTFIREAPP", zCall, "PC", 20014, -1);	//DDN_TODO: SNMP SetRequest
				}

				writeCDR ("process_DMOP_CANTFIREAPP", zCall, "RC", 20014, -1);	//DDN_TODO: SNMP SetRequest

				if (gCall[zCall].callState != CALL_STATE_CALL_CLOSED
					&& gCall[zCall].callState != CALL_STATE_CALL_CANCELLED
					&& gCall[zCall].callState != CALL_STATE_CALL_RELEASED)
				{
// MR #4325
					dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE,
							   WARN, "Sending SIP Message %d Not Found.",
								whyCantFireApp);

					eXosip_lock (geXcontext);
					eXosip_call_send_answer (geXcontext, gCall[zCall].tid, whyCantFireApp, 0);
					eXosip_unlock (geXcontext);
// END: MR #4325
				}

				if (gCall[zCall].permanentlyReserved == 1)
				{

					__DDNDEBUG__ (DEBUG_MODULE_CALL,
								  "Changing the status to init", "", zCall);
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "Setting port num=%d to permanentlyReserved",
							   zCall);

					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS,
									STATUS_INIT);
				}
#if 0
/*Update: DDN 20091005: Commented ELSE block to avoid CANTFIREAPP, license issue*/
				else
				{

					__DDNDEBUG__ (DEBUG_MODULE_CALL,
								  "Changing the status to idle", "", zCall);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS,
									STATUS_IDLE);
					//writeToRespShm(mod, tran_tabl_ptr, zCall, APPL_SIGNAL, 1);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_PID,
									getpid ());
					updateAppName (__LINE__, zCall, zCall);
				}
#endif
			}
			break;

		case DMOP_APPDIED:
			{
	struct Msg_AppDied *pAppDied;

	char            fifoName[256];

				sprintf (fifoName, "%s", gCall[zCall].responseFifo);

//                dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//                   "DJB: not doing APPDIED - BREAKING OUT OF IT");
//              break;


				//if (access (fifoName, F_OK) == 0)
				{

					if (gCall[zCall].responseFifoFd > 0)
					{
						close (gCall[zCall].responseFifoFd);
					}

                    SendApplicationPortDisconnect(zCall);

					arc_unlink (zCall, mod, fifoName);
				}
				gCall[zCall].responseFifoFd = -1;

				pAppDied = (struct Msg_AppDied *) &(gCall[zCall].msgToDM);

				gCall[pAppDied->appCallNum].appPid = -1;

				if (gCall[zCall].callState != CALL_STATE_CALL_CLOSED
					&& gCall[zCall].callState != CALL_STATE_CALL_CANCELLED
					&& gCall[zCall].callState != CALL_STATE_CALL_RELEASED)
				{
					dropCall (mod, zCall, 0, &(gCall[zCall].msgToDM));
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Got DROPCALL notifying Media Manger");
					notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
				}

				if (gCall[zCall].permanentlyReserved == 1
					&& gCall[zCall].canKillApp == YES)
				{
	struct MsgToRes yMsgToRes;

					memcpy (&yMsgToRes, &(gCall[zCall].msgToDM),
							sizeof (struct MsgToRes));

					yMsgToRes.opcode = RESOP_STATIC_APP_GONE;
					yMsgToRes.appCallNum = zCall;
					yMsgToRes.appPid = gCall[zCall].msgToDM.appPid;

					writeToResp (mod, (void *) &yMsgToRes);
				}

				if (gCall[zCall].permanentlyReserved == 1)
				{

					__DDNDEBUG__ (DEBUG_MODULE_CALL,
								  "Changing the status to init", "", zCall);
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "Setting port num=%d to permanentlyReserved",
							   zCall);

					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS,
									STATUS_INIT);
				}
				else
				{

					__DDNDEBUG__ (DEBUG_MODULE_CALL,
								  "Changing the status to idle", "", zCall);

					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS,
									STATUS_IDLE);
					//writeToRespShm(mod, tran_tabl_ptr, zCall, APPL_SIGNAL, 1);
				}
			}
			break;

		case DMOP_OUTPUTDTMF:
			{
				if (gSipOutputDtmfMethod == OUTPUT_DTMF_RFC_2833)
				{
					notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
				}
				else if (gSipOutputDtmfMethod == OUTPUT_DTMF_INBAND)
				{
					notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
				}
				else
				{
	//struct timeb    yTmpTimeb;
					struct timeval	yTmpTimeb;

				//	ftime (&yTmpTimeb);
					gettimeofday(&yTmpTimeb, NULL);

					yRc = addToTimerList (__LINE__, zCall,
										  MP_OPCODE_OUTPUT_DTMF,
										  gCall[zCall].msgToDM,
										  yTmpTimeb, -1, -1, -1);
					if (yRc < 0)
					{
	struct MsgToApp response;

						response.opcode = gCall[zCall].msgToDM.opcode;
						response.appCallNum = gCall[zCall].msgToDM.appCallNum;
						response.appPid = gCall[zCall].msgToDM.appPid;
						response.appRef = gCall[zCall].msgToDM.appRef;
						response.returnCode = -1;

						writeGenericResponseToApp (__LINE__, zCall, &response, mod);
					}
				}
			}
			break;

		case DMOP_SETGLOBAL:
			{
	int             isNotifyMediaMgr = process_DMOP_SETGLOBAL (zCall);

				if (isNotifyMediaMgr == 1)
				{
					notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
				}
			}
			break;

		case DMOP_GETGLOBAL:
			{
				process_DMOP_GETGLOBAL (zCall);
				//notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_GETGLOBALSTRING:
			{
				process_DMOP_GETGLOBALSTRING (zCall);
				//notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_SETGLOBALSTRING:
			{
				process_DMOP_SETGLOBALSTRING (zCall);
				//notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_PORTOPERATION:
			{
				if (gCall[zCall].callState == CALL_STATE_CALL_CLOSED
					|| gCall[zCall].callState == CALL_STATE_CALL_CANCELLED
					|| gCall[zCall].callState ==
					CALL_STATE_CALL_TERMINATE_CALLED
					|| gCall[zCall].callState == CALL_STATE_CALL_RELEASED)
				{
	struct MsgToApp response;
	struct Msg_AnswerCall msg;

					msg = *(struct Msg_AnswerCall *) &(gCall[zCall].msgToDM);

					zCall = msg.appCallNum;

					setCallState (zCall, CALL_STATE_IDLE);
					//gCall[zCall].callState = CALL_STATE_IDLE;

					response.returnCode = -3;
					response.opcode = DMOP_DISCONNECT;
					response.appCallNum = msg.appCallNum;
					response.appPid = msg.appPid;
					response.appRef = msg.appRef;
					response.appPassword = msg.appPassword;

					yRc = writeGenericResponseToApp (__LINE__, zCall, &response, mod);

					close (gCall[zCall].responseFifoFd);
					arc_unlink (zCall, mod, gCall[zCall].responseFifo);
					gCall[zCall].responseFifo[0] = 0;
					gCall[zCall].cid = -1;
					gCall[zCall].did = -1;
					gCall[zCall].responseFifoFd = -1;

					break;
				}

				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_SPEECHDETECTED:
			{
				__DDNDEBUG__ (DEBUG_MODULE_SR, "DMOP_SPEECHDETECTED", "", 0);

				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_SRRECOGNIZEFROMCLIENT:
			{
				__DDNDEBUG__ (DEBUG_MODULE_SR, "DMOP_SRRECOGNIZEFROMCLIENT",
							  "", 0);

				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_SRRECOGNIZE:
			{
				__DDNDEBUG__ (DEBUG_MODULE_SR, "DMOP_SRRECOGNIZE",
							  "", 0);

				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_START_CALL_PROGRESS_RESPONSE:
			{
				__DDNDEBUG__ (DEBUG_MODULE_SR, "DMOP_START_CALL_PROGRESS", "",
							  0);

				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;
		case DMOP_END_CALL_PROGRESS:
			{
				__DDNDEBUG__ (DEBUG_MODULE_SR, "DMOP_CALL_PROGRESS_END", "",
							  0);

				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

		case DMOP_FAX_PROCEED:
			{
				__DDNDEBUG__ (DEBUG_MODULE_SR, "DMOP_FAX_PROCEED", "", 0);
				//Setting outbound rtpPort
	struct Msg_Fax_Proceed yMsgFaxProceed;
	int             yResponseRetCode = -1;
				memcpy (&yMsgFaxProceed, &gCall[zCall].msgToDM,
						sizeof (struct Msg_Fax_Proceed));

				sscanf (yMsgFaxProceed.nameValue,
						"ip=%[^&]&faxMediaPort=%d",
						gCall[zCall].faxServer, &(gCall[zCall].faxRtpPort));

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE,
						   INFO,
						   "Got nameValue=%s, faxRtpPort=%d, faxServer=%s. "
						   "isFaxCall=%d  sendFaxReinvite=%d  send200OKForFax=%d",
						   yMsgFaxProceed.nameValue, gCall[zCall].faxRtpPort,
						   gCall[zCall].faxServer,
						   gCall[zCall].faxData.isFaxCall,
						   gCall[zCall].sendFaxReinvite,
						   gCall[zCall].send200OKForFax);

				if (gCall[zCall].sendFaxReinvite == 1)
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE,
							   INFO, "Calling sendFaxReInvite");
					yRc = fnSendFaxReInvite (zCall);
				}
				else if (gCall[zCall].send200OKForFax == 1)
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE,
							   INFO, "Calling fnSend200OKForFax");
					yRc = fnSend200OKForFax (zCall);
				}
//              notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);           // djb -don't send back the PROCEED

				if (gCall[zCall].sendFaxOnProceed == 1)
				{
					notifyMediaMgr (__LINE__, zCall,
									(struct MsgToDM *) &(gCall[zCall].
														 msgSendFax), mod);
				}

			}
			break;
		case DMOP_FAX_COMPLETE:
			{
				__DDNDEBUG__ (DEBUG_MODULE_SR, "DMOP_FAX_COMPLETE", "", 0);
				if (gCall[zCall].send200OKForFax == 0)
				{
					//RGD TODO Drop the call here with some error. 
				}

				notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
			}
			break;

#ifdef VOICE_BIOMETRICS
        case DMOP_VERIFY_CONTINUOUS_SETUP:
            {
                __DDNDEBUG__ (DEBUG_MODULE_SR, "DMOP_VERIFY_CONTINUOUS_SETUP", "",
                              0);

                notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
            }
            break;
        case DMOP_VERIFY_CONTINUOUS_GET_RESULTS:
            {
                __DDNDEBUG__ (DEBUG_MODULE_SR, "DMOP_VERIFY_CONTINUOUS_GET_RESULTS", "",
                              0);

                notifyMediaMgr (__LINE__, zCall, &(gCall[zCall].msgToDM), mod);
            }
            break;
#endif // VOICE_BIOMETRICS


		default:
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
						   TEL_INVALID_OPCODE, ERR,
						   "Unknown request opcode (%d) received from pid=%d.  No action taken.",
						   gCall[zCall].msgToDM.opcode,
						   gCall[zCall].msgToDM.appPid);
			}
			break;

		}						/* switch */

	}							/*END: while(gCanReadRequestFifo) */

	gAppReqReaderThreadId = 0;

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Detaching readAndProcessAppRequests",
				  "", 0);

	pthread_detach (pthread_self ());

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Exiting readAndProcessAppRequests", "",
				  0);

	//pthread_exit (NULL);
	return (NULL);

}								/*END: void * readAndProcessAppRequests */

int
notifyMediaMgrThroughTimer (int zLine, int zCall, struct MsgToDM *zpMsgToDM, char *mod)
{
	return (0);

}								/*int notifyMediaMgrThroughTimer */

///This is the function used to fire Media Manager.
int
startMediaMgr ()
{
char            mod[] = "startMediaMgr";
int             yTempPid;
int             yRc;
char            yErrMsg[128];
char            yStrIndex[10];
char           *yTmpIspBase;

	/*X instances of Call Manager */
char            yStrStartPort[32];
char            yStrEndPort[32];
char            yStrDynMgrId[32];

	sprintf (yStrStartPort, "%d", gStartPort);
	sprintf (yStrEndPort, "%d", gEndPort);
	sprintf (yStrDynMgrId, "%d", gDynMgrId);
	/*END: X instances of Call Manager */

	sleep (1);

	if ((yTmpIspBase = getenv ("ISPBASE")) == NULL)
	{
		sprintf (yErrMsg, "%s", "Failed to get ISBASE from env");

		printf ("%s\n", yErrMsg);
		fflush (stdout);

		return (-1);
	}

	sprintf (gAppPath, "%s/Telecom/Exec/ArcSipMediaMgr", yTmpIspBase);

	if ((yTempPid = fork ()) == 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_MEDIAMGR_STARTUP,
				   INFO, "Starting ArcSipMediaMgr # %d path(%s)", 0,
				   gAppPath);

		if (gDebugModules[DEBUG_MODULE_STACK].enabled == 1)
		{
			/*
			   yRc = execl (gAppPath, "ArcSipMediaMgr", (char *) 0);
			 */

			dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "%d=fork().  execl(%s)", yTempPid, gAppPath);

			yRc = execl (gAppPath,
						 "ArcSipMediaMgr",
						 "-s",
						 yStrStartPort,
						 "-e", yStrEndPort, "-d", yStrDynMgrId, NULL);
		}
		else
		{
			/*
			   yRc = execl (gAppPath, "ArcSipMediaMgr", "2>/dev/null",
			   (char *) 0);
			 */

			dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "%d=fork().  execl(%s)", yTempPid, gAppPath);

			yRc = execl (gAppPath,
						 "ArcSipMediaMgr",
						 "-s",
						 yStrStartPort,
						 "-e",
						 yStrEndPort,
						 "-d", yStrDynMgrId, "-x", "2>/dev/null", NULL);
		}

		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, DYN_BASE, INFO,
				   "execl yRc = %d", yRc);

		if (yRc < 0)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_BASE, ERR,
					   "Failed to execl ArcSipMediaMgr.  rc=%d.  [%d, %s]",
					   yRc, errno, strerror (errno));

			yTempPid = -1;

			arc_exit (-1, "");
		}

		exit (0);
	}

	if (yTempPid < 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_BASE, ERR,
				   "fork() failed.  rc=%d. [%d, %s]  Unable to start ArcSipMediaMgr.",
				   yTempPid, errno, strerror (errno));

		return (-1);
	}

	gMediaMgrPid = yTempPid;

	return (0);

}								/*END: startMediaMgr */

int
stopMediaMgr ()
{
	return (0);
}

///This function is used to fire the readAndProcessAppRequests() thread.
int
startAppRequestReaderThread ()
{
char            mod[] = { "startAppRequestReaderThread" };
int             yRc = 0;
int             zCall = -1;

	yRc = pthread_attr_init (&gAppReqReaderThreadAttr);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "pthread_attr_init() failed. rc=%d. [%d, %s] "
				   "Unable to create application request thread. Exiting.",
				   yRc, errno, strerror (errno));
		return (-1);
	}

	yRc = pthread_attr_setdetachstate (&gAppReqReaderThreadAttr,
									   PTHREAD_CREATE_DETACHED);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "pthread_attr_setdetachstate() failed. rc=%d. [%d, %s] "
				   "Unable to create application request thread. Exiting.",
				   yRc, errno, strerror (errno));
		return (-1);
	}

	yRc = pthread_create (&gAppReqReaderThreadId, &gAppReqReaderThreadAttr,
						  readAndProcessAppRequests, NULL);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "pthread_create() failed. rc=%d. [%d, %s] "
				   "Unable to create application request thread. Exiting.",
				   yRc, errno, strerror (errno));

		return (-1);
	}
	return (0);

}								/*END: int startAppRequestReaderThread */

///This function is the multiPurposeTimerThread.
void           *
timerThread (void *z)
{
	int             canDeleteElement = 0;
	int             zCall = -1;
	char            mod[] = { "timerThread" };
	char            yMessage[256];
	int             rc = 21;
	int             yRc = 0;

	dynVarLog (__LINE__, 0, mod, REPORT_VERBOSE, DYN_BASE, INFO,
			   "Timer thread pid=%d.", syscall (224));

	struct MsgToDM  gDisconnectStruct = { DMOP_DISCONNECT, -1, -1, -1 };
	struct MsgToDM  dropCallMsg; // BT-176


	struct MultiPurposeTimerStrcut Tmp;
	struct MultiPurposeTimerStrcut *TmpRef = NULL;
	struct MultiPurposeTimerStrcut *Last = NULL;
    int found; 

	//struct timeb    tb;
	struct timeval	tb;

	while (gCanReadRequestFifo)
	{

		//ftime (&tb);
		gettimeofday(&tb, NULL);

		canDeleteElement = 0;

        found = getFirstFromTimerList(&Tmp, (void *)Last, (void **)&TmpRef);

        if(!found){
         	if (!gCanReadRequestFifo){
				break;
			}
            Last = NULL;
			usleep (1000 * 100);
            // delete expired structures here 
			delFromTimerList(__LINE__, TimerByAge, NULL);
			delFromTimerList(__LINE__, TimerMarkedAsDeleted, NULL);
			continue;
        }

	    Last = TmpRef;

		zCall = Tmp.port;

		// printTimerList(__LINE__, zCall);
		switch (Tmp.opcode) {

		case MP_OPCODE_REREGISTER:
			{
				struct timeval	nt;
//                struct timeb nt; // next time !
		       // ftime (&nt);
				gettimeofday(&nt, NULL);

				if (Tmp.expires.tv_sec <= tb.tv_sec) {
					if (gSipRedirection) {
						registerToRedirector (mod);
                        // if we re-register reschedule the event 
                        nt.tv_sec += 60;
                        addToTimerList (__LINE__, zCall, MP_OPCODE_REREGISTER, gCall[0].msgToDM,
                        nt, -1, -1, -1);
					}
					canDeleteElement = 1;
				}
			}
			break;

#ifdef ACU_LINUX
		case MP_OPCODE_CHECK_FAX_DELAY:
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE,
						   INFO,
						   "Inside Timer expires.time=%d, current time=%d.",
						   Tmp.expires.time,
						   tb.tv_sec);

				if (Tmp.expires.time > tb.tv_sec) {
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "expires.time=%d, current time=%d, timer not expired; continuing.");
					canDeleteElement = 0;
					break;
				}
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "expires.time=%d, current time=%d.  Timer expired; deleting it. "
						   "faxServerSpecialFifo=(%s).",
						   Tmp.expires.tv_sec,
						   tb.tv_sec, gCall[zCall].faxServerSpecialFifo);

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE,
						   INFO,
						   "faxServerSpecialFifo=%s deleteing the timer",
						   gCall[zCall].faxServerSpecialFifo);

				if (gCall[zCall].faxServerSpecialFifo[0]) {
					gCall[zCall].faxDelayTimerStarted = 0;
					canDeleteElement = 1;
					break;
				}

	struct MsgToApp response;
				memcpy (&response, &(gCall[zCall].msgToDM),
						sizeof (struct MsgToApp));
				response.returnCode = -1;
				canDeleteElement = 1;
				gCall[zCall].faxDelayTimerStarted = 0;
				sprintf (response.message, "%s", "Fax timer expired");
				writeGenericResponseToApp (__LINE__, zCall, &response, mod);
				break;
			}
			break;
#endif

		case MP_OPCODE_STATIC_APP_GONE:

			{
				if (gCall[zCall].permanentlyReserved == 1) {
					if (gCall[zCall].canKillApp == NO) {
						canDeleteElement = 1;
					}
					else if (arc_kill (Tmp.msgToDM.appPid, 0) == -1)
					{
						if (errno == ESRCH){
							struct MsgToRes yMsgToRes;

							memcpy (&yMsgToRes,
									&(Tmp.msgToDM),
									sizeof (struct MsgToRes));

							yMsgToRes.opcode = RESOP_STATIC_APP_GONE;
							yMsgToRes.appCallNum = zCall;
							yMsgToRes.appPid = Tmp.msgToDM.appPid;

							sprintf (yMsgToRes.dnis, "%s", gCall[zCall].dnis);
							sprintf (yMsgToRes.ani, "%s", gCall[zCall].ani);

							writeToResp (mod, (void *) &yMsgToRes);
							dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_OS_SIGNALING, WARN, 
							"Sent RESOP_STATIC_APP_GONE for PID(%d). Timer thread list entries=%d", 
							Tmp.msgToDM.appPid, getTimerListCount());


							canDeleteElement = 1;
						}
					} else {
                      // the below was deleting an element used to schedule the next call 
					  ;; // 	canDeleteElement = 1;
					}
				}
			}
			break;

		case MP_OPCODE_OUTPUT_DTMF:
			{
	int             i = 0;
	int             rc = 0;
	int             yIntStrLen = 0;
	struct MsgToApp response;
	struct Msg_OutputDTMF yMsgOutputDtmf;
	osip_message_t *info = NULL;
	char            dtmf_body[1000];

				memcpy (&(yMsgOutputDtmf),
						&(Tmp.msgToDM), sizeof (struct Msg_OutputDTMF));

				yIntStrLen = strlen (yMsgOutputDtmf.dtmf_string);

				for (i = 0; i < yIntStrLen; i++)
				{
					eXosip_lock (geXcontext);
					rc = eXosip_call_build_info (geXcontext, gCall[zCall].did, &info);
					eXosip_unlock (geXcontext);

					if (rc == 0)
					{
						snprintf (dtmf_body, 999,
								  "Signal=%c\r\nDuration=250\r\n",
								  yMsgOutputDtmf.dtmf_string[i]);

						osip_message_set_content_type
							(info, "application/dtmf-relay");
						osip_message_set_body (info,
											   dtmf_body, strlen (dtmf_body));

						eXosip_lock (geXcontext);
						arc_fixup_contact_header (info, gHostIp, gSipPort,
												  zCall);
						rc = eXosip_call_send_request (geXcontext, gCall[zCall].did,
													   info);
						eXosip_unlock (geXcontext);
					}
				}

				response.opcode = yMsgOutputDtmf.opcode;
				response.appCallNum = yMsgOutputDtmf.appCallNum;
				response.appPid = yMsgOutputDtmf.appPid;
				response.appRef = yMsgOutputDtmf.appRef;
				response.returnCode = 0; 
				writeGenericResponseToApp (__LINE__, zCall, &response, mod);

				canDeleteElement = 1;

			}
			break;

		case MP_OPCODE_TERMINATE_APP:
			{
				if (gCanKillApp == YES)
				{
				//	ftime (&tb);
					gettimeofday(&tb, NULL);

					if (Tmp.expires.tv_sec <= tb.tv_sec)
					{
						if (Tmp.msgToDM.appPid > 0)
						{
							if (arc_kill
								(Tmp.msgToDM.appPid,
								 0) == 0)
							{

								__DDNDEBUG__ (DEBUG_MODULE_CALL,
											  "Terminating", "",
											  Tmp.msgToDM.appPid);

								dynVarLog (__LINE__, zCall, mod,
										   REPORT_DETAIL, TEL_OS_SIGNALING,
										   WARN,
										   "Sending SIGTERM to app pid(%d).",
										   Tmp.msgToDM.appPid);

								arc_kill (Tmp.msgToDM.appPid, 15);

							}
						}

						canDeleteElement = 1;
					}			/*END: if */
				}
			}
			break;

		case MP_OPCODE_SESSION_EXPIRES_TERMINATE_APP: 
			{
//                if ( (!canContinue (mod, zCall)) ||                 // MR # 4946 
//				     (gCall[zCall].canKillApp == YES) )
                if (!canContinue (mod, zCall))                 // MR # 4946 
                {       
                    /*Call already hung up */
                    canDeleteElement = 1;
                    break;  
                }
                else if ( (gCall[zCall].appPid != Tmp.data1) && (Tmp.data1 != -1) )
                {                             
                    /*Call has already gone from the system */
                    canDeleteElement = 1;
					
					// MR 5022 - session timers
					if ( gCall[zCall].appPid != -1 )
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_OS_SIGNALING, WARN,
							"Received session-expires timer for appPid (%d) does not match "
							"DM's appPid (%d);. Ignoring.  Timer will be deleted and call will NOT be dropped.",
							Tmp.data1, gCall[zCall].appPid );
					}
                    break;      
                }                          
                else if (Tmp.deleted == 1) 
                {
                    /*Somebody has removed this timer before it expired */
                    canDeleteElement = 1;
                    break;
                }

			//	ftime (&tb);
				gettimeofday(&tb, NULL);
	
				if (Tmp.expires.tv_sec <= tb.tv_sec)
				{
					if (gCanKillApp == YES)
					{
						if (Tmp.msgToDM.appPid > 0)
						{
							if (arc_kill (Tmp.msgToDM.appPid, 0) == 0)
							{
	
								__DDNDEBUG__ (DEBUG_MODULE_CALL, "Terminating", "", Tmp.msgToDM.appPid);
	
								dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_OS_SIGNALING, WARN,
											   "MP_OPCODE_SESSION_EXPIRES_TERMINATE_APP hit - Sending SIGTERM to app pid(%d).",
											   Tmp.msgToDM.appPid);
								arc_kill (Tmp.msgToDM.appPid, 15);
							}
						}
						canDeleteElement = 1;
					}			/*END: if */
					else
					{
						canDeleteElement = 1;
						// BT-176
						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_OS_SIGNALING, WARN,
								   "MP_OPCODE_SESSION_EXPIRES_TERMINATE_APP hit - Dropping the call and "
									"notifying Media Manager.");
						memcpy (&dropCallMsg, &(gCall[zCall].msgToDM), sizeof (struct Msg_DropCall));
						dropCallMsg.opcode = DMOP_DROPCALL;
						notifyMediaMgr (__LINE__, zCall, &(dropCallMsg), mod);
						dropCall (mod, zCall, 0, &(gCall[zCall].msgToDM));
					}
				}
			}
			break;

		case MP_OPCODE_SESSION_EXPIRES_SEND_REFRESH:
			{
				struct tm       tempTM;
			    struct tm       *pTM;
				char			str[21];
				//struct timeb	save_tb;
				struct timeval	save_tb;

                if (!canContinue (mod, zCall))  // MR # 4946
                {       
                    /*Call already hung up */
                    canDeleteElement = 1;
                    break;  
                }
                else if ( (gCall[zCall].appPid != Tmp.data1) && (Tmp.data1 != -1) )
                {                             
                    /*Call has already gone from the system */
                    canDeleteElement = 1;
							
					// MR 5022 - session timers
					if ( gCall[zCall].appPid != -1 )
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_OS_SIGNALING, WARN,
							"Received session-expires-send-refresh timer for appPid (%d) does not match "
							"DM's appPid (%d);. Ignoring.  Timer will be deleted and call will NOT be dropped.",
							Tmp.data1, gCall[zCall].appPid );
					}
		                    break;      
		                }                          
		                else if (Tmp.deleted == 1) 
		                {
		                    /*Somebody has removed this timer before it expired */
		                    canDeleteElement = 1;
		                    break;
		                }
		
	

				gettimeofday(&tb, NULL);
				gettimeofday(&save_tb, NULL);
				if (Tmp.expires.tv_sec <= tb.tv_sec)
				{
					int             rc = 0;
					int             sessExpires;
					char			sRefresher[32]="refresher=uac";
					char			c_sessExpires[32]="";
					char			c_minSE[32];

					osip_message_t *update = NULL;
//					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
//							"DJB: inside MP_OPCODE_SESSION_EXPIRES_SEND_REFRESH - time hit (%d)", tb.time);

					sessExpires = gCallInfo[zCall].GetIntOption("session-expires");
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						"Retrieved session-expires time of %d.", sessExpires);

			        sprintf(c_sessExpires, "%d;%s", sessExpires, sRefresher);

				    rc = gCallInfo[zCall].GetIntOption("refresh-time");
				    dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				        "Retrieved refresh-time of %d.  Adding to timer list.", rc);
					tb.tv_sec += rc;

				    rc = gCallInfo[zCall].GetIntOption("min-se");
			        sprintf(c_minSE, "%d", rc);

					eXosip_lock (geXcontext);
					rc = eXosip_call_build_update (geXcontext, gCall[zCall].did, &update);

				    dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				        "Adding Session-Expires header (%s)", c_sessExpires);

			        osip_message_set_header (update, (char *)"Session-Expires", c_sessExpires);
			        osip_message_set_header (update, (char *)"Supported", "timer");
			        osip_message_set_header (update, (char *)"Min-SE", c_minSE);

					if (rc == 0)
					{
						rc = eXosip_call_send_request (geXcontext, gCall[zCall].did, update);
					}
					canDeleteElement = 1;

					(void *)localtime_r(&tb.tv_sec, &tempTM);
					pTM = &tempTM;
					(void)strftime(str, 32, "%H:%M:%S", pTM);
					
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
								"Next refresh to be sent at %s.  Adding to timer list [ MP_OPCODE_SESSION_EXPIRES_SEND_REFRESH, cid=%d ]. %d",
								str, Tmp.data1, tb.tv_sec);

					(void) addToTimerList (__LINE__, zCall,
						  MP_OPCODE_SESSION_EXPIRES_SEND_REFRESH,
						  gCall[zCall].msgToDM, tb, Tmp.data1, -1, -1);

					save_tb.tv_sec += sessExpires;
					modifyExpiresTimerList (zCall, MP_OPCODE_SESSION_EXPIRES_TERMINATE_APP, save_tb);
					eXosip_unlock (geXcontext);
				}
			}
			break;

		case MP_OPCODE_KILL_APP:
			{
		//		ftime (&tb);
				gettimeofday(&tb, NULL);

				if (Tmp.expires.tv_sec <= tb.tv_sec)
				{
					if (Tmp.msgToDM.appPid > 0)
					{
						if (arc_kill
							(Tmp.msgToDM.appPid,
							 0) == 0)
						{
#if 0
							__DDNDEBUG__ (DEBUG_MODULE_CALL, "Killing", "",
										  gpTmpMultiPurposeTimerStruct->
										  msgToDM.appPid);

							dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
									   TEL_OS_SIGNALING, WARN,
									   "Sending SIGKILL to app pid (%d).",
									   gpTmpMultiPurposeTimerStruct->msgToDM.
									   appPid);

							arc_kill (gpTmpMultiPurposeTimerStruct->msgToDM.
									  appPid, 9);
#endif
						}
					}

					canDeleteElement = 1;

				}				/*END: if */
			}
			break;

		case MP_OPCODE_END_CALL:
			{
			//	ftime (&tb);
				gettimeofday(&tb, NULL);

				if (!canContinue (mod, zCall))
				{
					/*Call already hung up */
					canDeleteElement = 1;
					break;
				}
				else if (gCall[zCall].appPid != Tmp.data1)
				{
					/*Call has already gone from the system */
					canDeleteElement = 1;
					break;
				}
				else if (Tmp.deleted == 1)
				{
					/*Somebody has removed this timer before it expired */
					canDeleteElement = 1;
					break;
				}
				else if (Tmp.expires.tv_sec <= tb.tv_sec)
				{
					if (gCall[zCall].callState != CALL_STATE_CALL_CLOSED
						&& gCall[zCall].callState != CALL_STATE_CALL_CANCELLED
						&& gCall[zCall].callState != CALL_STATE_CALL_RELEASED
						&& gCall[zCall].callState != CALL_STATE_IDLE
						&& gCall[zCall].callState !=
						CALL_STATE_CALL_TERMINATE_CALLED
						&& gCall[zCall].appPid ==
						Tmp.data1)
					{
	struct Msg_DropCall msgDropCall;

						memcpy (&msgDropCall,
								&(Tmp.msgToDM),
								sizeof (struct Msg_DropCall));

						dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
								   DYN_BASE, INFO,
								   "Dropping call from MP_OPCODE_END_CALL. gMaxCallDuration=%d state:%d appPid:%d",
								   gMaxCallDuration, gCall[zCall].callState,
								   gCall[zCall].appPid);

						if ((gCall[zCall].callState == CALL_STATE_CALL_BRIDGED
							 || gCall[zCall].callSubState ==
							 CALL_STATE_CALL_MEDIACONNECTED)
							&& msgDropCall.allParties == 2
							&& gCall[zCall].crossPort >= 0
							&& gCall[zCall].leg == LEG_A)
						{
	int             yBLegPort = gCall[zCall].crossPort;

							setCallState (yBLegPort, CALL_STATE_CALL_CLOSED);

							gDisconnectStruct.appCallNum = yBLegPort;
							gDisconnectStruct.opcode = DMOP_DISCONNECT;
							gDisconnectStruct.appPid = gCall[zCall].appPid;
							gDisconnectStruct.appPassword =
								gCall[yBLegPort].appPassword;

							notifyMediaMgr (__LINE__, yBLegPort,
											(struct MsgToDM *)
											&gDisconnectStruct, mod);

							gDisconnectStruct.appCallNum = -1;
							gDisconnectStruct.appPid = -1;
							gDisconnectStruct.appPassword = -1;

							__DDNDEBUG__ (DEBUG_MODULE_CALL,
										  "Terminating the call on B leg", "",
										  yBLegPort);
							dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
									   DYN_BASE, INFO,
									   "Sending SIP Message BYE.");

							eXosip_lock (geXcontext);
							eXosip_call_terminate (geXcontext, gCall[yBLegPort].cid,
												   gCall[yBLegPort].did);
							time (&(gCall[yBLegPort].lastReleaseTime));
							eXosip_unlock (geXcontext);

							gCall[yBLegPort].prevAppPid = -1;

							break;
						}

						if (gCall[zCall].crossPort >= 0 &&
							(gCall[zCall].callState == CALL_STATE_CALL_BRIDGED
							 || gCall[zCall].callSubState ==
							 CALL_STATE_CALL_MEDIACONNECTED)
							&& gCall[zCall].leg == LEG_A)
						{
							if (msgDropCall.allParties == 1)
							{
	int             yBLegPort = gCall[zCall].crossPort;

								setCallState (yBLegPort,
											  CALL_STATE_CALL_CLOSED);
								//gCall[yBLegPort].callState = CALL_STATE_CALL_CLOSED;

								gDisconnectStruct.appCallNum = yBLegPort;
								gDisconnectStruct.opcode = DMOP_DISCONNECT;
								gDisconnectStruct.appPid =
									gCall[zCall].appPid;
								gDisconnectStruct.appPassword =
									gCall[zCall].appPassword;

								notifyMediaMgr (__LINE__, yBLegPort,
												(struct MsgToDM *)
												&gDisconnectStruct, mod);

								gDisconnectStruct.appCallNum = -1;
								gDisconnectStruct.appPid = -1;
								gDisconnectStruct.appPassword = -1;

								__DDNDEBUG__ (DEBUG_MODULE_CALL,
											  "Terminating the call on B leg",
											  "", yBLegPort);

								dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, DYN_BASE, INFO,
										   "Sending SIP Message BYE.");

								eXosip_lock (geXcontext);
								eXosip_call_terminate (geXcontext, gCall[yBLegPort].cid,
													   gCall[yBLegPort].did);
								time (&(gCall[yBLegPort].lastReleaseTime));
								eXosip_unlock (geXcontext);

								gCall[yBLegPort].prevAppPid = -1;

							}
						}

						dropCall (mod, zCall, 0, &(gCall[zCall].msgToDM));

#if 0
						eXosip_lock (geXcontext);
						eXosip_call_terminate (geXcontext, gCall[zCall].cid,
											   gCall[zCall].did);
						setCallState (zCall, CALL_STATE_CALL_CLOSED);
						time (&(gCall[zCall].lastReleaseTime));
						gCallsFinished++;
						eXosip_unlock (geXcontext);
#endif

						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   DYN_BASE, INFO,
								   "Got DROPCALL. Notifying Media Manager from timerThread(%d:%d)",
								   syscall (224), pthread_self ());

						gDisconnectStruct.appCallNum = zCall;
						gDisconnectStruct.opcode = DMOP_DISCONNECT;
						gDisconnectStruct.appPid = gCall[zCall].appPid;
						gDisconnectStruct.appPassword =
							gCall[zCall].appPassword;
						notifyMediaMgr (__LINE__, zCall,
										(struct MsgToDM *) &gDisconnectStruct,
										mod);

						if (gCall[zCall].leg == LEG_B)
						{
							gCall[zCall].prevAppPid = -1;
							gCall[zCall].appPid = -1;
						}
					}

					canDeleteElement = 1;

				}				/*END: if */
				//  canDeleteElement = 0;
			}
			break;

		default:
			{
				canDeleteElement = 1;
			}
			break;

		}						/*END: switch */

		if (canDeleteElement == 1){
			delFromTimerList(__LINE__, TimerByRef, TmpRef);
		}
		else
		{
			delFromTimerList(__LINE__, TimerByAge, NULL);
		}

		if (!gCanReadRequestFifo)
		{
			break;
		}

		usleep (1000 * 100);

	}							/*END: while */

	pthread_detach (pthread_self ());
	pthread_exit (NULL);
	return (NULL);
}								/*END: void * timerThread */

///This function is used to fire the timerThread() thread.
int
startMultiPurposeTimerThread ()
{
int             yRc = 0;
char            mod[] = { "startMultiPurposeTimerThread" };

	pthread_mutex_init (&gMultiPurposeTimerMutex, NULL);

	yRc = pthread_attr_init (&gMultiPurposeTimerThreadAttr);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "pthread_attr_init() failed. rc=%d. [%d, %s] "
				   "Unable to create timer thread. Exiting.",
				   yRc, errno, strerror (errno));

		return (-1);
	}

	yRc = pthread_attr_setdetachstate (&gMultiPurposeTimerThreadAttr,
									   PTHREAD_CREATE_DETACHED);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "pthread_attr_setdetachstate() failed. rc=%d. [%d, %s] "
				   "Unable to create timer thread. Exiting.",
				   yRc, errno, strerror (errno));

		return (-1);
	}

	yRc = pthread_create (&gMultiPurposeTimerThreadId,
						  &gMultiPurposeTimerThreadAttr, timerThread, NULL);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "pthread_create() failed. rc=%d. [%d, %s] "
				   "Unable to create timer thread. Exiting.",
				   yRc, errno, strerror (errno));

		return (-1);
	}

	return (0);

}								/*END: int startMultiPurposeTimerThread */

///This function creates mrcp details for the mrcp client to read.
int
createMrcpRtpDetails (int zCall)
{
	FILE           *fp;
	char            yStrFileName[1024];

	sprintf (yStrFileName, "%s/.mrcpRtpDetails.%d", gCacheDirectory, zCall);

	unlink (yStrFileName);

#ifdef SR_MRCP
	fp = fopen (yStrFileName, "w+");

	if (fp == NULL)
	{
		return (-1);
	}

	fprintf (fp, "telephonePayloadType=%d\n",
			 gCall[zCall].telephonePayloadType);
	//fprintf (fp, "telephonePayloadType=96\n");
	fprintf (fp, "codecType=%d\n", gCall[zCall].codecType);

	fclose (fp);
#endif

	return (0);

}								/*END: int createMrcpRtpDetails */

void           *
supervisedTransferCallThread (void *z)
{
	char            mod[] = { "supervisedTransferCallThread" };
	struct MsgToDM *ptrMsgToDM;
	struct Msg_InitiateCall msg;
	struct MsgToApp response;
	int             yRc;
	int             zCall;
	int             zOutboundCall = -1;
	int             waitCount = 0;
	int             ringCountIn100msUnits;
	char            message[MAX_LOG_MSG_LENGTH];
	time_t          yCurrentTime, yConnectTime;
	char            yStrDestination[500];
	char            yStrFromUrl[256];
	char            yStrLocalSdpPort[10];
	char            yStrPayloadNumber[10];
	char            yStrPayloadMime[255];

#if 0
	osip_message_t *invite = NULL;

	time (&yCurrentTime);
	time (&yConnectTime);

	msg = *(struct Msg_InitiateCall *) z;

	zCall = msg.appCallNum;
	memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

	if (msg.listenType == DIAL_OUT)
	{
		zOutboundCall = getFreeCallNum ();

		if (zOutboundCall == -1)
		{
			response.returnCode = -1;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}

		memcpy (&(gCall[zOutboundCall].msgToDM),
				&(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

		gCall[zCall].crossPort = zOutboundCall;
		gCall[zCall].leg = LEG_A;

		gCall[zOutboundCall].crossPort = zCall;
		gCall[zOutboundCall].leg = LEG_B;

		yConnectTime = yConnectTime + 2 + msg.numRings * 6;

		gCall[zOutboundCall].remoteRtpPort = -1;

		//Check to see what kind of format we are receiving fromt the API
		//Then construct the appropriate destination strings

	char            yStrTmpIPAdrress[32];

	char           *pch = strchr (msg.ipAddress, '@');

		sprintf (yStrTmpIPAdrress, "%s", msg.ipAddress);

		pch = strstr (yStrTmpIPAdrress, "@");

		if (pch == NULL)
		{
			sprintf (yStrDestination, "sip:%s@%s;user=phone",
					 msg.phoneNumber, msg.ipAddress);
		}
		else
		{
			sprintf (yStrDestination, "sip:%s", msg.ipAddress);
		}

		sprintf (yStrFromUrl, "sip:arc@%s", gHostIp);

		eXosip_lock (geXcontext);

		/*
		 * Set the capabilities same as A-LEG
		 */
		eXosip_sdp_negotiation_remove_audio_payloads ();

		switch (gCall[zCall].codecType)
		{

		case 8:
			eXosip_sdp_negotiation_add_codec (osip_strdup ("8"),
											  NULL,
											  osip_strdup
											  ("RTP/AVP"), NULL,
											  NULL, NULL, NULL,
											  NULL,
											  osip_strdup ("8 PCMA/8000"));
			break;

		case 3:
			eXosip_sdp_negotiation_add_codec (osip_strdup ("3"),
											  NULL,
											  osip_strdup
											  ("RTP/AVP"), NULL,
											  NULL, NULL, NULL,
											  NULL,
											  osip_strdup ("3 GSM/8000"));
			break;

		case 110:
			eXosip_sdp_negotiation_add_codec (osip_strdup ("110"),
											  NULL,
											  osip_strdup
											  ("RTP/AVP"), NULL,
											  NULL, NULL, NULL,
											  NULL,
											  osip_strdup ("110 speex/8000"));
			break;

		case 111:
			eXosip_sdp_negotiation_add_codec (osip_strdup ("111"),
											  NULL,
											  osip_strdup
											  ("RTP/AVP"), NULL,
											  NULL, NULL, NULL,
											  NULL,
											  osip_strdup
											  ("111 speex/16000"));
			break;

		case 0:
		default:
			eXosip_sdp_negotiation_add_codec (osip_strdup ("0"),
											  NULL,
											  osip_strdup
											  ("RTP/AVP"), NULL,
											  NULL, NULL, NULL,
											  NULL,
											  osip_strdup ("0 PCMU/8000"));
			break;

		}

		sprintf (yStrPayloadNumber, "%d", gCall[zCall].telephonePayloadType);
		sprintf (yStrPayloadMime, "%d telephone-event/8000",
				 gCall[zCall].telephonePayloadType);

		eXosip_sdp_negotiation_add_codec (osip_strdup
										  (yStrPayloadNumber), NULL,
										  osip_strdup ("RTP/AVP"), NULL,
										  NULL, NULL, NULL, NULL,
										  osip_strdup (yStrPayloadMime));

		if (gOutboundProxyRequired)
		{

	char            yStrTmpProxy[256];

			sprintf (yStrTmpProxy, "sip:%s", gOutboundProxy);

			yRc = eXosip_build_initial_invite_with_expiry_time (&invite, yStrTmpProxy,	//to
																yStrFromUrl,	//from
																yStrDestination,	//route
																"",	//subject
																msg.
																numRings * 6);
		}
		else
		{
			yRc = eXosip_build_initial_invite_with_expiry_time (&invite, yStrDestination,	//to
																yStrFromUrl,	//from
																"",	//route
																"",	//subject
																msg.
																numRings * 6);
		}

		if (yRc < 0)
		{
			response.returnCode = -1;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			eXosip_unlock (geXcontext);

			gCall[zCall].crossPort = -1;
			gCall[zCall].leg = LEG_A;

			gCall[zOutboundCall].crossPort = -1;
			gCall[zOutboundCall].leg = LEG_A;

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}

		sprintf (yStrLocalSdpPort, "%d",
				 LOCAL_STARTING_RTP_PORT + (zOutboundCall * 2));

		yRc = eXosip_initiate_call (invite, NULL, NULL, yStrLocalSdpPort);

		eXosip_unlock (geXcontext);

		if (yRc < 0)
		{
			response.returnCode = -1;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			gCall[zCall].crossPort = -1;
			gCall[zCall].leg = LEG_A;

			gCall[zOutboundCall].crossPort = -1;
			gCall[zOutboundCall].leg = LEG_A;

			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}

		setCallState (zCall, CALL_STATE_CALL_BRIDGED);
		//gCall[zCall].callState = CALL_STATE_CALL_BRIDGED;
		setCallState (zOutboundCall, CALL_STATE_CALL_BRIDGED);
		//gCall[zOutboundCall].callState = CALL_STATE_CALL_BRIDGED;

		gCall[zOutboundCall].lastError = CALL_NO_ERROR;
		gCall[zOutboundCall].cid = yRc;

		response.returnCode = 0;

		while (gCall[zOutboundCall].callState != CALL_STATE_CALL_ANSWERED)
		{
			usleep (10 * 1000);

			time (&yCurrentTime);

			if (yCurrentTime >= yConnectTime
				|| gCall[zOutboundCall].callState ==
				CALL_STATE_CALL_REQUESTFAILURE)
			{
				response.returnCode = -50;
				sprintf (response.message, "%d:%s", 50, "Ring no answer");

				if (gCall[zOutboundCall].callState ==
					CALL_STATE_CALL_REQUESTFAILURE)
				{
					response.returnCode =
						-1 *
						mapSipErrorToArcError (gCall[zOutboundCall].
											   eXosipStatusCode);
					sprintf (response.message, "%d:%s",
							 mapSipErrorToArcError (gCall[zCall].
													eXosipStatusCode),
							 gCall[zOutboundCall].eXosipReasonPhrase);
				}

				writeGenericResponseToApp (__LINE__, zCall, &response, mod);

				gCall[zCall].crossPort = -1;
				gCall[zCall].leg = LEG_A;

				gCall[zOutboundCall].crossPort = -1;
				gCall[zOutboundCall].leg = LEG_A;

				gCall[zOutboundCall].cid = -1;

				pthread_detach (pthread_self ());
				pthread_exit (NULL);
				return (NULL);
			}
		}						/*END: while */

		gCall[zOutboundCall].appPid = msg.appPid;
		gCall[zCall].appPid = msg.appPid;

		msg.appCallNum2 = zOutboundCall;
		msg.appCallNum1 = zCall;
		msg.appCallNum = zCall;

		sprintf (response.message, "%d", zOutboundCall);

		yRc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msg), mod);

		if (gCall[zOutboundCall].remoteRtpPort > 0)
		{
	struct MsgToDM  msgRtpDetails;
			memcpy (&msgRtpDetails, &(gCall[zOutboundCall].msgToDM),
					sizeof (struct MsgToDM));

			if (gCall[zOutboundCall].leg == LEG_B
				&& gCall[zOutboundCall].crossPort >= 0)
			{
	int             yALegPort = gCall[zOutboundCall].crossPort;

				gCall[zOutboundCall].codecType = gCall[yALegPort].codecType;

				gCall[zOutboundCall].full_duplex =
					gCall[yALegPort].full_duplex;

				gCall[zOutboundCall].telephonePayloadType =
					gCall[yALegPort].telephonePayloadType;
			}

			msgRtpDetails.opcode = DMOP_RTPDETAILS;
			sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
					 LEG_B,
					 gCall[zOutboundCall].remoteRtpPort,
					 gCall[zOutboundCall].localSdpPort,
					 (gCall[zOutboundCall].
					  telephonePayloadPresent) ? gCall[zOutboundCall].
					 telephonePayloadType : -99,
					 gCall[zOutboundCall].full_duplex,
					 gCall[zOutboundCall].codecType,
					 gCall[zOutboundCall].remoteRtpIp);

			yRc = notifyMediaMgr (__LINE__, zOutboundCall,
								  (struct MsgToDM *) &(msgRtpDetails), mod);
		}

		//}
		//else if (msg.listenType == CONNECT)
		//{
		zOutboundCall = gCall[zCall].crossPort;

		gCall[zCall].appPid = msg.appPid;
		gCall[zOutboundCall].appPid = msg.appPid;

		msg.appCallNum2 = zOutboundCall;
		msg.appCallNum1 = zCall;
		msg.appCallNum = zCall;

		yRc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msg), mod);

		sprintf (response.message, "%d", zOutboundCall);

		while (gCall[zOutboundCall].remoteRtpPort == -1)
		{
			;
		}

		if (gCall[zOutboundCall].remoteRtpPort < 0)
		{
			response.returnCode = -23;

			if (gCall[zCall].callState == CALL_STATE_CALL_TRANSFERFAILURE)
			{
				response.returnCode =
					-1 *
					mapSipErrorToArcError (gCall[zCall].eXosipStatusCode);
				sprintf (response.message, "%d:%s",
						 mapSipErrorToArcError (gCall[zCall].
												eXosipStatusCode),
						 gCall[zCall].eXosipReasonPhrase);
			}

			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}

		time (&yCurrentTime);
		time (&yConnectTime);

		//msg = *(struct Msg_InitiateCall *)z;

		//zCall = msg.appCallNum;       

		memcpy (&response, &(gCall[zCall].msgToDM), sizeof (struct MsgToApp));

		yConnectTime = yConnectTime + 2 + msg.numRings * 6;

		sprintf (yStrFromUrl, "sip:arc@%s", gHostIp);

		//Check to see what kind of format we are receiving fromt the API
		//Then construct the appropriate destination strings

	char            yStrTmpIPAdrress[32];

	char           *pch = strchr (msg.ipAddress, '@');

		sprintf (yStrTmpIPAdrress, "%s", msg.ipAddress);

		pch = strstr (yStrTmpIPAdrress, "@");

		if (pch == NULL)
		{
			sprintf (yStrDestination, "sip:%s@%s;user=phone",
					 msg.phoneNumber, msg.ipAddress);
		}
		else
		{
			sprintf (yStrDestination, "sip:%s", msg.ipAddress);
		}

		sprintf (yStrFromUrl, "sip:arc@%s", gHostIp);

		eXosip_lock (geXcontext);

		yRc = eXosip_transfer_call2 (gCall[zCall].did, yStrDestination,
									 yStrFromUrl, yStrFromUrl);

		eXosip_unlock (geXcontext);

		if (yRc < 0)
		{
			response.returnCode = -1;
			writeGenericResponseToApp (__LINE__, zCall, &response, mod);
			pthread_detach (pthread_self ());
			pthread_exit (NULL);
			return (NULL);
		}

		gCall[zCall].lastError = CALL_NO_ERROR;

		response.returnCode = 0;

		while (gCall[zCall].callState != CALL_STATE_CALL_TRANSFERCOMPLETE)
		{
			usleep (10 * 1000);

			time (&yCurrentTime);

			if (yCurrentTime >= yConnectTime
				|| gCall[zCall].callState == CALL_STATE_CALL_TRANSFERFAILURE)
			{
				response.returnCode = -50;
				sprintf (response.message, "%d:%s", 50, "Ring no answer");

				if (gCall[zCall].callState == CALL_STATE_CALL_TRANSFERFAILURE)
				{
					response.returnCode =
						-1 *
						mapSipErrorToArcError (gCall[zCall].eXosipStatusCode);
					sprintf (response.message, "%d:%s",
							 mapSipErrorToArcError (gCall[zCall].
													eXosipStatusCode),
							 gCall[zCall].eXosipReasonPhrase);
				}

				writeGenericResponseToApp (__LINE__, zCall, &response, mod);

				pthread_detach (pthread_self ());
				pthread_exit (NULL);
				return (NULL);
			}

		}						/*END: while */

	int             yBLegPort = gCall[zCall].crossPort;

		setCallState (yBLegPort, CALL_STATE_CALL_CLOSED);
		//gCall[yBLegPort].callState = CALL_STATE_CALL_CLOSED;

		gDisconnectStruct.appCallNum = yBLegPort;
		gDisconnectStruct.opcode = DMOP_DISCONNECT;
		gDisconnectStruct.appPid = gCall[zCall].appPid;
		gDisconnectStruct.appPassword = gCall[zCall].appPassword;

		notifyMediaMgr (__LINE__, yBLegPort,
						(struct MsgToDM *) &gDisconnectStruct, mod);

		gDisconnectStruct.appCallNum = -1;
		gDisconnectStruct.appPid = -1;
		gDisconnectStruct.appPassword = -1;

		eXosip_lock (geXcontext);
		eXosip_terminate_call (gCall[yBLegPort].cid, gCall[yBLegPort].did);
		eXosip_unlock (geXcontext);

		response.returnCode = 0;
		sprintf (response.message, "0:Success");
		writeGenericResponseToApp (__LINE__, zCall, &response, mod);

	}

#endif
	pthread_detach (pthread_self ());
	pthread_exit (NULL);
	return (NULL);

}								/*END void * supervisedTransferCallThread */

int
parseFaxSdpMsg (int zCall, char *zDestInfo, char *zStrMessage)
{

	char            mod[] = "parseFaxSdpMsg";
	char            temp_sdp[4000];
	char            yStrLhs[256];
	char            yStrRhs[256];
	char           *yStrIndex, *yStrTmp, *yStrPayload;
	char           *yStrTok = NULL;

	char            toMediaMgrStr[MAX_APP_MESSAGE_DATA];
	char            tempStr[MAX_APP_MESSAGE_DATA];
	struct MsgToDM  yMsgToDM;

	yMsgToDM.opcode = DMOP_FAX_SEND_SDPINFO;
	yMsgToDM.appPid = gCall[zCall].msgToDM.appPid;
	yMsgToDM.appRef = gCall[zCall].msgToDM.appRef;
	yMsgToDM.appPassword = zCall;
	yMsgToDM.appCallNum = zCall;

	if (!zStrMessage)
	{
		sprintf (yMsgToDM.data, "%s", zDestInfo);
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
				   "Sending DMOP_FAX_SEND_SDPINFO(%d) with data field set to (%s).",
				   yMsgToDM.opcode, yMsgToDM.data);
		notifyMediaMgr (__LINE__, zCall, &yMsgToDM, mod);
		return (0);
	}

	sprintf (temp_sdp, "%s", zStrMessage);

	yStrIndex = strtok_r (temp_sdp, "\n\r", (char **) &yStrTok);

	memset ((char *) toMediaMgrStr, '\0', sizeof (toMediaMgrStr));
	memset ((char *) tempStr, '\0', sizeof (tempStr));
	while (yStrIndex != NULL)
	{
		memset (yStrLhs, 0, sizeof (yStrLhs));
		memset (yStrRhs, 0, sizeof (yStrRhs));

		yStrTmp = yStrIndex;

		sscanf (yStrTmp, "%[^=]=%s", yStrLhs, yStrRhs);

		if (strstr (yStrIndex, "T38FaxVersion") != NULL)
		{
			sscanf (yStrRhs, "T38FaxVersion:%d",
					&gCall[zCall].faxData.version);
			if (gCall[zCall].faxData.version != gT38FaxVersion)
			{
				if (toMediaMgrStr[0] == '\0')
				{
					sprintf (toMediaMgrStr, "%s", "T38FaxVersion:%d",
							 gCall[zCall].faxData.version);
				}
				else
				{
					sprintf (tempStr, "%s", "T38FaxVersion:%d",
							 gCall[zCall].faxData.version);
					strcat (toMediaMgrStr, tempStr);
				}
			}
		}
		else if (strstr (yStrIndex, "T38MaxBitRate") != NULL)
		{
			sscanf (yStrRhs, "T38MaxBitRate:%d",
					&gCall[zCall].faxData.bitrate);
			if (gCall[zCall].faxData.bitrate != gT38MaxBitRate)
			{
				if (toMediaMgrStr[0] == '\0')
				{
					sprintf (toMediaMgrStr, "%s", "T38MaxBitRate:%d",
							 gCall[zCall].faxData.bitrate);
				}
				else
				{
					sprintf (tempStr, "%s", "T38MaxBitRate:%d",
							 gCall[zCall].faxData.bitrate);
					strcat (toMediaMgrStr, tempStr);
				}
			}
		}
		else if (strstr (yStrIndex, "T38FaxFillBitRemoval") != NULL)
		{
			sscanf (yStrRhs, "T38aaxFillBitRemoval:%d",
					&gCall[zCall].faxData.fillBitRemoval);
			if (gCall[zCall].faxData.fillBitRemoval != gT38FaxFillBitRemoval)
			{
				if (toMediaMgrStr[0] == '\0')
				{
					sprintf (toMediaMgrStr, "%s", "T38FaxFillBitRemoval:%d",
							 gCall[zCall].faxData.fillBitRemoval);
				}
				else
				{
					sprintf (tempStr, "%s", "T38FaxFillBitRemoval:%d",
							 gCall[zCall].faxData.fillBitRemoval);
					strcat (toMediaMgrStr, tempStr);
				}
			}
		}
		else if (strstr (yStrIndex, "T38FaxTranscodingMMR") != NULL)
		{
			sscanf (yStrRhs, "T38FaxTranscodingMMR:%d",
					&gCall[zCall].faxData.transcodingMMR);
			if (gCall[zCall].faxData.transcodingMMR != gT38FaxTranscodingMMR)
			{
				if (toMediaMgrStr[0] == '\0')
				{
					sprintf (toMediaMgrStr, "%s", "T38FaxTranscodingMMR:%d",
							 gCall[zCall].faxData.transcodingMMR);
				}
				else
				{
					sprintf (tempStr, "%s", "T38FaxTranscodingMMR:%d",
							 gCall[zCall].faxData.transcodingMMR);
					strcat (toMediaMgrStr, tempStr);
				}
			}
		}
		else if (strstr (yStrIndex, "T38FaxTranscodingJBIG") != NULL)
		{
			sscanf (yStrRhs, "T38FaxTranscodingJBIG:%d",
					&gCall[zCall].faxData.transcodingJBIG);
			if (gCall[zCall].faxData.transcodingJBIG !=
				gT38FaxTranscodingJBIG)
			{
				if (toMediaMgrStr[0] == '\0')
				{
					sprintf (toMediaMgrStr, "%s", "T38FaxTranscodingJBIG:%d",
							 gCall[zCall].faxData.transcodingJBIG);
				}
				else
				{
					sprintf (tempStr, "%s", "T38FaxTranscodingJBIG:%d",
							 gCall[zCall].faxData.transcodingJBIG);
					strcat (toMediaMgrStr, tempStr);
				}
			}
		}
		else if (strstr (yStrIndex, "T38FaxRateManagement") != NULL)
		{
			// LocalTCF -> 0;  TransferredTCF -> 1
	int             myTCF = 0;

			sscanf (yStrRhs, "T38FaxRateManagement:%s",
					gCall[zCall].faxData.rateManagement);
			if (strcmp (gCall[zCall].faxData.rateManagement, "TransferredTCF")
				== 0)
			{
				myTCF = 1;
			}

			if (myTCF != gT38FaxRateManagement)
			{
				if (toMediaMgrStr[0] == '\0')
				{
					sprintf (toMediaMgrStr, "%s", "T38FaxRateManagement:%s",
							 gCall[zCall].faxData.rateManagement);
				}
				else
				{
					sprintf (tempStr, "%s", "T38FaxRateManagement:%s",
							 gCall[zCall].faxData.rateManagement);
					strcat (toMediaMgrStr, tempStr);
				}
			}
		}
		else if (strstr (yStrIndex, "T38FaxMaxBuffer") != NULL)
		{
			sscanf (yStrRhs, "T38FaxMaxBuffer:%d\n",
					&gCall[zCall].faxData.maxBuffer);
			if (gCall[zCall].faxData.maxBuffer != gT38FaxMaxBuffer)
			{
				if (toMediaMgrStr[0] == '\0')
				{
					sprintf (toMediaMgrStr, "T38FaxMaxBuffer:%d",
							 gCall[zCall].faxData.maxBuffer);
				}
				else
				{
					sprintf (tempStr, "T38FaxMaxBuffer:%d",
							 gCall[zCall].faxData.maxBuffer);
					strcat (toMediaMgrStr, tempStr);
				}
			}
		}
		else if (strstr (yStrIndex, "T38FaxMaxDatagram") != NULL)
		{
			sscanf (yStrRhs, "T38FaxMaxDatagram:%d\n",
					&gCall[zCall].faxData.maxDatagram);
			if (gCall[zCall].faxData.maxDatagram != gT38FaxMaxDataGram)
			{
				if (toMediaMgrStr[0] == '\0')
				{
					sprintf (toMediaMgrStr, "T38FaxMaxDatagram:%d",
							 gCall[zCall].faxData.maxDatagram);
				}
				else
				{
					sprintf (tempStr, "T38FaxMaxDatagram:%d",
							 gCall[zCall].faxData.maxDatagram);
					strcat (toMediaMgrStr, tempStr);
				}
			}
		}
#if 0
		else if (strstr (yStrIndex, "T38FaxUdpEC") != NULL)
		{
			sscanf (yStrRhs, "T38FaxUdpEC:%s", gCall[zCall].faxData.udpEC);
			if (gCall[zCall].faxData.udpEC != gT30ErrorCorrection)
			{
				if (toMediaMgrStr[0] == '\0')
				{
					sprintf (toMediaMgrStr, "T38FaxUdpEC:%s",
							 gCall[zCall].faxData.udpEC);
				}
				else
				{
					sprintf (tempStr, "T38FaxUdpEC:%s",
							 gCall[zCall].faxData.udpEC);
					strcat (toMediaMgrStr, tempStr);
				}
			}
		}
#endif
		else
		{
			;
		}

		yStrIndex = strtok_r (NULL, "\n", (char **) &yStrTok);

	}							/*while yStrIndex */

	if (toMediaMgrStr[0] != '\0')
	{
		sprintf (yMsgToDM.data, "%s", zDestInfo);
	}
	else
	{
		sprintf (yMsgToDM.data, "%s%s", zDestInfo, toMediaMgrStr);
	}
	notifyMediaMgr (__LINE__, zCall, &yMsgToDM, mod);
	return 0;

}								/*END: int parseFaxSdpMsg() */

int
setSendReceiveStatus (int zCall, char *zStrMessage)
{
	char            mod[] = "setSendReceiveStatus";
	int             isSendRecv = 1;
	char			tmpStrMessage[4096];

	sprintf(tmpStrMessage, "%s", zStrMessage);

	removeVideo(tmpStrMessage);			// BT-160
	if ( strcmp(zStrMessage, tmpStrMessage) )
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			"Video media descriptions successfully removed from SDP info.  SDP is now (%s)",
			tmpStrMessage);
	}

	if (tmpStrMessage != NULL)
	{
		if (strstr (tmpStrMessage, "sendonly") != NULL)
		{
			isSendRecv = 0;
			gCall[zCall].sendRecvStatus = SENDONLY;
		}
		if (strstr (tmpStrMessage, "recvonly") != NULL)
		{
			isSendRecv = 0;
			gCall[zCall].sendRecvStatus = RECVONLY;
		}
		if (strstr (tmpStrMessage, "inactive") != NULL)
		{
			isSendRecv = 0;
			gCall[zCall].sendRecvStatus = INACTIVE;
		}

		if (isSendRecv == 0)
		{
			gCall[zCall].isCallAnswered_noMedia = 1;
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Got sendonly in SDP message, waiting for sendrecv.");
		}
		else
		{
			gCall[zCall].isCallAnswered_noMedia = 0;
			gCall[zCall].sendRecvStatus = SENDRECV;
			gCall[zCall].full_duplex = SENDRECV;
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Got sendrecv in SDP message.");
			// "Got sendrecv in SDP message, should start media threads now.");
		}
	}
	return 0;
}

/*-----------------------------------------------------------------------------
	// BT-160
  ---------------------------------------------------------------------------*/
static void removeVideo(char *zSDPMessage)
{
	char	*pVideo;
	char	*pOther;

	if ( ( zSDPMessage == NULL ) || ( zSDPMessage[0] == '\0' ) )
	{
		return;
	}

	if (strstr (zSDPMessage, "m=video") == NULL)
	{
		return;
	}

	for(;;)
	{
		if ((pVideo=strstr(zSDPMessage, "m=video")) == NULL)
		{
			return;
		}

//		fprintf(stderr, "DJB: [%d]\n", __LINE__);
//		fprintf(stderr, "\n------------   pVideo  ------------\n%s\n", pVideo);
//		fprintf(stderr, "DJB: [%d]\n", __LINE__);
		if ((pOther=strstr ((pVideo+1), "m=")) == NULL)
		{
			*pVideo='\0';
			return;
		}

//		fprintf(stderr, "\n--------- pOther --------------\n%s\n", pOther);

		// 
		// At this point, m=video either came first or there is something else 
		// that came later.  Need to remove the video and keep everything after that.
		//
		sprintf(pVideo, "%s", pOther);
	}
}  // END: removeVideo()

///This function parses the sdp body sent to it through zStrMessage and finds out what codecs are used for this call.
int
setAudioCodecString (int zCall, int zIntCodec)
{
	char            mod[] = "setAudioCodecString";

	switch (zIntCodec)
	{

	case 8:
		sprintf (gCall[zCall].audioCodecString, "PCMA");
		break;

	case 3:
		sprintf (gCall[zCall].audioCodecString, "GSM");
		break;

	case 110:
		sprintf (gCall[zCall].audioCodecString, "SPEEX");
		break;

	case 111:
		sprintf (gCall[zCall].audioCodecString, "SPEEX");
		break;
	case 4:
		sprintf (gCall[zCall].audioCodecString, "G724");
		break;
	case 18:
		sprintf (gCall[zCall].audioCodecString, "G729");
		break;
	case 9:
		sprintf (gCall[zCall].audioCodecString, "G722");
		break;

	case 0:
	default:
		sprintf (gCall[zCall].audioCodecString, "711r");
		break;

	}

	if (zIntCodec == gCall[zCall].amrIntCodec[0])
	{
		sprintf (gCall[zCall].audioCodecString, "AMR");
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO,
			   "Setting audiocodec string gCall[%d].audioCodecString=%s, zIntCodec(%d)",
			   zCall,gCall[zCall].audioCodecString, zIntCodec);

	return (0);

}								/*END: int setAudioCodecString */

int
parseAMRFmtp (int zCall, char *amr_fmtpMessage)
{
	char            mod[] = "parseAMRFmtp";
	char            lFmtpMsg[256];
	int             isOctectAlignPresent = 1;
	int             isFmtpGood = 1;
	char           *yStrTok;
	char           *yStrIndex = NULL;
	char            yStrLhs[256];
	char            yStrRhs[256];

	sprintf (lFmtpMsg, "%s", amr_fmtpMessage);

	yStrIndex = strtok_r (lFmtpMsg, ";", (char **) &yStrTok);

	while (yStrIndex != NULL)
	{
		memset (yStrLhs, 0, sizeof (yStrLhs));
		memset (yStrRhs, 0, sizeof (yStrRhs));

		sscanf (yStrIndex, "%[^=]=%s", yStrLhs, yStrRhs);

		if (strcmp (yStrLhs, "octet-align") == 0)
		{
#if 0
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "octet-align=%s", yStrRhs);
	int             octectAlign = atoi (yStrRhs);

			if (octectAlign == 1)
			{
				isOctectAlignPresent = 1;
			}
#endif
		}
		else if (strcmp (yStrLhs, "crc") == 0)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "crc=%s", yStrRhs);
			if (strcmp (yStrRhs, "1") == 0)
			{
				isFmtpGood = 0;
			}
		}
		else if (strcmp (yStrLhs, "robust-sorting") == 0)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "robust-sorting=%s", yStrRhs);
			if (strcmp (yStrRhs, "1") == 0)
			{
				isFmtpGood = 0;
			}
		}
		else if (strcmp (yStrLhs, "interleaving") == 0)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "interleaving=%s", yStrRhs);
			isFmtpGood = 0;
		}
		else if (strcmp (yStrLhs, "channels") == 0)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "channel=%s", yStrRhs);
	int             channels = atoi (yStrRhs);

			if (channels >= 2)
			{
				isFmtpGood = 0;
			}
		}

		yStrIndex = strtok_r (NULL, "; ", (char **) &yStrTok);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "yStrIndex=%s", yStrIndex);

	}							/*END: while() */

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "isFmtpGood=%d, isOctectAlignPresent=%d.",
			   isFmtpGood, isOctectAlignPresent);

	if (isFmtpGood == 0 || isOctectAlignPresent == 0)
	{
		return -1;
	}

	return 0;
}

int
getAMRChannel (char *amrString)
{
	char            lAmrStr[128] = "";

	sprintf (lAmrStr, "%s", amrString);

	//a=rtpmap:97 AMR/8000/1

	if (strstr (lAmrStr, "AMR/8000/") != NULL)
	{
		if (strstr (lAmrStr, "AMR/8000/1") != NULL)
		{
		}
		else
		{
			return -1;
		}
	}

	return 0;
}

int
removeOctectAlign (char *zStrFmtpLine, char *tempFmtpLine)
{
	char            mod[] = "removeOctectAlign";

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Old FMTP msg=(%s).", zStrFmtpLine);

	if (zStrFmtpLine != NULL)
	{
	int             fmtplen = strlen (zStrFmtpLine);
	char           *octectAlignStart = strstr (zStrFmtpLine, "octet-align");

		if (octectAlignStart != NULL)
		{
	int             len1 = strlen (octectAlignStart);
	char            lStrFmtpLine[512] = "";

			snprintf (lStrFmtpLine, fmtplen - len1, "%s", zStrFmtpLine);
			sprintf (tempFmtpLine, "%s%s", lStrFmtpLine,
					 octectAlignStart + 13);

			dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Old FMTP msg (%s) new FMTP Message (%s).",
					   zStrFmtpLine, tempFmtpLine);
		}
	}

	return 0;
}

int
parseSdpMessage (int zCall, int zLine, char *zStrMessage, sdp_message_t * remote_sdp)
{
	char            mod[] = "parseSdpMessage";

	char            temp_sdp[4000];

	char            yStrLhs[256];
	char            yStrRhs[256];
	char            yStrRemoteIp[256];
	char            yStrMessage[256];
	char            yStrAmrFmtpSearch[256];
	int             g729AnnexMismatch = NO;

	char           *yStrTok;

	char           *ypChrEqual = NULL;

	int             yIntTelephonePayloadType;
	int             yIntAmrCodec;

	char           *yStrIndex, *yStrTmp, *yStrPayload;

	int             yIntFull_Duplex = 1;

	int             yIntVoiceCodecType = 0;

	int             yIntVoiceCodecTypeReceived = 0;

	int             yIntAmrCodecSet = 0;

	int             yIntCodecNeeded[10] =
		{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
	char            yIntAMRIndexNeeded[10] =
		{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
	int             yIntCodecReceived[10] =
		{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
	int             yIntAMRIndexReceived[10] =
		{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

	int             yIntCodecReceivedCounter = 0;
	int             yIntMediaSize = 0;

	char            yAmrFmtpMessage[10][256];
	int             amrPayLoadCount = 0;

	char           *amrFmtp[10];

	yStrAmrFmtpSearch[0] = 0;

	__DDNDEBUG__ (DEBUG_MODULE_SIP, "Parsing SDP message", "", zCall);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "Inside %s.  Called from %d.", mod, zLine);

	/*DDN_FAX: 02/25/2010 */
	gCall[zCall].faxData.isFaxCall = 0;

	if (!remote_sdp)
	{
		return (-1);
	}

	if (!zStrMessage)
	{
		return (-1);
	}

	if (strlen (zStrMessage) > 2047)		// BT-17
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_SIP_SIGNALING,
				   ERR,
				   "Invalid SDP message length (%d); max value is 2048. "
				   "Unable to parse SDP message", strlen (zStrMessage));
		return (-1);
	}


	sprintf (temp_sdp, "%s", zStrMessage);

	removeVideo(temp_sdp);			// BT-160
	if ( strcmp(zStrMessage, temp_sdp) )
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			"Video media descriptions successfully removed from SDP info.  SDP is now (%s)",
			temp_sdp);
	}
	yStrIndex = strtok_r (temp_sdp, "\n\r", (char **) &yStrTok);

	if (gCall[zCall].isInitiate == 1)
	{
		gCall[zCall].codecType = 0;
	}

	if (strcmp (g729Annexb, "NO") == 0 || strcmp (g729Annexb, "no") == 0)
	{
		gCall[zCall].isG729AnnexB = NO;
		if (strstr (zStrMessage, "annexb=yes") != NULL)
		{
			g729AnnexMismatch = YES;
			//return -1;    
		}
	}
	else if (strcmp (g729Annexb, "YES") == 0 ||
			 strcmp (g729Annexb, "yes") == 0)
	{
		if (strstr (zStrMessage, "annexb=no") != NULL)
		{
			gCall[zCall].isG729AnnexB = NO;
			g729AnnexMismatch = YES;
			//return -1;
		}
	}

	if (gAMRIndex[0] != '\0')
	{
	int             yIntCodecNeededCounter = 0;
	int             yIntCodecNeededLoopCounter = 0;

		if (strstr (gAMRIndex, "0") != NULL)
		{
			yIntAMRIndexNeeded[yIntCodecNeededCounter] = '0';
			yIntCodecNeededCounter++;
		}

		if (strstr (gAMRIndex, "1") != NULL)
		{
			yIntAMRIndexNeeded[yIntCodecNeededCounter] = '1';
			yIntCodecNeededCounter++;
		}

		if (strstr (gAMRIndex, "2") != NULL)
		{
			yIntAMRIndexNeeded[yIntCodecNeededCounter] = '2';
			yIntCodecNeededCounter++;
		}

		if (strstr (gAMRIndex, "3") != NULL)
		{
			yIntAMRIndexNeeded[yIntCodecNeededCounter] = '3';
			yIntCodecNeededCounter++;
		}

		if (strstr (gAMRIndex, "4") != NULL)
		{
			yIntAMRIndexNeeded[yIntCodecNeededCounter] = '4';
			yIntCodecNeededCounter++;
		}
		if (strstr (gAMRIndex, "5") != NULL)
		{
			yIntAMRIndexNeeded[yIntCodecNeededCounter] = '5';
			yIntCodecNeededCounter++;
		}
		if (strstr (gAMRIndex, "6") != NULL)
		{
			yIntAMRIndexNeeded[yIntCodecNeededCounter] = '6';
			yIntCodecNeededCounter++;
		}
		if (strstr (gAMRIndex, "7") != NULL)
		{
			yIntAMRIndexNeeded[yIntCodecNeededCounter] = '7';
			yIntCodecNeededCounter++;
		}
		if (strstr (gAMRIndex, "8") != NULL)
		{
			yIntAMRIndexNeeded[yIntCodecNeededCounter] = '8';
			yIntCodecNeededCounter++;
		}

	}

	while (yStrIndex != NULL)
	{
		memset (yStrLhs, 0, sizeof (yStrLhs));
		memset (yStrRhs, 0, sizeof (yStrRhs));

		yStrTmp = yStrIndex;

		snprintf (yStrLhs, 255, "%s", yStrTmp);
		ypChrEqual = strchr (yStrLhs, '=');

		if (ypChrEqual != NULL)
		{
			ypChrEqual[0] = 0;

			snprintf (yStrRhs, 255, "%s", ypChrEqual + 1);
		}
		else
		{
			yStrIndex = strtok_r (NULL, "\n", (char **) &yStrTok);
			continue;
		}

		//sscanf (yStrTmp, "%[^=]=%s", yStrLhs, yStrRhs);

/**
		 Get the telephone event payload type
		 Or if it is an Initiate Call get the voice codec
		 We will also now grab the remoteRtpIp from this function
*/

#if 1
		__DDNDEBUG__ (DEBUG_MODULE_SIP, "Parsing SDP message: Line=",
					  yStrIndex, zCall);
#endif

		if ((strstr (yStrIndex, "amr/8000") != NULL ||
			 strstr (yStrIndex, "AMR/8000") != NULL))
		{

			__DDNDEBUG__ (DEBUG_MODULE_SIP, "going to set AMR intcodec", "",
						  gCall[zCall].amrIntCodec[amrPayLoadCount]);

	int             rc = getAMRChannel (yStrIndex);

			if (rc != 0)
			{
				gCall[zCall].amrIntCodec[amrPayLoadCount] = -2;
			}
			else
			{
				gCall[zCall].amrIntCodec[amrPayLoadCount] = yIntAmrCodec;
			}

			yIntAmrCodecSet = 1;
			sscanf (yStrRhs, "rtpmap:%d", &yIntAmrCodec);
			gCall[zCall].amrIntCodec[amrPayLoadCount] = yIntAmrCodec;

			__DDNDEBUG__ (DEBUG_MODULE_SIP, "Got AMR codec type", "",
						  gCall[zCall].amrIntCodec[amrPayLoadCount]);

			//sprintf(yStrAmrFmtpSearch, "%d mode-set=", gCall[zCall].amrIntCodec[amrPayLoadCount]);
			sprintf (yStrAmrFmtpSearch, "a=fmtp:%d",
					 gCall[zCall].amrIntCodec[amrPayLoadCount]);

		}
		else if (!gCall[zCall].telephonePayloadPresent
				 && strstr (yStrIndex, "telephone-event") != NULL)
		{
			gCall[zCall].telephonePayloadType = 96;
			gCall[zCall].telephonePayloadPresent = 1;

			sscanf (yStrRhs, "rtpmap:%d", &yIntTelephonePayloadType);

			__DDNDEBUG__ (DEBUG_MODULE_SIP, "Got DTMF Payload Type", "",
						  yIntTelephonePayloadType);

			if(yIntTelephonePayloadType > 127)
			{
				yIntTelephonePayloadType = 101;
			}

			gCall[zCall].telephonePayloadType = yIntTelephonePayloadType;
		}
		else if (yIntAmrCodecSet == 1 && yStrAmrFmtpSearch[0]
				 && strstr (yStrIndex, yStrAmrFmtpSearch) != NULL)
		{
	char           *yStrFmtpLine = strstr (yStrIndex, yStrAmrFmtpSearch);
	char            tempFmtpLine[512] = "";

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "yStrFmtpLine=(%s)", yStrFmtpLine);

			if (strstr (yStrFmtpLine, "octet-align") != NULL)
			{
				removeOctectAlign (yStrFmtpLine, tempFmtpLine);
				sprintf (gCall[zCall].amrFmtp, "%s", tempFmtpLine);
				sprintf (yAmrFmtpMessage[amrPayLoadCount], "%s",
						 tempFmtpLine);
			}
			else
			{
				sprintf (gCall[zCall].amrFmtp, "%s", yStrFmtpLine);
				sprintf (yAmrFmtpMessage[amrPayLoadCount], "%s",
						 yStrFmtpLine);
			}
			amrPayLoadCount++;
			yIntAmrCodecSet = 0;
		}

		if (strstr (yStrIndex, "c=IN") != NULL)
		{
	char           *tmp = strstr (yStrIndex, "IP4 ");

			if (tmp == NULL)
			{
				tmp = strstr (yStrIndex, "IP6 ");
			}

			if (tmp == NULL)
			{
				return (-1);
			}

			tmp = tmp + 4;
			sprintf (yStrRemoteIp, "%s", tmp);

			__DDNDEBUG__ (DEBUG_MODULE_SIP, "Got Remote RTP IP", yStrRemoteIp, zCall);
		}

		/*DDN_FAX: 02/25/2010 */
		if (strstr (yStrIndex, "m=image") != NULL)
		{
			gCall[zCall].faxData.isFaxCall = 1;
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, FAX_BASE, INFO,
					   "Set faxData.isFaxCall=%d.",
					   gCall[zCall].faxData.isFaxCall);
		}
		/*END: DDN_FAX: 02/25/2010 */
		yStrIndex = strtok_r (NULL, "\n", (char **) &yStrTok);

	}							/*END: while() */

	/*Parse the SDP structure (osip_message_t) to negotiate the codec. */
	yIntMediaSize = osip_list_size (&remote_sdp->m_medias);

	if (yIntMediaSize == 0)
	{

		__DDNDEBUG__ (DEBUG_MODULE_SIP, "No m= line found in SDP.", "",
					  zCall);

		return (-1);
	}
	else
	{
	int             i = 0;

	char           *yStrVoiceCodecFromStruct = NULL;

		for (int i = 0; i < yIntMediaSize; i++)
		{
	int             pos = 0;

			if (remote_sdp == NULL)
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE,
						   WARN, "remote_sdp is NULL");
				return -1;
			}

	sdp_media_t    *remote_sdp_media =
		(sdp_media_t *) osip_list_get (&remote_sdp->m_medias, i);

			if (remote_sdp_media)
			{
	osip_list_t    *m_payloads = &remote_sdp_media->m_payloads;

				while (!osip_list_eol (m_payloads, pos))
				{
					yStrVoiceCodecFromStruct =
						(char *) osip_list_get (m_payloads, pos);

					if (yStrVoiceCodecFromStruct)
					{
						yIntCodecReceived[yIntCodecReceivedCounter] =
							atoi (yStrVoiceCodecFromStruct);
						yIntCodecReceivedCounter++;
					}

					pos++;

					__DDNDEBUG__ (DEBUG_MODULE_SIP, "Got Voice Codec",
								  yStrVoiceCodecFromStruct, i);

				}				/*while osip_list_eol */

			}					/*if(remote_sdp_media) */
		}
	}

	gCall[zCall].full_duplex = gCall[zCall].sendRecvStatus;

	gCall[zCall].codecType = yIntVoiceCodecType;

	gCall[zCall].remoteRtpIp[0] = '\0';

	snprintf (gCall[zCall].remoteRtpIp, sizeof (gCall[zCall].remoteRtpIp),
			  "%s", yStrRemoteIp);

	if (gCall[zCall].GV_PreferredCodec[0] != '\0')
	{
		if (strstr (gCall[zCall].GV_PreferredCodec, "G729") != NULL)
		{
			if (g729AnnexMismatch == NO)
			{
				yIntCodecNeeded[0] = 18;
			}
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "G724") != NULL)
		{
			yIntCodecNeeded[0] = 4;
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "PCMU") != NULL)
		{
			yIntCodecNeeded[0] = 0;
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "PCMA") != NULL)
		{
			yIntCodecNeeded[0] = 8;
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "GSM") != NULL)
		{
			yIntCodecNeeded[0] = 3;
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "AMR") != NULL ||
				 strstr (gCall[zCall].GV_PreferredCodec, "amr") != NULL)
		{
			yIntCodecNeeded[0] = gCall[zCall].amrIntCodec[0];
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "SPEEX") != NULL)
		{
			if (strstr (gCall[zCall].GV_PreferredCodec, "8000"))
			{
				yIntCodecNeeded[0] = 110;
			}
			else
			{
				yIntCodecNeeded[0] = 111;
			}
		}
		else if (strstr (gCall[zCall].GV_PreferredCodec, "G722") != NULL)
		{
			yIntCodecNeeded[0] = 9;
		}

#if 0
		if (yIntCodecNeeded[0] != gCall[zCall].codecType)
		{
			return -1;
		}
#endif

		for (int i = 0; i < yIntCodecReceivedCounter; i++)
		{
			if ((yIntCodecNeeded[0] == yIntCodecReceived[i])
				&& (yIntCodecReceived[i] >= 0))
			{
				gCall[zCall].codecType = yIntCodecReceived[i];
				setAudioCodecString (zCall, gCall[zCall].codecType);
				return (0);
			}
		}

		return (-1);

	}
	else if (gPreferredCodec[0] != '\0')
	{
	int             yIntCodecNeededCounter = 0;
	int             yIntCodecNeededLoopCounter = 0;

		if (strstr (gPreferredCodec, "G729") != NULL)
		{
			if (g729AnnexMismatch == NO)
			{
				yIntCodecNeeded[yIntCodecNeededCounter] = 18;
				yIntCodecNeededCounter++;
			}
		}

#if 0
		if (strstr (gPreferredCodec, "G723") != NULL)
		{
			yIntCodecNeeded[yIntCodecNeededCounter] = 4;
			yIntCodecNeededCounter++;
		}
#endif

		if (strstr (gPreferredCodec, "PCMU") != NULL)
		{
			yIntCodecNeeded[yIntCodecNeededCounter] = 0;
			yIntCodecNeededCounter++;
		}

		if (strstr (gPreferredCodec, "PCMA") != NULL)
		{
			yIntCodecNeeded[yIntCodecNeededCounter] = 8;
			yIntCodecNeededCounter++;
		}

		if (strstr (gPreferredCodec, "GSM") != NULL)
		{
			yIntCodecNeeded[yIntCodecNeededCounter] = 3;
			yIntCodecNeededCounter++;
		}

		if (strstr (gPreferredCodec, "AMR") != NULL ||
			strstr (gPreferredCodec, "amr") != NULL)
		{
			yIntCodecNeeded[yIntCodecNeededCounter] =
				gCall[zCall].amrIntCodec[0];
			yIntCodecNeededCounter++;
		}

		if (strstr (gPreferredCodec, "G722") != NULL)
		{
			yIntCodecNeeded[yIntCodecNeededCounter] = 9;
			yIntCodecNeededCounter++;
		}

		if (strstr (gPreferredCodec, "SPEEX") != NULL)
		{
			if (strstr (gPreferredCodec, "8000"))
			{
				yIntCodecNeeded[yIntCodecNeededCounter] = 110;
				yIntCodecNeededCounter++;
			}
			else
			{
				yIntCodecNeeded[yIntCodecNeededCounter] = 111;
				yIntCodecNeededCounter++;
			}
		}

		for (yIntCodecNeededLoopCounter; yIntCodecNeededLoopCounter < 5;
			 yIntCodecNeededLoopCounter++)
		{

			for (int i = 0; i < yIntCodecReceivedCounter; i++)
			{

				sprintf (yStrMessage,
						 "Checking config codec (%d) with sdp codec (%d)",
						 yIntCodecNeeded[yIntCodecNeededLoopCounter],
						 yIntCodecReceived[i]);
				__DDNDEBUG__ (DEBUG_MODULE_RTP, yStrMessage, "", zCall);

				if (yIntCodecNeeded[yIntCodecNeededLoopCounter] ==
					yIntCodecReceived[i] && (yIntCodecReceived[i] >= 0))
				{
					gCall[zCall].codecType = yIntCodecReceived[i];
					setAudioCodecString (zCall, gCall[zCall].codecType);
					setSendReceiveStatus (zCall, zStrMessage);

					if (strcmp (gCall[zCall].audioCodecString, "AMR") == 0)
					{
						for (int count = 0; count < amrPayLoadCount; count++)
						{
	int             isParamFailed = 0;

							sprintf (gCall[zCall].amrFmtp, "%s",
									 yAmrFmtpMessage[count]);

							gCall[zCall].amrIntCodec[0] =
								gCall[zCall].amrIntCodec[count];
							gCall[zCall].codecType =
								gCall[zCall].amrIntCodec[count];

	char           *set_mode = strstr (gCall[zCall].amrFmtp, "mode-set=");

							if (set_mode != NULL)
							{
								set_mode = set_mode + strlen ("mode-set=");

								if (strchr (set_mode, ';') != NULL)
								{
	char           *semi_colon = strchr (set_mode, ';') + 2;

									sprintf (gCall[zCall].amrFmtpParams, "%s",
											 semi_colon);
									dynVarLog (__LINE__, zCall, mod,
											   REPORT_VERBOSE, TEL_BASE, INFO,
											   "amrFmtpParams=(%s).",
											   gCall[zCall].amrFmtpParams);

	int             rc = parseAMRFmtp (zCall, gCall[zCall].amrFmtpParams);

									*semi_colon = 0;
									if (rc != 0)
									{
										//return -1;
										isParamFailed = 1;
										//continue;
									}

								}
							}
							if (set_mode == NULL || set_mode[0] == '\0')
							{
								if (gCall[zCall].amrIntCodec[0] == -2)
								{
									//  isParamFailed = 1;
									continue;
								}

	int             length = 2;

								if (gCall[zCall].amrIntCodec[count] >= 100)
								{
									length = 3;
								}
	char           *semi_colon =
		gCall[zCall].amrFmtp + strlen ("a=fmtp:") + length + 1;
								__DDNDEBUG__ (DEBUG_MODULE_RTP,
											  gCall[zCall].amrFmtp, "",
											  zCall);
								if (semi_colon != NULL)
								{
									sprintf (gCall[zCall].amrFmtpParams, "%s",
											 semi_colon);

									dynVarLog (__LINE__, zCall, mod,
											   REPORT_VERBOSE, TEL_BASE, INFO,
											   "amrFmtpParams=(%s).",
											   gCall[zCall].amrFmtpParams);

	int             rc = parseAMRFmtp (zCall, gCall[zCall].amrFmtpParams);

									*semi_colon = 0;
									if (rc != 0)
									{
										isParamFailed = 1;
										//continue;
										//return -1;
									}
								}

								gCall[zCall].amrIntIndex =
									yIntAMRIndexNeeded[0] - 48;
								return 0;
							}
							for (int i = 0; i < 10; i++)
							{
								if (yIntAMRIndexNeeded[i] == -1)
								{
									continue;
									//return -1;
								}
								if (strchr (set_mode, yIntAMRIndexNeeded[i])
									!= NULL)
								{
									gCall[zCall].amrIntIndex =
										yIntAMRIndexNeeded[i] - 48;
									// -48 to chnage the ascii value to integer
									if (isParamFailed == 1
										|| gCall[zCall].amrIntCodec[0] == -2)
									{
										return -1;
									}
									return 0;
								}
							}
						}
						gCall[zCall].amrIntCodec[0] =
							gCall[zCall].amrIntCodec[0] + 1;
						gCall[zCall].codecType = gCall[zCall].amrIntCodec[0];
						gCall[zCall].amrIntIndex = yIntAMRIndexNeeded[0] - 48;
						setAudioCodecString (zCall, gCall[zCall].codecType);
						//createAMRFmtp();
						return 0;

					}
					if (g729AnnexMismatch == YES
						&& gCall[zCall].codecType == 18)
					{
						return -1;
					}
					return 0;
				}
			}

		}
		return -1;
	}

	setAudioCodecString (zCall, gCall[zCall].codecType);
	setSendReceiveStatus (zCall, zStrMessage);

	gCall[zCall].full_duplex = gCall[zCall].sendRecvStatus;
	if (g729AnnexMismatch == YES && gCall[zCall].codecType == 18)
	{
		return -1;
	}

	return (0);

}								/*END: int parseSdpMessage */

int
getRtpPortsFromMediaMgr ()
{
char            mod[] = "getRtpPortsFromMediaMgr";
int             yIntCounter = 1000;

	dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, INFO,
			   "Waiting for RTP Ports from Media Manager.");

	while (gIsRtpPortSet == NO)
	{
		usleep (100 * 1000);	//wait

		if (!yIntCounter--)
		{
			gCanReadSipEvents = 0;
			gCanReadRequestFifo = 0;
			dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, INFO,
					   "No RTP Ports received from Media Manager.");

			break;
		}
	}

	dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE, INFO,
			   "Done waiting for RTP Ports from Media Manager.");

	return(0);
}								/*int getRtpPortsFromMediaMgr */


int
canDynMgrExit ()
{
    int             yIntExit = -99;

	for (int i = gStartPort; i <= gEndPort; i++)
	{
		if (gCall[i].callState != CALL_STATE_CALL_CLOSED
			&& gCall[i].callState != CALL_STATE_CALL_CANCELLED
			&& gCall[i].callState != CALL_STATE_CALL_TERMINATE_CALLED
			&& gCall[i].callState != CALL_STATE_RESERVED_NEW_CALL
			&& gCall[i].callState != CALL_STATE_CALL_RELEASED
			&& gCall[i].callState != CALL_STATE_IDLE)
		{
			yIntExit = gCall[i].callState;
			// break; because of the below we need to loop through completely 
		} else {
           // added joes, minimalist way to gracefully shutdown ports while a shutdown is in progress 
           if(gShutDownFlag == 1){
             // fprintf(stderr, " %s() shutting down port %d due to shutdown and recycle...\n", __func__, i);
             writeToRespShm (__LINE__, (char *)__func__, tran_tabl_ptr, i, APPL_STATUS, STATUS_INIT);
           }
        }
	}


	return yIntExit;
}


/*
  moved here to keep track of who does what from where ....  
*/


int
doDynMgrExit(int zCall, const char *func, int lineno){

   int rc = -1;
   int i;
   int call_id;
   int call_did;

   dynVarLog (__LINE__, zCall, (char *) __func__,
       REPORT_NORMAL,
       TEL_CALLMGR_SHUTDOWN, WARN,
       " %s() Called from line[%d] Restarting ArcIPDynMgr (%d). gCallsStarted=%d gCallsFinished=%d",
       __func__, lineno, gDynMgrId, gCallsStarted, gCallsFinished);


   for (int i = gStartPort; i <= gEndPort; i++){
		if (gCall[i].callState != CALL_STATE_CALL_CLOSED
			&& gCall[i].callState != CALL_STATE_CALL_CANCELLED
			&& gCall[i].callState != CALL_STATE_CALL_TERMINATE_CALLED
			&& gCall[i].callState != CALL_STATE_RESERVED_NEW_CALL
			&& gCall[i].callState != CALL_STATE_CALL_RELEASED
			&& gCall[i].callState != CALL_STATE_IDLE)
		{

		rc = SendMediaManagerPortDisconnect (i, 1); 			// BT-111

     if(gCall[i].cid != -1 || gCall[i].did != -1){

    dynVarLog (__LINE__, i, (char *) __func__,
//       REPORT_NORMAL, TEL_CALLMGR_SHUTDOWN, ERR, 
       REPORT_NORMAL, TEL_CALLMGR_SHUTDOWN, WARN, 		// MR # 4982
       " %s() Call state is not cleared for this port, attempting to send a BYE or CANCEL for this Call on hard shutdown of ArcIPDynMgr, CallId-%d DialogId=%d",
     __func__, gCall[i].cid, gCall[i].did);

         eXosip_lock (geXcontext);
         eXosip_call_terminate (geXcontext, gCall[i].cid, gCall[i].did);
         time (&(gCall[zCall].lastReleaseTime));
         eXosip_unlock (geXcontext);
        }
	 }
   }

   sleep(5);
   gCanReadSipEvents = 0;
   sleep (5);
   gCanReadRequestFifo = 0;

   return rc;
}

/*
 *
 *
 */

void
unlinkFiles ()
{

char            yStrFileName[256];

	sprintf (yStrFileName, ".portStatus.%d", gDynMgrId);

	unlink (yStrFileName);
}

int
createNetworkAnnouncementVxmlURIScript (int zCall)
{
	int             yRc;
	char            mod[] = "createNetworkAnnouncementVxmlURIScript";

	char            yStrScriptName[256];

	FILE           *fpNetAnn = NULL;

	sprintf (yStrScriptName, "vxi/networkAnnouncement.%d.vxml", zCall);

	fpNetAnn = fopen (yStrScriptName, "w+");

	if (!fpNetAnn)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
				   ERR,
				   "fopen(%s) failed. [%d, %s] "
				   "Unable to open net-ann script; rejecting the call.",
				   yStrScriptName, errno, strerror (errno));
		return -1;
	}

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Got GV_SipUriVoiceXML=%s.", gCall[zCall].GV_SipUriVoiceXML);

	/*Dynamically generated script */
	fprintf (fpNetAnn,
			 ""
			 "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>"
			 "<vxml version=\"2.0\" xmlns=\"http://www.w3.org/2001/vxml\">"
			 "	<form id=\"frmNetAnnURI\">"
			 "		<block><goto expr=\"unescape(\'%s\')\" /></block>"
			 "	</form>" "</vxml>" "", gCall[zCall].GV_SipUriVoiceXML);

	fclose (fpNetAnn);

	fpNetAnn = NULL;

	return 0;

}								/*END: int createNetworkAnnouncementVxmlScript */

int
createNetworkAnnouncementVxmlScript (int zCall,
									 osip_uri_param_t * repeat_param,
									 osip_uri_param_t * delay_param)
{
	int             yRc;
	char            mod[] = "createNetworkAnnouncementVxmlScript";

	char            yStrScriptName[256];

	FILE           *fpNetAnn = NULL;

	int             yIntRepeatCounter = 0;

	int             yIntRepeat = 1;
	int             yIntDelay = 0;

	if (repeat_param && repeat_param->gvalue)
	{
		if (strcmp (repeat_param->gvalue, "forever") == 0)
		{
			yIntRepeat = 10;
		}
		else
		{
			yIntRepeat = atoi (repeat_param->gvalue);
		}

		if (yIntRepeat < 1)
		{
			yIntRepeat = 1;
		}

		if (yIntRepeat > 10)
		{
			yIntRepeat = 10;
		}
	}

	if (delay_param && delay_param->gvalue)
	{

		yIntDelay = atoi (delay_param->gvalue);

		if (yIntDelay < 1)
		{
			yIntDelay = 0;
		}

		if (yIntDelay > 100000)
		{
			yIntDelay = 100000;
		}
	}

	sprintf (yStrScriptName, "vxi/networkAnnouncement.%d.vxml", zCall);

	fpNetAnn = fopen (yStrScriptName, "w+");

	if (!fpNetAnn)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
				   ERR,
				   "fopen(%s) failed. [%d, %s] "
				   "Unable to open net-ann script; rejecting the call.",
				   yStrScriptName, errno, strerror (errno));
		return -1;
	}

	/*Dynamically generated script */
	fprintf (fpNetAnn,
			 ""
			 "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>"
			 "<vxml version=\"2.0\" xmlns=\"http://www.w3.org/2001/vxml\">"
			 "	<form id=\"frmNetAnnPlay\">"
			 "		<object classid = \"TEL_AnswerCall\" name = \"ansForNetAnn\">"
			 "			<param value = \"5\" name = \"ARC_param_rings\" />"
			 "		</object>");

	if (yIntDelay > 0)
	{
		for (int i = 0; i < yIntRepeat; i++)
		{
			fprintf (fpNetAnn,
					 "<block><prompt><audio src=\"%s\">"
					 "	Alternate text for network announcement </audio>"
					 "	</prompt></block>", gCall[zCall].GV_SipUriPlay);

			if (i < yIntRepeat - 1)
			{
				fprintf (fpNetAnn,
						 "<object classid = \"UTL_sleep\" name = \"mySleep%d\">"
						 "	<param value = \"0\" name = \"ARC_param_sleepSeconds\" />"
						 "	<param value = \"%d\" name = \"ARC_param_sleepMilliSeconds\" />"
						 "</object>", i, yIntDelay);
			}
		}
	}
	else
	{
		for (int i = 0; i < yIntRepeat; i++)
		{
			fprintf (fpNetAnn,
					 "<block><prompt><audio src=\"%s\">"
					 "Alternate text for network announcement </audio>"
					 "</prompt></block>", gCall[zCall].GV_SipUriPlay);
		}
	}

	fprintf (fpNetAnn, "	</form>" "</vxml>" "");

	fclose (fpNetAnn);

	fpNetAnn = NULL;

	return 0;

}								/*END: int createNetworkAnnouncementVxmlScript */

#define CALL_MANAGER_STACK_SIZE (128 * 1024 * 1024)

int restartMMOutputThread(int zCall, sdp_message_t  *remote_sdp, sdp_media_t    *audio_media)
{
	int yRc = 0;
	char  mod[] = "restartMMOutputThread";

	char *yStrTmpSdp = NULL;

	struct MsgToDM  msgRtpDetails;

	gCall[zCall].remoteRtpPort =
		atoi (audio_media->m_port);

	if (gCall[zCall].remoteRtpPort < 0 )
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE,
		   ERR, "Failed to retrieve remote rtp port while handling reINVITE.");

		return -1;
	}


	sdp_message_to_str (remote_sdp,
							(char **) &yStrTmpSdp);

	sprintf (gCall[zCall].sdp_body_remote, "%s",
				 yStrTmpSdp);

	osip_free (yStrTmpSdp);
	yStrTmpSdp = NULL;

	yRc = parseSdpMessage (zCall, __LINE__,
							 gCall[zCall].sdp_body_remote,
							 remote_sdp);
	if (yRc == -1)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE,
		   ERR, "Failed to parse SDP while handling reINVITE.");

		return -1;
	}


	memcpy (&msgRtpDetails, &(gCall[zCall].msgToDM),
								sizeof (struct MsgToDM));

	msgRtpDetails.opcode = DMOP_RESTART_OUTPUT_THREAD;
		sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
			 gCall[zCall].leg,
			 gCall[zCall].remoteRtpPort,
			 gCall[zCall].localSdpPort,
			 (gCall[zCall].telephonePayloadPresent) ? 
				gCall[zCall].telephonePayloadType : -99, RECVONLY,
			 gCall[zCall].codecType,
			 gCall[zCall].remoteRtpIp);

	yRc =
		notifyMediaMgr (__LINE__, zCall,
						(struct MsgToDM *)
					&(msgRtpDetails), mod);

	return yRc;
}// int restartMMOutputThread

int
main (int argc, char *argv[])
{
	int             yRc;
	char            mod[] = "main";

	char            yBlastResultFile[128];
	int             i, j;
	int             zCall = -1;
	int             yIntExosipInitCount = 0;
	char            yStrSR[64] = "";
	char            yOSVersion[64] = "";
	char            yErrMsg[512];
	char            ringEventTime[17];
	char           *yStrTmpSdp = NULL;
	char           *yStrTmpRemoteSdp = NULL;
	struct MsgToDM  gDisconnectStruct = { DMOP_DISCONNECT, -1, -1, -1 };

	int             yIntCodecPayload;
	int             yIntAutomaticActionCounter = 0;
	int             yCrossPort = 0;

	struct sigaction sig_act, sig_oact;
	int             isVXIPresent = 0;

	gShutDownFromRedirector = 0;
	gShutDownFlag = 0;
	gCallsStarted = 0;
	gCallsFinished = 0;

//	LM_HANDLE      *GV_lmHandle;
	char            failureReason[1024] = "";
	int             yIntSavedCallState = -1;

	eXosip_event_t *eXosipEvent = NULL;

	pthread_attr_t  thread_attr;

	sdp_message_t  *remote_sdp = NULL;
	sdp_media_t    *audio_media = NULL;
	osip_from_t    *sipFrom = NULL;
	osip_uri_t     *sipMainUri = NULL;
	osip_to_t      *sipTo = NULL;
	osip_uri_t     *sipUrl = NULL;
	osip_message_t *answer = NULL;
	sdp_message_t  *remote_sdp_answered = NULL;
	osip_message_t *ringing = NULL;

	osip_uri_param_t *voicexml_param = NULL;
	osip_uri_param_t *aai_param = NULL;
	osip_uri_param_t *play_param = NULL;
	osip_uri_param_t *early_param = NULL;
	osip_uri_param_t *delay_param = NULL;
	osip_uri_param_t *repeat_param = NULL;

//  char            *pStrPrackHdr;
	int             yIntPRACKSendRequire = 0;
	int             yInt101Setting = 1;
	int				yRPortSetting = 0;

    int             yNumMaxInboundPortAllowed = 48;
    int             yNumPorts;
	int				rc;

	char		tmpBuf[128];
#if 0
	struct rlimit   rlim;

	memset (&rlim, 0, sizeof (rlim));

	rlim.rlim_cur = CALL_MANAGER_STACK_SIZE;
	rlim.rlim_max = CALL_MANAGER_STACK_SIZE;

	int             rc = setrlimit (RLIMIT_STACK, &rlim);

	if (rc != 0)
	{
//fprintf(stderr, " %s() failed to set rlimit for stack size to %d\n", __func__, CALL_MANAGER_STACK_SIZE);
	}
	else
	{
		;;
	}
#endif

//   char tracefile[256];
//
//    snprintf(tracefile, sizeof(tracefile), "MALLOC_TRACE=/tmp/mtrace.%d", getpid());
//    putenv(tracefile);
//    mtrace();

	//eXosipFreeList.Init();

    // there should always be a argv[0] 

    arc_dbg_open(argv[0]);

	RegistrationInfoListInit (&RegistrationInfoList);
	SubscriptionInfoInit (&SubscriptionInfoList);

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "%d:%d:%s:Starting with pid=%d.",
			   getpid (), __LINE__, __FILE__, getpid ());

/*Right now, we support only Linux OS*/
#ifndef __linux

#ifdef SR_SPEECHWORKS
	sprintf (yStrSR, "%s", "SpeechWorks");
#elif SR_MRCP
	sprintf (yStrSR, "%s", "MRCP");
#endif

#ifdef ACU_LINUX
	fprintf (stdout, " Aculab Compatible.\n");
#endif
//#ifdef CALEA
	fprintf (stdout, " CALEA Support.\n");
//#endif

	exit (-1);

#endif

	gCanReadRequestFifo = 1;

	/*
	 * Set the signal handlers for segv and pipe
	 */
	signal (SIGPIPE, sigpipe);
	signal (SIGTERM, sigterm);
	signal (SIGUSR1, sigusr);

	/*
	 * END: Set the signal handlers for segv and pipe
	 */

/* set death of child function */
	sig_act.sa_handler = NULL;
	sig_act.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT;
	if (sigaction (SIGCHLD, &sig_act, &sig_oact) != 0)
	{
		sprintf (yErrMsg,
				 "sigaction(SIGCHLD, SA_NOCLDWAIT): system call failed. [%d, %s]",
				 errno, strerror (errno));
		__DDNDEBUG__ (DEBUG_MODULE_MEMORY, "", yErrMsg, -1);
		exit (-1);
	}
	if (signal (SIGCHLD, SIG_IGN) == SIG_ERR)
	{
		sprintf (yErrMsg,
				 "signal(SIGCHLD, SIG_IGN): system call failed. [%d, %s]",
				 errno, strerror (errno));
		__DDNDEBUG__ (DEBUG_MODULE_MEMORY, "", yErrMsg, -1);
		exit (-1);
	}
	/*END: set death of child function */

	sprintf (gProgramName, "%s", argv[0]);

	/*
	 * Print the version info
	 */
	if (argc == 2 && strcmp (argv[1], "-v") == 0)
	{
#ifdef SR_SPEECHWORKS
		sprintf (yStrSR, "%s", "SpeechWorks");
#elif SR_MRCP
		sprintf (yStrSR, "%s", "MRCP");
#endif
#ifdef OS_ROCKY
		sprintf(yOSVersion, "%s", "Rocky Linux v8.9");
#elif OS_ORACLE
		sprintf(yOSVersion, "%s", "Oracle Linux v8.9");
#endif
		fprintf (stdout,
				 "ARC Single Dynamic Manager for SIP Telecom Services on %s.\n"
				 "Version:%s Build:%s\n"
				 "Compiled on %s %s.\n", yOSVersion,
				 ARCSIP_VERSION, ARCSIP_BUILD, __DATE__,
				 __TIME__);

#ifdef ACU_LINUX
		fprintf (stdout, " Aculab Compatible.\n");
#endif

		//muntrace();
		exit (-1);
	}
	else if (argc == 2 && strstr (argv[1], "?") != NULL)
	{
#ifdef SR_SPEECHWORKS
		sprintf (yStrSR, "%s", "SpeechWorks");
#elif SR_MRCP
		sprintf (yStrSR, "%s", "MRCP");
#endif
		fprintf (stdout,
				 "ARC Single Dynamic Manager for SIP Telecom Services.(%s).\n"
				 "Version:%s Build:%s\n"
				 "Compiled on %s %s with %s Speech Recognition.\n",
				 argv[0], ARCSIP_VERSION, ARCSIP_BUILD, __DATE__,
				 __TIME__, yStrSR);

		fprintf (stdout, "%s",
				 "-v		Product info		\n"
				 "-vv	Version number		\n"
				 "-vb	Build number		\n" "-vp	Protocol info");

		exit (-1);
	}
	else if (argc == 2 && strcmp (argv[1], "-vp") == 0)
		 /*PROTOCOL*/
	{
		sprintf (yStrSR, "%s", "ArcIPDynMgr: Protocol: SIP/2.0\n");
		fprintf (stdout, "%s", yStrSR);
		exit (0);
	}
	else if (argc == 2 && strcmp (argv[1], "-vv") == 0)
	{
		sprintf (yStrSR, "ArcIPDynMgr: Version: %s\n", ARCSIP_VERSION);
		fprintf (stdout, "%s", yStrSR);
		exit (0);
	}
	else if (argc == 2 && strcmp (argv[1], "-vb") == 0)
	{
		sprintf (yStrSR, "ArcIPDynMgr: Build: %s\n", ARCSIP_BUILD);
		fprintf (stdout, "%s", yStrSR);
		exit (0);
	}

#ifdef ddn_and___linux

	/*
	 ** Only Responsibility can start ArcIPDynMgr, 
	 ** for every other parent, just print the version info 
	 */
	else
	{
	int             yIntParentId = getppid ();

	char            yStrParentCmdLineFile[256];
	char            yStrParentCmdLineData[256];

	FILE           *yFpStartup = NULL;

		sprintf (yStrParentCmdLineFile, "/proc/%d/cmdline", yIntParentId);

		yFpStartup = fopen (yStrParentCmdLineFile, "r");

		if (yFpStartup == NULL)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR,
					   "fopen(%s) failed. [%d, %s] "
					   "Failed to verify the identity of parent process. Exiting.",
					   yStrParentCmdLineFile, errno, strerror (errno));
			sleep (2);
			exit (-1);
		}

		fscanf (yFpStartup, "%s", yStrParentCmdLineData);
		fclose (yFpStartup);

		if (strstr (yStrParentCmdLineData, "ArcIPResp") !=
			yStrParentCmdLineData)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_NORMAL,
					   TEL_INITIALIZATION_ERROR, ERR,
					   "Failed to verify the identity of parent process. Exiting.");
			exit (-1);
#if 0
#ifdef SR_SPEECHWORKS
			sprintf (yStrSR, "%s", "SpeechWorks");
#elif SR_MRCP
			sprintf (yStrSR, "%s", "MRCP");
#endif
#endif

			fprintf (stdout,
					 "ARC Single Dynamic Manager for SIP Telecom Services.(%s).\n"
					 "Version:%s Build:%s\n"
					 "Compiled on %s %s with %s Speech Recognition.\n",
					 argv[0], ARCSIP_VERSION, ARCSIP_BUILD,
					 __DATE__, __TIME__, yStrSR);

			exit (-1);
		}
	}
#endif


	parse_arguments (mod, argc, argv);

#if 0
	if ((gEndPort + 1) % 48 != 0)
	{
		gEndPort--;
	}
#endif

	if ((gStartPort < 0) ||
		(gEndPort < 0) ||
		(gDynMgrId < 0) ||
		(gStartPort > gEndPort) ||
		(gStartPort > MAX_PORTS) ||
		(gEndPort > MAX_PORTS) || (gDynMgrId > (MAX_PORTS / 48)))
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_INVALID_PARM, ERR,
				   "Failed to start Dynamic Manager, wrong parameter(s). "
				   "Start Port (%d), End Port (%d), Dynamic Manager instance (%d).",
				   gStartPort, gEndPort, gDynMgrId);
		exit (-1);
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_CALLMGR_STARTUP,
				   INFO,
				   "Started Dynamic Manager.  Start port (%d), End port (%d), "
				   "Dynamic Manager instance (%d).", gStartPort, gEndPort,
				   gDynMgrId);
	}

	gLastGivenPort = gStartPort - 1;
	maxNumberOfCalls = MAX_NUM_CALLS;
	sprintf (gHostName, "%s", 0);
	gethostname (gHostName, 255);

#if 0
	yRc = getIpAddress (gHostName, gHostIp, yErrMsg);
	if (yRc < 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_BASE, ERR, yErrMsg);
		sleep (EXIT_SLEEP);
		return (-1);
	}
#endif

	/*
	 * Determine base pathname.
	 */
	ispbase = (char *) getenv ("ISPBASE");
	if (ispbase == NULL)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_DATA_NOT_FOUND, ERR,
				   "Failed to get ISPBASE environment variable. "
				   "Make sure the ISPBASE is properly exported and restart.  Exiting.");
		sleep (EXIT_SLEEP);
		return (-1);
	}
	/*
	 * END: Determine base pathname.
	 */

	sprintf (gIspBase, "%s", ispbase);

	sprintf (gGlobalCfg, "%s/Global/.Global.cfg", gIspBase);

	sprintf (gFifoDir, "%s", "\0");

	yRc = get_name_value ("", FIFO_DIR_NAME_IN_NAME_VALUE_PAIR,
						  DEFAULT_FIFO_DIR, gFifoDir, sizeof (gFifoDir),
						  gGlobalCfg, yErrMsg);

	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_DATA_NOT_FOUND, WARN,
				   "Failed to find (%s) in (%s). Using (%s).",
				   FIFO_DIR_NAME_IN_NAME_VALUE_PAIR, gGlobalCfg, gFifoDir);
	}

	sprintf (gToRespFifo, "%s/%s", gFifoDir, DYNMGR_RESP_PIPE_NAME);

	if (gResId > -1)
	{
		sprintf (gToRespFifo, "%s/%s.%d", gFifoDir, DYNMGR_RESP_PIPE_NAME,
				 gResId);
	}

	tran_shmid = shmget (SHMKEY_TEL, SIZE_SCHD_MEM, PERMS);
	if (tran_shmid < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "shmget() failed. rc=%d.  Unable to get resp shared memory segment SHMKEY_TEL. [%d, %s] "
				   "gDynMgrId is (%d). Exiting.",
				   tran_shmid, errno, strerror (errno), gDynMgrId);
		return (-1);
	}

	/* attach shared memory */
	if ((tran_tabl_ptr = (char *) shmat (tran_shmid, 0, 0)) == (char *) -1)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_IPC_ERROR, ERR,
				   "shmat() failed.  Unable to attach to shared memory key %d. [%d, %s] Exiting.",
				   tran_shmid, errno, strerror (errno));
		return (-1);
	}

	/*
	 * Initialize IPC between CallMgr and MediaMgr
	 */
	yRc = removeSharedMem (-1);
	if (yRc == -1)
	{
		sleep (EXIT_SLEEP);
		return (-1);
	}

	yRc = createSharedMem (-1);
	if (yRc == -1)
	{
		sleep (EXIT_SLEEP);
		return (-1);
	}

	yRc = attachSharedMem (-1);
	if (yRc == -1)
	{
		sleep (EXIT_SLEEP);
		return (-1);
	}
	/*
	 * END: Initialize IPC between CallMgr and MediaMgr
	 */

	/*
	 * Load the config file
	 */
	yRc = loadConfigFile (NULL, NULL);

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "gPrackMethod set to %d; PRACK_DISABLED=%d, PRACK_SUPPORTED=%d, "
			   "PRACK_REQUIRE=%d; SIP_InboundEarlyMedia=%d ", gPrackMethod,
			   PRACK_DISABLED, PRACK_SUPPORTED, PRACK_REQUIRE,
			   gInboundEarlyMedia);

    yNumPorts = gEndPort - gStartPort + 1;
    yNumMaxInboundPortAllowed = yNumPorts + gStartPort ;
    if ( gPercentInboundAllowed < 100 )
    {
        yNumMaxInboundPortAllowed = (int)( yNumPorts * gPercentInboundAllowed * .01) + gStartPort;
    }
    dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
        "Max inbound only port: %d ( %d%c of %d ports ). ",
		yNumMaxInboundPortAllowed, gPercentInboundAllowed, '%', yNumPorts);

	__DDNDEBUG__ (DEBUG_MODULE_RTP, "IP address from .TEL.cfg", gHostIp, -1);

	if (!gHostIp[0] || strcmp (gHostIp, "0.0.0.0") == 0)
	{
		yRc = getIpAddress (gHostName, gHostIp, yErrMsg);
		if (yRc < 0)
		{
			dynVarLog (__LINE__, -1, mod, REPORT_NORMAL,
					   TEL_INITIALIZATION_ERROR, ERR, "%s Exiting.", yErrMsg);
			sleep (EXIT_SLEEP);
			return (-1);
		}
	}

	__DDNDEBUG__ (DEBUG_MODULE_RTP, "IP address in use", gHostIp, -1);

	/*
	 * END: Load the config file
	 */

	/* 
	 * Check for Video License
	 */
#if 0
     // video no longer used so commenting out the check for it Fri Oct  2 09:23:55 EDT 2015
     // this appears to commented out already in 64 bit 
	if ((yRc =
		 flexLMCheckOut (gVideoLicenseFeature, "2.0", 1, &GV_lmHandle,
						 failureReason)) != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_LICENSE_FAILURE, ERR,
				   "License failure (VIDEO).  %s.", failureReason);
		gVideoLicenseAvailable = 0;
	}
	else
	{
		dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "Successfully verified VIDEO license.");
		gVideoLicenseAvailable = 1;
	}
#endif

	gIsRtpPortSet = NO;

	/*
	 * Start Media Mgr
	 */
	yRc = startMediaMgr ();
	/*
	 * END: Start Media Mgr
	 */

	isVXIPresent = checkIfVXIPresent ();
	if (isVXIPresent == 1)
	{
		gCanKillApp = NO;
	}
	else
	{
		gCanKillApp = YES;
	}

	for (i = 0; i < MAX_PORTS; i++)
	{
		gCall[i].isInBlastDial = 0;

#if 0 
		gCall[i].data_buffer = NULL;
#endif
		gCall[i].portState = PORT_STATE_ON;
		gCall[i].outboundRetCode = 0;

		gCall[i].leg = LEG_A;
		gCall[i].crossPort = -1;
		gCall[i].responseFifoFd = -1;
		gCall[i].lastResponseFifo[0] = 0;
		gCall[i].dtmfAvailable = 0;
		gettimeofday(&gCall[i].lastDtmfTime, NULL);
		gCall[i].speakStarted = 0;
		gCall[i].rtp_session = NULL;
		gCall[i].in_user_ts = 0;
		gCall[i].out_user_ts = 0;
		gCall[i].callNum = i;
		setCallState (i, CALL_STATE_IDLE);
		//gCall[i].callState = CALL_STATE_IDLE;
		gCall[i].stopSpeaking = 0;
		gCall[i].lastTimeThreadId = 0;
		gCall[i].outputThreadId = 0;
		gCall[i].permanentlyReserved = 0;
		gCall[i].appPassword = 0;
		gCall[i].prevAppPid = -1;
		gCall[i].appPid = -1;
		if (isVXIPresent == 1)
		{
			gCall[i].canKillApp = NO;
		}
		else
		{
			gCall[i].canKillApp = YES;
		}
		gCall[i].lastError = CALL_NO_ERROR;

		gCall[i].srClientFifoFd = -1;
		gCall[i].ttsClientFifoFd = -1;

		gCall[i].telephonePayloadType = 96;
		gCall[i].telephonePayloadType_2 = 101;
		gCall[i].telephonePayloadType_3 = 120;
		gCall[i].telephonePayloadType_4 = 127;
		gCall[i].codecType = 0;
		gCall[i].isInitiate = 0;

		for (j = 0; j < MAX_THREADS_PER_CALL; j++)
		{
			gCall[i].threadInfo[j].appRef = -1;
			gCall[i].threadInfo[j].threadId = 0;
		}

		gCall[i].ttsRtpPort = (i * 2 + LOCAL_STARTING_RTP_PORT) + 700;
		gCall[i].ttsRtcpPort = (i * 2 + LOCAL_STARTING_RTP_PORT) + 701;

#ifdef SR_MRCP
		gCall[i].pFirstRtpMrcpData = NULL;
		gCall[i].pLastRtpMrcpData = NULL;

		gCall[i].gUtteranceFileFp = NULL;
		memset (gCall[i].gUtteranceFile, 0, sizeof (gCall[i].gUtteranceFile));

		gCall[i].rtp_session_mrcp = NULL;
#endif

		gCall[i].pFirstSpeak = NULL;
		gCall[i].pLastSpeak = NULL;
		gCall[i].pendingInputRequests = 0;
		gCall[i].pendingOutputRequests = 0;

		gCall[i].speakProcessWriteFifoFd = -1;
		gCall[i].speakProcessReadFifoFd = -1;

		gCall[i].faxData.version = -1;
		gCall[i].faxData.isFaxCall = -1;

		gCall[i].GV_PreferredCodec[0] = '\0';
		gCall[i].GV_SipAuthenticationUser[0] = '\0';
		gCall[i].GV_SipAuthenticationPassword[0] = '\0';
		gCall[i].GV_SipAuthenticationId[0] = '\0';
		gCall[i].GV_SipAuthenticationRealm[0] = '\0';
		gCall[i].GV_SipFrom[0] = '\0';
		gCall[i].GV_SipAsAddress[0] = '\0';
		gCall[i].GV_DefaultGateway[0] = '\0';
		gCall[i].GV_RouteHeader[0] = '\0';
		gCall[i].GV_PAssertedIdentity[0] = '\0';
		gCall[i].GV_PChargeInfo[0] = '\0';
		gCall[i].GV_CallInfo[0] = '\0';
		gCall[i].GV_Diversion[0] = '\0';
		gCall[i].GV_CallerName[0] = '\0';

		gCall[i].GV_CustData1[0] = '\0';
		gCall[i].GV_CustData2[0] = '\0';
		memset (gCall[i].GV_SipURL, 0, 2048);
		memset (gCall[i].audioCodecString, 0, 256);
		memset (gCall[i].sdp_body_remote, 0, 4096);
		sprintf (gCall[i].audioCodecString, "%s", gPreferredCodec);
		sprintf (gCall[i].call_type, "%s", "Voice-Only");

		gCall[i].lastReleaseTime = 0;
		memset (gCall[i].rdn, 0, 32);
		memset (gCall[i].ocn, 0, 32);
		gCall[i].isCallAnswered_noMedia = 0;
		gCall[i].full_duplex = SENDRECV;

		gCall[i].amrIntCodec[0] = 97;
		for (int j = 1; j < 10; j++)
		{
			gCall[i].amrIntCodec[j] = -1;
		}
		gCall[i].GV_CallDuration = gMaxCallDuration;
		gCall[i].GV_CancelOutboundCall = NO;
		gCall[i].isG729AnnexB = YES;
		memset (gCall[i].amrFmtp, 0, 256);
		memset (gCall[i].amrFmtpParams, 0, 256);

		gCall[i].remote_sdp = NULL;
		gCall[i].codecMissMatch = YES;
		gCall[i].localSdpPort = LOCAL_STARTING_RTP_PORT + (i * 2);
		gCall[i].presentRestrict = 0;

		for (int count = 0; count < MAX_CONF_PORTS; count++)
		{
			gCall[i].conf_ports[count] = -1;
		}
		gCall[i].confUserCount = 0;
		memset (gCall[i].confID, 0, sizeof (gCall[i].confID));
#ifdef ACU_LINUX
		gCall[i].faxRtpPort = -1;
		gCall[i].sendFaxReinvite = 0;
		gCall[i].send200OKForFax = 0;
		gCall[zCall].faxAlreadyGotReinvite = 0;
		gCall[i].isFaxReserverResourceCalled = 0;
		gCall[i].sendFaxOnProceed = 0;
		memset (&gCall[i].msgSendFax, 0, sizeof (gCall[i].msgSendFax));
		gCall[i].faxDelayTimerStarted = 0;
#endif

		memset (gCall[i].transferAAI, 0, sizeof (gCall[i].transferAAI));
		memset (gCall[i].remoteAAI, 0, sizeof (gCall[i].remoteAAI));

        // user to user header joes Thu Apr 23 11:51:15 EDT 2015
        memset(gCall[i].local_UserToUser, 0, sizeof(gCall[i].local_UserToUser));
        memset(gCall[i].remote_UserToUser, 0, sizeof(gCall[i].remote_UserToUser));
        // end user to user 


		gCall[i].isInputThreadStarted = 0;
		gCall[i].isOutputThreadStarted = 0;
		gCall[i].tid = -1;
		gCall[i].reinvite_tid = -1;
		gCall[i].reinvite_empty_sdp = 0;
		memset (gCall[i].originalContactHeader, 0, 1024);

		gCall[i].PRACKSendRequire = 0;
		gCall[i].PRACKRSeqCounter = 1;
		gCall[i].UAC_PRACKOKReceived = 0;
		gCall[i].UAC_PRACKRingCounter = 0;
		gCall[i].nextSendPrackSleep = 500;
		gCall[i].UAS_PRACKReceived = 0;
		sprintf (gCall[i].redirectedContacts, "\0");
		memset (gCall[i].pAIdentity_ip, 0, 256);

		gCall[i].callDirection = -1;			// MR 4964
		gCall[i].sessionTimersSet = 0;			// MR 4802
        gCall[i].issuedBlindTransfer = 0;
		gCall[i].canExitTransferCallThread = 0;	// BT-171
		memset (gCall[i].sipUserAgent, 0, sizeof(gCall[i].sipUserAgent));

        if ( i < yNumMaxInboundPortAllowed )
        {
            gCall[i].inboundAllowed = 1;
        }
        else
        {
            gCall[i].inboundAllowed = 0;
        }

/*
        if ( (i >= gStartPort ) && ( i <= gEndPort ) )
        {
            dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
                    "gCall[%d].inboundAllowed=%d", i, gCall[i].inboundAllowed);
        }
*/
	}

	/*
	 * eXosip initializations
	 */

	if (gDebugModules[DEBUG_MODULE_STACK].enabled == 1)
	{
		TRACE_INITIALIZE (END_TRACE_LEVEL, NULL);
	}

	SipLoadConfig ();

	/*Write Port Status */
	{
	FILE           *yFpReadStatus = NULL;
	int             i = -1;
	char            yStrPortStatusFile[256];

		sprintf (yStrPortStatusFile, ".portStatus.%d", gDynMgrId);

		yFpReadStatus = fopen (yStrPortStatusFile, "w+");

		if (yFpReadStatus != NULL)
		{
			fprintf (yFpReadStatus, "port=%d:pid=%d:id=%d:ip=%s\n", gSipPort,
					 getpid (), gDynMgrId, gHostIp);
			fclose (yFpReadStatus);
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
					   ERR,
					   "fopen(%s) failed. [%d, %s] "
					   "Unable to write port status.", yStrPortStatusFile,
					   errno, strerror (errno));
		}
	}
	/*END: Write Port Status */

	if (!gSipRedirection)
	{
		sprintf (tmpBuf, "%d", gSipPort);
		setenv("arcSipPort", tmpBuf, 1);
		RegistrationInfoLoadConfig (gHostIp, gSipPort, gDynMgrId, gIpCfg, gSipUserAgent);
		SubscriptionInfoLoadConfig (gHostIp, gSipPort, gIpCfg);
	}

	/*
	 * END: eXosip initializations
	 */

	pthread_mutex_init (&gNotifyMediaMgrLock, NULL);
	pthread_mutex_init (&gFreeCallAssignMutex, NULL);
	pthread_mutex_init (&gFreeBLegAssignMutex, NULL);
	pthread_mutex_init (&gRespShmMutex, NULL);

	yRc = pthread_attr_init (&gThreadAttr);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "pthread_attr_init() failed. rc=%d. [%d, %s] "
				   "Unable to initialize thread. Exiting.",
				   yRc, errno, strerror (errno));
		arc_exit (-1, "");
	}

	yRc = pthread_attr_setdetachstate (&gThreadAttr, PTHREAD_CREATE_DETACHED);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "pthread_attr_setdetachstate() failed. rc=%d. [%d, %s] "
				   "Unable to initialize thread. Exiting.",
				   yRc, errno, strerror (errno));
		arc_exit (-1, "");
	}

#if 0
//	size_t          thread_stack;
//
//	yRc = pthread_attr_getstacksize (&gThreadAttr, &thread_stack);
//	if (yRc == 0)
//	{
//		fprintf (stderr, " %s() thread stacksize set to %d\n", __func__,
//				 thread_stack);
//	}
//
//	size_t          stacksize = (1024 * 1024) * 2;
//
//	yRc = pthread_attr_setstacksize (&gThreadAttr, stacksize);
//
//	if (yRc == 0)
//	{
//		fprintf (stderr, " %s() setting stacksize to %d bytes\n", __func__,
//				 stacksize);
//	}
//
#endif

	yRc = startAppRequestReaderThread ();
	if (yRc != 0)
	{
		arc_exit (-1, "");
	}

	yRc = startMultiPurposeTimerThread ();
	if (yRc != 0)
	{
		arc_exit (-1, "");
	}

	yRc = pthread_attr_init (&thread_attr);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "pthread_attr_init() failed. rc=%d. [%d, %s] "
				   "Unable to initialize thread. Exiting.",
				   yRc, errno, strerror (errno));
		arc_exit (-1, "");
	}

	yRc = pthread_attr_setdetachstate (&thread_attr, PTHREAD_CREATE_DETACHED);
	if (yRc != 0)
	{
		dynVarLog (__LINE__, -1, mod, REPORT_NORMAL, TEL_PTHREAD_ERROR, ERR,
				   "pthread_attr_setdetachstate() failed. rc=%d. [%d, %s] "
				   "Unable to initialize thread. Exiting.",
				   yRc, errno, strerror (errno));
		arc_exit (-1, "");
	}

	gCanReadSipEvents = 1;

	isDynMgrReady = 1;

    // creates the pid file and deletes the recycle file 
    // Sun Mar 29 05:13:21 EDT 2015
    arc_recycle_up_file(RECYCLEPIDFILE, gDynMgrId);

	getRtpPortsFromMediaMgr ();

/*

 This has also been moved to the main exosip_event loop 
 as the timerthread entry doesn't re-register like it should. 

 registerToRedirector (mod);

 is the best way to do this, it properly forms the message and sends it. 

 in the exosip loop we also reregister the dynmgr's roughly 
 every 7 minutes or so, as there was a race at startup where 
 some spans disappeared from teh redirector. Joe S. 



	if (gSipRedirection)
	{
		registerToRedirector (mod);
	}

*/
    if (gSipUserAgent != NULL && gSipUserAgent[0] != '\0')		// BT-62
    {
        eXosip_set_user_agent (geXcontext, ( const char *) gSipUserAgent);
    }

#if NOT_EXOSIP_PORTED
	eXosip_set_option (geXcontext, EXOSIP_OPT_DONT_SEND_101, &yInt101Setting);
#endif // if NOT_EXOSIP_PORTED

	eXosip_set_option (geXcontext, EXOSIP_OPT_USE_RPORT, &yRPortSetting);

	while (gCanReadSipEvents)
	{

		if (remote_sdp != NULL)
		{
			__DDNDEBUG__ (DEBUG_MODULE_SIP, "Freeing remote_sdp", "", -1);

			sdp_message_free (remote_sdp);
			remote_sdp = NULL;
		}

		if (remote_sdp_answered != NULL)
		{
			sdp_message_free (remote_sdp_answered);
			remote_sdp_answered = NULL;
		}

		sipFrom = NULL;
		sipTo = NULL;

		sipMainUri = NULL;

		sipUrl = NULL;
	char            username[128] = "";

		if (gShutdownRegistrations)
		{
			deRegistrationHandler (&RegistrationInfoList);
		}

		if (eXosipEvent != NULL)
		{
			eXosip_lock (geXcontext);
			eXosip_event_free (eXosipEvent);
			eXosip_unlock (geXcontext);

			eXosipEvent = NULL;
		}

		eXosipEvent = eXosip_event_wait (geXcontext, 0, 0);

        canDynMgrExit();


       if (gShutDownFlag == 1)
       {
           time_t yTmpShutDownCheckTime;

           time (&yTmpShutDownCheckTime);

           // added JOES -- Mon Oct 26 09:45:36 EDT 2015
           // this will cause the call manager to exit forcefully 
           // so many seconds after gShutDownFlag is set.

           if (yTmpShutDownCheckTime - gShutDownFlagTime >= gShutDownMaxTime)
           {
				// MR # 4982
                // dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_CALLMGR_SHUTDOWN, ERR,
                dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_CALLMGR_SHUTDOWN, WARN,
                          " CallManager is taking a long time to shutdown, triggering a forceful shutdown after %d seconds ", gShutDownMaxTime);

                 doDynMgrExit(zCall, __func__, __LINE__);
            }

        }

		//30 seconds: Looking for active calls to send keep alive
		if (gSipSendKeepAlive == 1 && 
			yIntAutomaticActionCounter % 30000 == 0)
		{
			if ( strcmp (gHostTransport, "tcp") == 0 || 
				 strcmp (gHostTransport, "tls") == 0)
			{
				for (i = gStartPort; i <= gEndPort; i++)
				{
					if (gCall[i].callState == CALL_STATE_CALL_ANSWERED || 
						gCall[i].callState == CALL_STATE_CALL_ACK ||
						gCall[i].callState == CALL_STATE_CALL_HOLD ||
						gCall[i].callState == CALL_STATE_CALL_OFFHOLD ||
						gCall[i].callState == CALL_STATE_CALL_BRIDGED )
					{
					__DDNDEBUG__ (DEBUG_MODULE_CALL, "Checking calls to send keep alive.", "", i);
						process_DMOP_SENDKEEPALIVE(i);
					}//callstates
				}//for all ports
			}//transport
		}//30 sec

		if (eXosipEvent == NULL)
		{

#if 1
			if (yIntAutomaticActionCounter % 2000 == 0)
			{
				// yIntAutomaticActionCounter = 0;
				eXosip_lock (geXcontext);
				eXosip_automatic_action (geXcontext);
				eXosip_unlock (geXcontext);
			}

           
            /* roughly every seven minutes */
			if (yIntAutomaticActionCounter % 20000 == 0){
			if (gSipRedirection && (gShutDownFromRedirector != 1))
			  {
//fprintf(stderr, " %s() JOE S --- re-reg to redirector\n", __func__);
				registerToRedirector (mod);
			  }
            }

			yIntAutomaticActionCounter++;
#endif
		}

		if (eXosipEvent != NULL)
		{

			switch (eXosipEvent->type){
			case EXOSIP_CALL_RELEASED:
				zCall = getCallNumResetCidAndDid (eXosipEvent, eXosipEvent->cid, eXosipEvent->did);
				break;
			default:
				zCall = getCallNumFromCidAndDid (eXosipEvent, eXosipEvent->cid, eXosipEvent->did);
				break;
			}

            // utility to get some sip header info for global vars 
            if(eXosipEvent->request != NULL){
				//fprintf(stderr, "DDN_DEBUG: %s() Calling arc_get_global_headers for user to user 1\n", __func__);
               arc_get_global_headers(zCall, eXosipEvent->request);
            }
            // both added Fri Apr 24 06:59:03 EDT 2015
            if(eXosipEvent->response != NULL){ 
				//fprintf(stderr, "DDN_DEBUG: %s() Calling arc_get_global_headers for user to user 2\n", __func__);
               arc_get_global_headers(zCall, eXosipEvent->response);
            }
            // end get global headers as they come in 

			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "eXosip event  Call(%d) Dialog(%d) type(%d) (%s)",
					   eXosipEvent->cid,
					   eXosipEvent->did,
					   eXosipEvent->type, eXosipEvent->textinfo);

			switch (eXosipEvent->type)
			{
			case EXOSIP_CALL_REDIRECTED:
				{

#if 0
					eXosip_lock (geXcontext);
					eXosip_default_action (geXcontext, eXosipEvent);
					eXosip_unlock (geXcontext);

					continue;
#endif

#if 1
					yRc =
						OutboundRedirectionHandler (eXosipEvent, zCall,
													yErrMsg);

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "OutboundRedirection result: %s",
							   yErrMsg);

					continue;
#endif
				}
				break;

#if NOT_EXOSIP_PORTED
				// SUPSCRIPTION EVENTS 
			case EXOSIP_SUBSCRIPTION_UPDATE:
				break;
			case EXOSIP_SUBSCRIPTION_CLOSED:
				deletedSubscriptionHandler (eXosipEvent);
				break;
			case EXOSIP_SUBSCRIPTION_NOANSWER:
				unsuccessfulSubscriptionHandler (eXosipEvent);
				break;
			case EXOSIP_SUBSCRIPTION_PROCEEDING:
				break;
			case EXOSIP_SUBSCRIPTION_ANSWERED:
				successfulSubscriptionHandler (eXosipEvent);
				break;
			case EXOSIP_SUBSCRIPTION_REDIRECTED:
				break;
			case EXOSIP_SUBSCRIPTION_REQUESTFAILURE:
				unsuccessfulSubscriptionHandler (eXosipEvent);
				break;
			case EXOSIP_SUBSCRIPTION_SERVERFAILURE:
				unsuccessfulSubscriptionHandler (eXosipEvent);
				break;
			case EXOSIP_SUBSCRIPTION_GLOBALFAILURE:
				unsuccessfulSubscriptionHandler (eXosipEvent);
				break;
			case EXOSIP_SUBSCRIPTION_NOTIFY:
				if( (gCall[zCall].remoteAgent & USER_AGENT_AVAYA_CM) ||
			        (gCall[zCall].remoteAgent & USER_AGENT_AVAYA_SM) )
				{
					;
				}
				else
				{
					incomingNotifyHandler (eXosipEvent);
				}

				break;
			case EXOSIP_SUBSCRIPTION_RELEASED:
				deletedSubscriptionHandler (eXosipEvent);
				break;

				// REGISTRATION EVENTS

			case EXOSIP_REGISTRATION_NEW:
				break;
			case EXOSIP_REGISTRATION_SUCCESS:
				successfulRegistrationHandler (eXosipEvent);
				break;
			case EXOSIP_REGISTRATION_FAILURE:
				unsuccessfulRegistrationHandler (eXosipEvent);
				break;
				// END REGISTRATION EVENTS
#endif // if NOT_EXOSIP_PORTED

#ifdef EXOSIP_VERSION_401
			case EXOSIP_CALL_INVITE:
#else
			case EXOSIP_CALL_NEW:
#endif

				{
					;
				}
				break;

			case EXOSIP_MESSAGE_NEW:
				// placeholder for real options processing
				if (MSG_IS_OPTIONS (eXosipEvent->request))
				{

					int             yRc = 0;

					osip_message_t *optionAnswer = NULL;

					eXosip_lock (geXcontext);

					yRc =
						eXosip_options_build_answer (geXcontext, eXosipEvent->tid, 200,
													 &optionAnswer);

					if (yRc < 0 || optionAnswer == NULL)
					{
						dynVarLog (__LINE__, -1, mod, REPORT_NORMAL,
								   TEL_SIP_SIGNALING, ERR,
								   "eXosip_options_build_answer() failed. rc=%d.  "
								   "Unable to build answer to options message (tid:%d).",
								   yRc, eXosipEvent->tid);
					}
					else
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
								   TEL_BASE, WARN,
								   "Sending SIP Message 200.");

						//yRc = eXosip_options_send_answer (geXcontext, eXosipEvent->tid, 488, NULL);
						yRc = eXosip_options_send_answer (geXcontext, eXosipEvent->tid, 200, optionAnswer);
						if (yRc < 0)
						{
							dynVarLog (__LINE__, -1, mod, REPORT_NORMAL,
									   TEL_SIP_SIGNALING, ERR,
									   "eXosip_options_send_answer() failed. rc=%d. "
									   "Unable to send answer to options message (tid:%d).",
									   yRc, eXosipEvent->tid);
						}
						else
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Sent answer to options message (tid:%d).",
									   eXosipEvent->tid);
						}
					}

					eXosip_unlock (geXcontext);
				}
				break;

#ifdef EXOSIP_VERSION_401
			case EXOSIP_CALL_MESSAGE_NEW:

	            OutboundCallReferHandler(__LINE__, eXosipEvent, zCall);
				dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, DYN_BASE, INFO,
						   "eXosip event EXOSIP_CALL_MESSAGE_NEW");

				if (!MSG_IS_BYE (eXosipEvent->request))
				{
					if (MSG_IS_PRACK (eXosipEvent->request))
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "eXosip event EXOSIP_CALL_MESSAGE_NEW::PRACK");
						gCall[zCall].UAS_PRACKReceived = 1;
					}
					else
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "eXosip event EXOSIP_CALL_MESSAGE_NEW");
						if ( zCall >= 0 )				// BT-49
						{
							incomingCallMessageDispatch (eXosipEvent, zCall);
							rc = gCallInfo[zCall].GetIntOption("session-expires");
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
									   INFO, "Retrieved session-expires of %d", rc);
							if ( rc > 0 )
							{
								if ( doSessionExpiry(zCall, __LINE__) == 0 )
								{
									gCall[zCall].sessionTimersSet = 0;
									dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
									   INFO, "Set sessionTimers to %d", gCall[zCall].sessionTimersSet);
								}
							}
						}
					}
				}
				else
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "eXosip event EXOSIP_CALL_MESSAGE_NEW with BYE");

					eXosipEvent->type = EXOSIP_CALL_CLOSED;
					break;
					// ths falls through to other switch statement below 
				}
#else
			case EXOSIP_OPTIONS_NEW:
			case EXOSIP_INFO_NEW:
			case EXOSIP_IN_SUBSCRIPTION_NEW:
#endif
				break;
			case EXOSIP_CALL_MESSAGE_REQUESTFAILURE:
				if ((gCall[zCall].faxData.isFaxCall == 1) &&
					(gCall[zCall].sendFaxReinvite == 1))
				{
	char            yRemoteConnectionInfo[128];

					sprintf (yRemoteConnectionInfo, "%s",
							 "destIP=NONE&destFaxPort=0");
					dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
							   TEL_SIP_SIGNALING, WARN,
							   "EXOSIP_CALL_MESSAGE_REQUESTFAILURE(%d) recieved.  Fax call rejected. "
							   "Notifying media mgr with (%s)",
							   EXOSIP_CALL_MESSAGE_REQUESTFAILURE,
							   yRemoteConnectionInfo);
					yStrTmpRemoteSdp = (char *) NULL;
					yRc = parseFaxSdpMsg (zCall,
										  yRemoteConnectionInfo,
										  yStrTmpRemoteSdp);
				}
				break;
#if NOT_EXOSIP_PORTED
			case EXOSIP_MESSAGE_PRACK:
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Received EXOSIP_MESSAGE_PRACK");
				gCall[zCall].UAS_PRACKReceived = 1;
				break;
			case EXOSIP_MESSAGE_PRACK_200OK:
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Received EXOSIP_MESSAGE_PRACK_200OK");
				gCall[zCall].UAC_PRACKOKReceived = 1;
				break;
#endif // if NOT_EXOSIP_PORTED
			default:
				{
					if (zCall < 0)
					{
						if (eXosipEvent->type == EXOSIP_CALL_RELEASED)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
									   TEL_BASE, WARN,
									   "Ignoring EXOSIP_CALL_RELEASED for Call(%d) Dialog(%d).",
									   eXosipEvent->cid, eXosipEvent->did);

							//gCallsFinished++;

							if (gShutDownFromRedirector == 1 && gSipRedirection)
							{
								if (gCallsStarted <= gCallsFinished)
								{
									dynVarLog (__LINE__, zCall, mod,
											   REPORT_NORMAL,
											   TEL_CALLMGR_SHUTDOWN, WARN,
											   "Restarting ArcIPDynMgr (%d)."
											   " gShutDownFromRedirector=1, gCallsStarted=%d gCallsFinished=%d",
											   gDynMgrId, gCallsStarted,
											   gCallsFinished);

									gShutDownFlag = 1;
								}

								__DDNDEBUG__ (DEBUG_MODULE_CALL,
											  "Calls started", "",
											  gCallsStarted);
								__DDNDEBUG__ (DEBUG_MODULE_CALL,
											  "Calls finished", "",
											  gCallsFinished);

								yRc = canDynMgrExit ();

								if (yRc == -99)
								{
                                    doDynMgrExit(zCall, __func__, __LINE__);
								}
							}
							else if (!gSipRedirection && gCallMgrRecycleBase > 0)
							{
								if ((gCallsStarted >
									(gCallMgrRecycleBase +
									 gCallMgrRecycleDelta * gDynMgrId)) && arc_can_recycle(RECYCLEPIDFILE, gDynMgrId))
								{
	                                char    yStrPortStatusFile[256];

									sprintf (yStrPortStatusFile,
											 ".portStatus.%d", gDynMgrId);
									unlink (yStrPortStatusFile);

									if (gShutDownFlag == 0)
									{
										dynVarLog (__LINE__, zCall, mod,
												   REPORT_VERBOSE, TEL_BASE,
												   INFO,
												   "Removing (%s) ArcIPDynMgr (%d)."
												   "gShutDownFlag = 1, gCallsStarted = %d",
												   yStrPortStatusFile,
												   gDynMgrId, gCallsStarted);

										if ( gShutDownFlagTime == 0 )
										{
											time (&gShutDownFlagTime);
										}
									}

                                    arc_recycle_down_file(RECYCLEPIDFILE, gDynMgrId);
									gShutDownFlag = 1;
								}
								else
								{
									gShutDownFlag = 0;
								}

#if 0
								if (gCallsStarted <= gCallsFinished && gShutDownFlag == 1)
								{
                                    doDynMgrExit(zCall, __func__, __LINE__);
								}
#endif 

								yRc = canDynMgrExit ();

								if (yRc == -99 && gShutDownFlag == 1)
								{
                                    doDynMgrExit(zCall, __func__, __LINE__);
								}
							}
						}
						else
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_DIAGNOSE,
									   TEL_INVALID_PORT, WARN,
									   "Failed to get port for Call(%d) Dialog(%d).  Ignoring event (%d).",
									   eXosipEvent->cid, eXosipEvent->did, eXosipEvent->type);
						}
						continue;

					}			/*if */
				}

			}					/*switch */

			switch (eXosipEvent->type)
			{

			case EXOSIP_CALL_MESSAGE_NEW:
                 // OutboundCallReferHandler(__LINE__, eXosipEvent, zCall);


#if 0 // this is all sip frag stuff for a refer this is not in the sip handling 
				{
	osip_body_t    *newMessageBody = NULL;

					osip_message_get_body (eXosipEvent->request, 0,
										   &newMessageBody);

					if (newMessageBody)
					{
	int             yIntTmpStatusCode = -1;
	char            yStrTmpStatusCode[256];

						sscanf (newMessageBody->body, "SIP/2.0 %d %s",
								&yIntTmpStatusCode, yStrTmpStatusCode);

						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Got EXOSIP_CALL_MESSAGE_NEW: with (%s). Status Code (%d).",
								   newMessageBody->body, yIntTmpStatusCode);

						switch (yIntTmpStatusCode)
						{
						case 100:
							break;

						case 180:
						case 181:
						case 182:
							setCallState (zCall, CALL_STATE_CALL_RINGING);
							break;

							/*Success */
						case 200:
							setCallState (zCall,
										  CALL_STATE_CALL_TRANSFERCOMPLETE);
							break;

							/*Redirection Messages */
						case 300:
						case 301:
						case 302:
						case 305:
						case 380:
							break;

							/*Request Failure */
						case 400 ... 499:

#if 0
						case 401:
						case 402:
						case 403:
						case 404:
						case 405:
						case 406:
						case 407:
						case 408:
						case 409:
						case 410:
						case 411:
						case 412:
						case 415:
						case 420:
						case 480:
						case 481:
						case 482:
						case 483:
						case 484:
						case 485:
						case 486:
						case 487:
						case 488:
						case 489:
						case 491:
						case 492:
						case 493:
						case 494:
#endif

							/*Server Failure */
						case 500 ... 599:

							/*Global Failures */
						case 600:
						case 601:
						case 602:
						case 603:
						case 604:
						case 605:
						case 606:

							gCall[zCall].eXosipStatusCode = yIntTmpStatusCode;
							gCall[zCall].outboundRetCode =
								gCall[zCall].eXosipStatusCode;
							setCallState (zCall,
										  CALL_STATE_CALL_TRANSFERFAILURE);
							break;
						}
					}
				}

#endif 
				break;

			case EXOSIP_IN_SUBSCRIPTION_NEW:
				{
					int	yRc = 0;

					int	yResponse = 488;

					osip_message_t *infoAnswer = NULL;

					dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE,
							   WARN,
							   "Sending SIP Message 488 Not Acceptable Here.");

					eXosip_lock (geXcontext);
					yRc =
						eXosip_call_build_answer (geXcontext, eXosipEvent->tid, yResponse,
												  &infoAnswer);
					yRc =
						eXosip_call_send_answer (geXcontext, eXosipEvent->tid, yResponse,
												 infoAnswer);
					eXosip_unlock (geXcontext);

					if (yRc < 0)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
								   TEL_SIP_SIGNALING, ERR,
								   "Failed to send %d to SUBSCRIBE message (tid:%d).",
								   yResponse, eXosipEvent->tid);
					}
					else
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Sent %d to SUBSCRIBE message (tid:%d).",
								   yResponse, eXosipEvent->tid);
					}
				}
				break;

#if 0
			case EXOSIP_INFO_NEW:
				{
	int             yRc = 0;

	int             yResponse = 200;

	osip_message_t *infoAnswer = NULL;

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Sending SIP Message 200 OK.");

					yRc =
						eXosip_call_build_answer (geXcontext, eXosipEvent->tid, yResponse,
												  &infoAnswer);

					yRc =
						eXosip_call_send_answer (geXcontext, eXosipEvent->tid, yResponse,
												 infoAnswer);
					if (yRc < 0)
					{
						dynVarLog (__LINE__, -1, mod, REPORT_NORMAL,
								   TEL_SIP_SIGNALING, ERR,
								   "Failed to send %d to INFO message (tid:%d).",
								   yResponse, eXosipEvent->tid);
					}
					else
					{
						dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Sent %d to INFO message (tid:%d).",
								   yResponse, eXosipEvent->tid);
					}
				}
				break;

#endif

#if 0

			case EXOSIP_OPTIONS_NEW:
				{
	int             yRc = 0;

	osip_message_t *optionAnswer = NULL;

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
							   "Sending SIP Message 200 OK.");
					yRc =
						eXosip_options_build_answer (geXcontext, eXosipEvent->tid, 200,
													 &optionAnswer);

					if (yRc < 0 || optionAnswer == NULL)
					{
						dynVarLog (__LINE__, -1, mod, REPORT_NORMAL,
								   TEL_SIP_SIGNALING, ERR,
								   "Failed to build answer to options message (tid:%d).",
								   eXosipEvent->tid);
					}
					else
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Sending SIP Message 488.");

						yRc =
							eXosip_options_send_answer (geXcontext, eXosipEvent->tid, 488,
														NULL);
						if (yRc < 0)
						{
							dynVarLog (__LINE__, -1, mod, REPORT_NORMAL,
									   TEL_SIP_SIGNALING, ERR,
									   "Failed to send answer to options message (tid:%d).",
									   eXosipEvent->tid);
						}
						else
						{
							dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Sent answer to options message (tid:%d).",
									   eXosipEvent->tid);
						}
					}

				}
				break;

#endif

#ifdef EXOSIP_VERSION_401
			case EXOSIP_CALL_MESSAGE_ANSWERED:

                 OutboundCallReferHandler(__LINE__, eXosipEvent, zCall);
				{
					if (gCall[zCall].msgToDM.opcode == DMOP_TRANSFERCALL ||
						gCall[zCall].callState ==
						CALL_STATE_CALL_TRANSFER_ISSUED)
					{
						setCallState (zCall,
									  CALL_STATE_CALL_TRANSFER_MESSAGE_ACCEPTED);
					}

					break;
				}
#endif

#ifdef EXOSIP_VERSION_401
			case EXOSIP_CALL_REINVITE:
#else
			case EXOSIP_CALL_MODIFIED:
#endif
				{


					int             yRc = 0;
					char            yStrTmpSdpPort[15];
					int             yResponse = 200;

					osip_message_t *reInviteAnswer = NULL;

 					// test to see if session expires is set right
                    // else send a 422 back
                    // set some timer somewhere and continue with the call
					if ( (rc = gCallInfo[zCall].ProcessSessionTimers (mod, __LINE__, zCall, eXosipEvent)) == -1)
					{
						osip_message_t *answer = NULL;
						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN, "Sending SIP Message 422.");

						eXosip_lock (geXcontext);
						eXosip_call_build_answer (geXcontext, eXosipEvent->tid, 422, &answer);
						osip_message_set_header (answer, "Min-SE", "90");		// BT-118
						yRc = eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 422, answer);
						eXosip_unlock (geXcontext);

						if (remote_sdp != NULL)
						{
							sdp_message_free (remote_sdp);
							remote_sdp = NULL;
						}
						break;
					}
					rc = gCallInfo[zCall].GetIntOption("session-expires");
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Retrieved session-expires of %d", rc);
					if ( rc > 0 )
					{
						if ( doSessionExpiry(zCall, __LINE__) == 0 )
						{
							gCall[zCall].sessionTimersSet = 0;
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Set sessionTimers to %d", gCall[zCall].sessionTimersSet);
						}
					}
				
					eXosip_lock (geXcontext);
					remote_sdp = eXosip_get_sdp_info (eXosipEvent->request);
					eXosip_unlock (geXcontext);

					if (remote_sdp)
					{
						sdp_message_to_str (remote_sdp,
											(char **) &yStrTmpRemoteSdp);

/*DDN_FAX: 02/25/2010 */
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   DYN_BASE, INFO,
								   "Parsing SDP in REINVITE (tid=%d)",
								   gCall[zCall].reinvite_tid);

						yRc =
							parseSdpMessage (zCall, __LINE__, yStrTmpRemoteSdp,
											 remote_sdp);
						audio_media = eXosip_get_audio_media (remote_sdp);
						if (audio_media != NULL)
						{
							gCall[zCall].remoteRtpPort = 0;
							gCall[zCall].remoteRtpPort =
								atoi (audio_media->m_port);
						}
						gCall[zCall].reinvite_empty_sdp = 0;
					}
					else
					{
						if(gRejectEmptyInvite == 1)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
								   TEL_BASE, WARN,
								   "Sending SIP Message 400 Bad Request since gRejectEmptyInvite=1.");

							eXosip_lock (geXcontext);
							yRc =
								eXosip_call_send_answer (geXcontext, 
											eXosipEvent->tid, 400, NULL);
							eXosip_unlock (geXcontext);

							break;

						}

						yStrTmpRemoteSdp = (char *) calloc (1, 1);
						gCall[zCall].reinvite_empty_sdp = 1;

					}
					gCall[zCall].reinvite_tid = eXosipEvent->tid;

#if 0
//RG TODO: If SpanDSP then send stop call progress to MediMgr and write FAX found to App
					if (gCall[zCall].faxData.isFaxCall == 1)
					{

					}
#endif
					if (gCall[zCall].faxData.isFaxCall == 1)
					{
						char            yRemoteConnectionInfo[128];
						sdp_media_t    *media_media = NULL;

						arc_fax_stoptone (zCall);

						media_media = eXosip_get_media (remote_sdp, "image");

#if 0
						sprintf (yRemoteConnectionInfo,
								 "destIP=%.*s&destFaxPort=%d", 15,
								 remote_sdp->o_addr,
								 atoi (media_media->m_port));
#endif

#if 1
						//DDN: MR4052:  3rd priority to sdp o=...IP4...<ip>.
						sprintf (yRemoteConnectionInfo,
								 "destIP=%s&destFaxPort=%d",
								 remote_sdp->o_addr,
								 atoi (media_media->m_port));

						//DDN: MR4052:  2nd priority to sdp c= IP4...<ip>.
						if (remote_sdp && remote_sdp->c_connection)
						{
	sdp_connection_t *c_tmp_sdp_connection = remote_sdp->c_connection;

							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   FAX_BASE, INFO,
									   "CALL_MODIFIED: Address retrieved from audio sdp details is (%s).",
									   c_tmp_sdp_connection->c_addr);

							sprintf (yRemoteConnectionInfo,
									 "destIP=%s&destFaxPort=%d",
									 c_tmp_sdp_connection->c_addr,
									 atoi (media_media->m_port));
						}

						//DDN: MR4052:  1st priority to sdp c= IP4..<ip>..  of m=image
						if (&media_media->c_connections != NULL)
						{
	sdp_connection_t *c_tmp_image_header =
		(sdp_connection_t *) osip_list_get (&media_media->c_connections, 0);

							if (c_tmp_image_header != NULL)
							{
								dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, FAX_BASE, INFO,
										   "CALL_MODIFIED: Address retrieved from image sdp details is (%s).",
										   c_tmp_image_header->c_addr);

								sprintf (yRemoteConnectionInfo,
										 "destIP=%s&destFaxPort=%d",
										 c_tmp_image_header->c_addr,
										 atoi (media_media->m_port));
							}
						}

#endif

						sprintf (yStrTmpSdpPort, "%d",
								 gCall[zCall].localSdpPort);
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "FAX: Found media type image, marking as fax call, dest info=(%s)",
								   yRemoteConnectionInfo);

						yRc =
							parseFaxSdpMsg (zCall, yRemoteConnectionInfo,
											yStrTmpRemoteSdp);

//                      dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
//                          "FAX: Creating outbound fax sdp.");
//                      yRc = createFaxSdpBody (zCall,
//                                              yStrTmpSdpPort, 
//                                              0,
//                                              0,
//                                              101);

#if 0							// djb - undo this - test for joe
						//RG added the following
						if (yRc <= 0)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "FAX: Failed to create outbound fax sdp.");

	struct MsgToDM  yReserveSendFaxResource;
							memcpy (&yReserveSendFaxResource,
									&(gCall[zCall].msgToDM),
									sizeof (struct MsgToDM));
							yReserveSendFaxResource.opcode =
								DMOP_FAX_RESERVE_RESOURCE;

							yRc =
								notifyMediaMgr (__LINE__, zCall,
												(struct MsgToDM *)
												&(yReserveSendFaxResource),
												mod);
							gCall[zCall].send200OKForFax = 1;
							gCall[zCall].sendFaxReinvite = 0;
							gCall[zCall].isFaxReserverResourceCalled = 1;
							gCall[zCall].cid = eXosipEvent->cid;
							gCall[zCall].did = eXosipEvent->did;
							gCall[zCall].tid = eXosipEvent->tid;

							if (gCall[zCall].faxServerSpecialFifo[0] == 0 &&
								gCall[zCall].faxDelayTimerStarted == 0)
							{
								dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, TEL_BASE, INFO,
										   "Adding to 4s timer.");
								gCall[zCall].faxDelayTimerStarted = 1;
//	struct timeb    yTmpTimeb;
	struct timeval	yTmpTimeb;

							//	ftime (&yTmpTimeb);
								gettimeofday(&yTmpTimeb, NULL);
								yTmpTimeb.tv_sec += 4;
								yRc = addToTimerList (__LINE__, zCall,
													  MP_OPCODE_CHECK_FAX_DELAY,
													  gCall[zCall].msgToDM,
													  yTmpTimeb, -1, -1, -1);
							}

							break;

						}
#endif
					}//fax
//  #endif
					if (strstr (yStrTmpRemoteSdp, "sendonly") != NULL ||
						strstr (yStrTmpRemoteSdp, "recvonly") != NULL ||
						strstr (yStrTmpRemoteSdp, "inactive") != NULL)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Setting isCallAnswered_noMedia = 1.");

						gCall[zCall].isCallAnswered_noMedia = 1;

						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Got sendonly in SDP message, waiting for sendrecv.");
					}
					else if (strstr (yStrTmpRemoteSdp, "m=image") != NULL)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Got Fax in SDP message; sending SIP Message 180 Ringing.");
						eXosip_lock (geXcontext);
						yRc =
							eXosip_call_send_answer (geXcontext, gCall[zCall].
													 reinvite_tid, 180, NULL);
						eXosip_unlock (geXcontext);
					}
					else
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Setting isCallAnswered_noMedia = 0.");

						gCall[zCall].isCallAnswered_noMedia = 0;
					}

					eXosip_lock (geXcontext);

					yRc =
						eXosip_call_build_answer (geXcontext, eXosipEvent->tid, yResponse,
												  &reInviteAnswer);
					if (yRc < 0)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
								   TEL_SIP_SIGNALING, ERR,
								   "eXosip_call_build_answer() failed.  rc=%d.  "
								   "Failed to build %d to re-INVITE message (tid:%d).",
								   yRc, eXosipEvent->tid, yResponse);
					}
					else
					{
						/*DDN_FAX: 02/25/2010 */
						if (gCall[zCall].faxData.isFaxCall == 1)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "FAX: Attaching fax sdp to 200 OK of REINVITE.");

							osip_message_set_body (reInviteAnswer,
												   gCall[zCall].
												   sdp_body_fax_local,
												   strlen (gCall[zCall].
														   sdp_body_fax_local));
							osip_message_set_content_type (reInviteAnswer,
														   "application/sdp");
							gCall[zCall].faxAlreadyGotReinvite = 1;
							gCall[zCall].send200OKForFax = 1;
							gCall[zCall].sendFaxReinvite = 0;
							// added Mon Oct  3 13:39:37 EDT 2011
							//eXosip_lock (geXcontext);
							//yRc = eXosip_call_send_answer (geXcontext, gCall[zCall].reinvite_tid, 200, reInviteAnswer);
							//eXosip_unlock (geXcontext);
							//fprintf(stderr, " %s() isedning 200 to reinvite yRc=%d\n", __func__, yRc);
							// end joes

						}
						/*END: DDN_FAX: 02/25/2010 */
						else
						{
							/*Send the old SDM info. TBD: Check of changes in the media */
							osip_message_set_body (reInviteAnswer,
												   gCall[zCall].
												   sdp_body_local,
												   strlen (gCall[zCall].
														   sdp_body_local));
							osip_message_set_content_type (reInviteAnswer,
														   "application/sdp");
							arc_fixup_contact_header (reInviteAnswer, gContactIp, gSipPort, zCall);

							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Sending SIP Message %d.", yResponse);

							yRc =
								eXosip_call_send_answer (geXcontext, eXosipEvent->tid,
														 yResponse,
														 reInviteAnswer);
							if (yRc < 0)
							{
								dynVarLog (__LINE__, zCall, mod,
										   REPORT_NORMAL, TEL_BASE, ERR,
										   "eXosip_call_send_answer() failed. rc=%d. "
										   "Unable to send %d to re-INVITE message (tid:%d).",
										   yRc, yResponse, eXosipEvent->tid);
							}
							else
							{
								dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, TEL_BASE, INFO,
										   "Sent %d to re-INVITE (tid:%d).",
										   yResponse, eXosipEvent->tid);
							}
						}
					}

					eXosip_unlock (geXcontext);

					if (yStrTmpRemoteSdp)
					{
						osip_free (yStrTmpRemoteSdp);
						yStrTmpRemoteSdp = NULL;
					}
                    if ( ( remote_sdp != NULL ) && ( audio_media != NULL ) )		// BT-81
                    {
                        restartMMOutputThread(zCall, remote_sdp, audio_media);
                    }
                    else
                    {
                        dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, INFO,
                            "Not restarting MM output thread:remote sdp / media is null.");
                    }

					break;
// MR3069:UAS
					if (gCall[zCall].PRACKSendRequire)
					{
						ringing = NULL;
						yRc = buildPRACKAnswer (zCall, &ringing);
						if (yRc == 0)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Sending SIP Message 180 Ringing with PRACK.");
							eXosip_lock (geXcontext);
							yRc =
								eXosip_call_send_answer (geXcontext, gCall[zCall].
														 reinvite_tid, 180,
														 ringing);
							eXosip_unlock (geXcontext);
						}
					}
					else
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Sending SIP Message 180 Ringing.");
						eXosip_lock (geXcontext);
						yRc =
							eXosip_call_send_answer (geXcontext, gCall[zCall].
													 reinvite_tid, 180, NULL);
						eXosip_unlock (geXcontext);
					}

					//eXosip_lock (geXcontext);
					//yRc = eXosip_call_send_answer (geXcontext, gCall[zCall].tid, 180, NULL);
					//eXosip_unlock (geXcontext);

					if (yRc < 0)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
								   TEL_SIP_SIGNALING, ERR,
								   "eXosip_call_send_answer() failed. rc=%d.  Unable to answer call.",
								   yRc);

						answer = NULL;

						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
								   TEL_BASE, WARN,
								   "Sending SIP Message 400 Bad Request.");

						eXosip_lock (geXcontext);
						yRc =
							eXosip_call_send_answer (geXcontext, gCall[zCall].
													 reinvite_tid, 400, NULL);
						eXosip_unlock (geXcontext);

						break;

					}

					answer = NULL;

					eXosip_lock (geXcontext);
					yRc =
						eXosip_call_build_answer (geXcontext, gCall[zCall].reinvite_tid,
												  200, &answer);
					eXosip_unlock (geXcontext);

					sprintf (yStrTmpSdpPort, "%d", gCall[zCall].localSdpPort);
					yRc = createSdpBody (zCall, yStrTmpSdpPort, -1, -1, -1);

					osip_message_set_body (answer,
										   gCall[zCall].sdp_body_local,
										   strlen (gCall[zCall].
												   sdp_body_local));

					osip_message_set_content_type (answer, "application/sdp");
					arc_fixup_contact_header (answer, gContactIp, gSipPort, zCall);

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Sending SIP Message 200 OK.");

					eXosip_lock (geXcontext);
					yRc =
						eXosip_call_send_answer (geXcontext, gCall[zCall].reinvite_tid,
												 200, answer);
					eXosip_unlock (geXcontext);

					if (yRc < 0)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
								   TEL_BASE, ERR,
								   "eXosip_answer_call() failed.  rc=%d.  Unable to answer call.",
								   yRc);
						break;
					}

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "eXosip_answer_call (EXOSIP_CALL_MODIFIED) succeeded.  rc=%d.",
							   yRc);

					gCall[zCall].remoteRtpIp[0] = '\0';
					snprintf (gCall[zCall].remoteRtpIp, 15, "%s",
							  remote_sdp->o_addr);

					gCall[zCall].remoteRtpPort = 0;
					gCall[zCall].remoteRtpPort = atoi (audio_media->m_port);

					if (gCall[zCall].remoteRtpPort > 0)
					{
						struct MsgToDM  msgRtpDetails;
						memcpy (&msgRtpDetails, &(gCall[zCall].msgToDM),
								sizeof (struct MsgToDM));

						msgRtpDetails.opcode = DMOP_RTPDETAILS;
						sprintf (msgRtpDetails.data, "%d^%d^%d^%d^%d^%d^%s",
								 gCall[zCall].leg,
								 gCall[zCall].remoteRtpPort,
								 gCall[zCall].localSdpPort,
								 (gCall[zCall].
								  telephonePayloadPresent) ? gCall[zCall].
								 telephonePayloadType : -99,
								 gCall[zCall].full_duplex,
								 gCall[zCall].codecType,
								 gCall[zCall].remoteRtpIp);

						//yRc = notifyMediaMgr (__LINE__, zCall, (struct MsgToDM *) &(msgRtpDetails), mod);
					}

                    // BT-51 - ERR: Timeout waiting for response from Dynamic Manager
                    if ( gCall[zCall].callState != CALL_STATE_CALL_BRIDGED)
                    {
                        setCallState (zCall, CALL_STATE_CALL_ACK);
                    }
				}

				break;

				//case EXOSIP_CALL_REFER_STATUS:
			case EXOSIP_SUBSCRIPTION_NOTIFY:

				if (eXosipEvent->notifyType == ARCSIP_NOTIFY_TYPE_DTMF
					&& eXosipEvent->dtmf >= 0 && eXosipEvent->dtmf < 16)
				{
				//	struct timeb    tb;
					struct timeval	tb;

				//	ftime (&tb);
					gettimeofday(&tb, NULL);

					if (isItFreshDtmf (zCall, tb))
					{
						struct MsgToDM  msgDtmfDetails;
						char            yStrTmpDtmf[5];

						memcpy (&msgDtmfDetails, &(gCall[zCall].msgToDM),
								sizeof (struct MsgToDM));

						msgDtmfDetails.opcode = DMOP_SETDTMF;
						msgDtmfDetails.appPid = gCall[zCall].appPid;
						msgDtmfDetails.appPassword = gCall[zCall].appPassword;

						sprintf (msgDtmfDetails.data, "%d",
								 eXosipEvent->dtmf);
						sprintf (yStrTmpDtmf, "%c",
								 dtmf_tab[eXosipEvent->dtmf]);

						__DDNDEBUG__ (DEBUG_MODULE_SIP, "Got DTMF",
									  yStrTmpDtmf, zCall);

						gCall[zCall].lastDtmfTime = tb;

						yRc =
							notifyMediaMgr (__LINE__, zCall,
											(struct MsgToDM *)
											&(msgDtmfDetails), mod);
					}
				}
				else if (eXosipEvent->notifyType ==
						 ARCSIP_NOTIFY_TYPE_CISCO_TRANSFER_RESULT
						 && gCall[zCall].callState ==
						 CALL_STATE_CALL_TRANSFER_ISSUED)
				{
					char	yTmpTransferResult[50];

					sprintf (yTmpTransferResult, "%s",
							 eXosipEvent->transferResult);

					if (strstr (yTmpTransferResult, "200") != NULL
						&& strstr (yTmpTransferResult, "OK") != NULL)
					{
						setCallState (zCall,
									  CALL_STATE_CALL_TRANSFERCOMPLETE);
						//gCall[zCall].callState = CALL_STATE_CALL_TRANSFERCOMPLETE;
						gCall[zCall].lastError = CALL_NO_ERROR;
					}
					else if (strstr (yTmpTransferResult, "180") != NULL
							 && strstr (yTmpTransferResult,
										"Ringing") != NULL)
					{
						//180:Ringing Do nothing.
					}
					else
					{
						char	*pch = strstr (yTmpTransferResult, "503");

						if (pch != NULL)
						{
							__DDNDEBUG__ (DEBUG_MODULE_CALL,
										  "Transfer Failed : 503 No Ring Back",
										  yTmpTransferResult, -1);
							gCall[zCall].eXosipStatusCode = 503;
							pch = NULL;
						}
						else if ((pch =
								  strstr (yTmpTransferResult, "508")) != NULL)
						{
							__DDNDEBUG__ (DEBUG_MODULE_CALL,
										  "Transfer Failed : 508 Busy Signal",
										  yTmpTransferResult, -1);
							gCall[zCall].eXosipStatusCode = 508;
							pch = NULL;
						}
						else
						{
							__DDNDEBUG__ (DEBUG_MODULE_CALL,
										  "Transfer Failed : General Failure",
										  yTmpTransferResult, -1);
						}

						dynVarLog (__LINE__, -1, mod, REPORT_NORMAL,
								   TEL_SIP_SIGNALING, ERR,
								   "Failed to transfer call. (%s)",
								   yTmpTransferResult);

						setCallState (zCall, CALL_STATE_CALL_TRANSFERFAILURE);
						//gCall[zCall].callState = CALL_STATE_CALL_TRANSFERFAILURE;
						gCall[zCall].lastError = -1;
					}
				}
				else
				{
					int	yRc = 0;

					int	yResponse = 488;

					osip_message_t *infoAnswer = NULL;

					dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE,
							   WARN,
							   "Sending SIP Message 488 Not Acceptable Here.");

					eXosip_lock (geXcontext);
					yRc =
						eXosip_call_build_answer (geXcontext, eXosipEvent->tid, yResponse,
												  &infoAnswer);
					arc_fixup_contact_header (infoAnswer, gHostIp, gSipPort,
											  zCall);
					yRc =
						eXosip_call_send_answer (geXcontext, eXosipEvent->tid, yResponse,
												 infoAnswer);
					eXosip_unlock (geXcontext);

					if (yRc < 0)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
								   TEL_SIP_SIGNALING, ERR,
								   "eXosip_call_send_answer() failed. rc=%d.  "
								   "Failed to send %d to INFO message (tid:%d).",
								   yRc, yResponse, eXosipEvent->tid);
					}
					else
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Sent %d to INFO message (tid:%d).",
								   yResponse, eXosipEvent->tid);
					}
				}

				break;

			case EXOSIP_CALL_INVITE:
				{
					struct MsgToRes lMsgToRes;

					char            dnis[256];
					char            ani[256];

					char           *dnisNoSpecialChars = NULL;
					char           *aniNoSpecialChars = NULL;

					char           *yStrTo = NULL;
					char           *yStrFrom = NULL;
					char           *yStrContact = NULL;

					osip_uri_param_t *yToTag = NULL;
					osip_uri_param_t *yFromTag = NULL;

					osip_contact_t *con = NULL;

					//gCallsStarted++;

					sprintf (ani, "%s", "\0");
					sprintf (dnis, "%s", "\0");

                    if (eXosipEvent->request &&
                        eXosipEvent->request->call_id &&
                        eXosipEvent->request->call_id->number)
                    {
                        dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
                               INFO,
                               "eXosip event EXOSIP_CALL_INVITE received. sipCallId=%s",
                                eXosipEvent->request->call_id->number);
                    }
                    else
                    {
                        dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
                               INFO,
                               "Warning: eXosip event EXOSIP_CALL_INVITE received, but no call id is associated with it.");
                    }

					if (gShutDownFlag == 1)
					{
						time_t yTmpShutDownCheckTime;

						time (&yTmpShutDownCheckTime);

						if (yTmpShutDownCheckTime - gShutDownFlagTime > 30)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
									   TEL_CALLMGR_SHUTDOWN, ERR,
									   "Receiving calls 30 seconds after attempting shut down. Sending deRegistration.");

							// first unregister SIP registers 
							deRegistrationHandler (&RegistrationInfoList);
						}

                        // added JOES -- Mon Oct 26 09:45:36 EDT 2015
                        // this will cause the call manager to exit forcefully 
						// so many seconds after gShutDownFlag is set.

						if (yTmpShutDownCheckTime - gShutDownFlagTime >= gShutDownMaxTime)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
									   TEL_CALLMGR_SHUTDOWN, ERR,
									   " CallManager is taking a long time to shutdown, triggering a forceful shutdown after %d seconds ", gShutDownMaxTime);

                            doDynMgrExit(zCall, __func__, __LINE__);
						}

					}

					if (gShutDownFromRedirector == 1)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
								   TEL_RESOURCE_NOT_AVAILABLE, WARN,
								   "Received shutdown message from redirector, so there are no ports available. Waiting for ports to clear.");

						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
								   TEL_BASE, WARN,
								   "Sending SIP Message 486 Busy Here.");

						eXosip_lock (geXcontext);
						yRc =
							eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 486,
													 0);
						eXosip_unlock (geXcontext);

						continue;
					}
					else if (gReceivedHeartBeat == 0)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
								   TEL_HEARTBEAT, ERR,
								   "No Heartbeat. Rejecting the call");

						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
								   TEL_BASE, WARN,
								   "Sending SIP Message 486 Busy Here.");

						eXosip_lock (geXcontext);
						yRc =
							eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 486,
													 0);
						eXosip_unlock (geXcontext);

						continue;
					}

					/*Check if request is present */
					if (!eXosipEvent->request)
					{
						osip_message_t *infoAnswer = NULL;

						eXosip_lock (geXcontext);
						yRc =
							eXosip_call_build_answer (geXcontext, eXosipEvent->tid, 400,
													  &infoAnswer);
						eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 400,
												 infoAnswer);
						eXosip_unlock (geXcontext);

						break;
					}

					/* Check if main URL in the request is present */
					sipMainUri = osip_message_get_uri (eXosipEvent->request);

					if (!sipMainUri)
					{
						osip_message_t *infoAnswer = NULL;

						eXosip_lock (geXcontext);
						yRc =
							eXosip_call_build_answer (geXcontext, eXosipEvent->tid, 400,
													  &infoAnswer);
						eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 400,
												 infoAnswer);
						eXosip_unlock (geXcontext);

						break;
					}

					eXosip_lock (geXcontext);
					con =
						(osip_contact_t *) osip_list_get (&eXosipEvent->
														  request->contacts,
														  0);
					eXosip_unlock (geXcontext);

					if (con == NULL)
					{
						osip_message_t *infoAnswer = NULL;

						eXosip_lock (geXcontext);
						yRc =
							eXosip_call_build_answer (geXcontext, eXosipEvent->tid, 400,
													  &infoAnswer);
						eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 400,
												 infoAnswer);
						eXosip_unlock (geXcontext);

						break;
					}

					// MR3069:UAS - get '100rel' from INVITE and determine/set
					//              action to be taken

					yRc = checkINVITEForPRACK (zCall, eXosipEvent);
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "checkINVITEForPRACK() returned %d; PRACK_REJECT_420(%d), "
							   "PRACK_REJECT_403(%d), PRACK_NORMAL_CALL(%d), PRACK_SEND_REQUIRE(%d).",
							   yRc, PRACK_REJECT_420, PRACK_REJECT_403,
							   PRACK_NORMAL_CALL, PRACK_SEND_REQUIRE);

					if (yRc == PRACK_REJECT_403)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
								   TEL_BASE, WARN,
								   "Sending SIP Message 403 Forbidden.");

						eXosip_lock (geXcontext);
						yRc =
							eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 403,
													 0);
						eXosip_unlock (geXcontext);

						continue;
					}
					else if (yRc == PRACK_REJECT_420)
					{
						osip_message_t *reject420 = NULL;
						int	yResponse = 420;

						eXosip_lock (geXcontext);

						yRc =
							eXosip_call_build_answer (geXcontext, eXosipEvent->tid,
													  yResponse, &reject420);
						dynVarLog (__LINE__, -1, mod, REPORT_DETAIL, TEL_BASE,
								   WARN,
								   "Sending SIP Message 420 Unsupported Header for 100rel.");

						osip_message_set_header (reject420, "Unsupported",
												 "100rel");

						yRc =
							eXosip_call_send_answer (geXcontext, eXosipEvent->tid,
													 yResponse, reject420);
						if (yRc < 0)
						{
							dynVarLog (__LINE__, -1, mod, REPORT_NORMAL,
									   TEL_SIP_SIGNALING, ERR,
									   "eXosip_call_send_answer() failed.  rc=%d. "
									   "Unable to send 420 Unsupported Header for 100rel (tid:%d).",
									   yRc, eXosipEvent->tid);
						}
						else
						{
							dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Sent 420 Unsupported Header for 100rel (tid:%d).",
									   eXosipEvent->tid);
						}

						eXosip_unlock (geXcontext);

						continue;
					}
					else if (yRc == PRACK_SEND_REQUIRE)
					{
						// MR 3069
						yIntPRACKSendRequire = 1;
					}

					yRc = getFreeCallNum (INBOUND);

					if (yRc < 0)
					{
						// The log message is logged in the getFreeCallNum()
						// routine.  No need here.
						//
						// dynVarLog (__LINE__, -1, mod, REPORT_NORMAL,
						// 		   TEL_RESOURCE_NOT_AVAILABLE, ERR,
						// 		   "No free inbound call slot available. Rejecting the call");

						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
								   TEL_BASE, WARN,
								   "Sending SIP Message 486 Busy Here.");

						eXosip_lock (geXcontext);
						yRc =
							eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 486,
													 0);
						//eXosip_call_terminate (geXcontext, eXosipEvent->cid, eXosipEvent->did);
						eXosip_unlock (geXcontext);

						continue;
					}

					zCall = yRc;

				 	write_headers_to_file(zCall, eXosipEvent->request);

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "For Call-ID (%s): Free call slot available (%d).",
                                eXosipEvent->request->call_id->number, yRc);

					gCall[zCall].sipTo =
						osip_message_get_to (eXosipEvent->request);

					gCall[zCall].sipFrom =
						osip_message_get_from (eXosipEvent->request);

					arc_save_sip_from(zCall, gCall[zCall].sipFrom);	// BT-123
					arc_save_sip_to(zCall, gCall[zCall].sipTo);

					osip_message_get_contact (eXosipEvent->request, 0,
											  &gCall[zCall].sipContact);

					if (gCall[zCall].sipContact
						&& gCall[zCall].sipContact->url
						&& gCall[zCall].sipContact->url->username)
					{
						sprintf (gCall[zCall].sipContactUsername, "%s",
								 gCall[zCall].sipContact->url->username);
					}

					osip_to_to_str (gCall[zCall].sipTo, &yStrTo);
					sprintf (gCall[zCall].sipToStr, "%s", yStrTo);
					osip_free (yStrTo);
					yStrTo = NULL;

					osip_from_to_str (gCall[zCall].sipFrom, &yStrFrom);
					sprintf (gCall[zCall].sipFromStr, "%s", yStrFrom);
					osip_free (yStrFrom);
					yStrFrom = NULL;

					osip_contact_to_str (gCall[zCall].sipContact,
										 &yStrContact);
					sprintf (gCall[zCall].sipContactStr, "%s", yStrContact);
					osip_free (yStrContact);
					yStrContact = NULL;

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "Saved To(%s) From(%s) Contact(%s) from initial INVITE.",
							   gCall[zCall].sipToStr, gCall[zCall].sipFromStr,
							   gCall[zCall].sipContactStr);

					osip_to_get_tag (gCall[zCall].sipTo, &yToTag);
					osip_to_get_tag (gCall[zCall].sipFrom, &yFromTag);

					if (yToTag && yToTag->gvalue)
					{
						sprintf (gCall[zCall].tagToValue, "%s",
								 yToTag->gvalue);
						__DDNDEBUG__ (DEBUG_MODULE_SIP, "Incoming tagToValue",
									  gCall[zCall].tagToValue, zCall);
					}

					if (yFromTag && yFromTag->gvalue)
					{
						sprintf (gCall[zCall].tagFromValue, "%s",
								 yFromTag->gvalue);
						__DDNDEBUG__ (DEBUG_MODULE_SIP,
									  "Incoming tagFromValue",
									  gCall[zCall].tagFromValue, zCall);
					}

					/*Check for P-Charge-Info DDN 06/13/2011 */
	osip_header_t  *pChargeInfoHeader = NULL;

					sprintf (gCall[zCall].GV_PChargeInfo, "\0");

					if (osip_message_header_get_byname
						(eXosipEvent->request, "P-Charge-Info", 0,
						 &pChargeInfoHeader) > -1)
					{
						if (pChargeInfoHeader && pChargeInfoHeader->hvalue)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO, "P-Charge-Info: %s",
									   pChargeInfoHeader->hvalue);

							sprintf (gCall[zCall].GV_PChargeInfo, "%s",
									 pChargeInfoHeader->hvalue);
						}
					}
					/*END: Check for P-Charge-Info DDN 06/13/2011 */

					if (eXosipEvent->request &&
						eXosipEvent->request->call_id &&
						eXosipEvent->request->call_id->number)
					{
						if (eXosipEvent->request->call_id->host
							&& eXosipEvent->request->call_id->host[0])
						{
							sprintf (gCall[zCall].sipCallId, "%s@%s",
									 eXosipEvent->request->call_id->number,
									 eXosipEvent->request->call_id->host);
						}
						else
						{
							sprintf (gCall[zCall].sipCallId, "%s",
									 eXosipEvent->request->call_id->number);
						}

						__DDNDEBUG__ (DEBUG_MODULE_SIP, "Incoming CallID",
									  gCall[zCall].sipCallId, zCall);
					}

					// clear the gCall structure 
					clearPort (zCall);
					gCall[zCall].PRACKSendRequire = yIntPRACKSendRequire;
					yIntPRACKSendRequire = 0;

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Set gCall[%d].PRACKSendRequire to %d",
							   zCall, gCall[zCall].PRACKSendRequire);

						// somewhere
					if ( (rc = gCallInfo[zCall].ProcessSessionTimers (mod, __LINE__, zCall, eXosipEvent)) == -1)
					{
						osip_message_t *answer = NULL;
						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_BASE, WARN, "Sending SIP Message 422.");

						eXosip_lock (geXcontext);
						eXosip_call_build_answer (geXcontext, eXosipEvent->tid, 422, &answer);
						osip_message_set_header (answer, "Min-SE", "90");		// BT-118
						yRc = eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 422, answer);
						eXosip_unlock (geXcontext);

						if (remote_sdp != NULL)
						{
							sdp_message_free (remote_sdp);
							remote_sdp = NULL;
						}
						break;
					}
					rc = gCallInfo[zCall].GetIntOption("session-expires");
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "Retrieved session-expires of %d", rc);
					if ( rc > 0 )
					{
                        gCall[zCall].sessionTimersSet = 1;
                        dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
                               INFO, "Set sessionTimers to %d", gCall[zCall].sessionTimersSet);
					}

					if (con != NULL && con->url != NULL && con->url->host != NULL)
					{
						snprintf (gCall[zCall].remoteContactIp, 15, "%s",
								  con->url->host);
						gCall[zCall].remoteContactPort =
							osip_atoi (con->url->port);
					}
					else
					{
						sprintf (gCall[zCall].remoteContactIp, "%s", "\0");
						gCall[zCall].remoteContactPort = -1;
					}

					__DDNDEBUG__ (DEBUG_MODULE_SIP, "Contact host and IP",
								  gCall[zCall].remoteContactIp,
								  gCall[zCall].remoteContactPort);

					setOcnRdn (zCall, eXosipEvent->request);

					memset (gCall[zCall].sipMessage, 0,
							sizeof (gCall[zCall].sipMessage));

					{
	char           *yStrSipMessageBuffer = NULL;
	size_t          yIntSipMessageLen = 0;
	int             yIntOsipMessageToStrRc = -1;

						yIntOsipMessageToStrRc =
							osip_message_to_str (eXosipEvent->request,
												 &yStrSipMessageBuffer,
												 (size_t *) &
												 yIntSipMessageLen);
						if (yIntOsipMessageToStrRc != 0)
						{
	osip_message_t *infoAnswer = NULL;

							eXosip_lock (geXcontext);
							yRc =
								eXosip_call_build_answer (geXcontext, eXosipEvent->tid,
														  400, &infoAnswer);
							eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 400,
													 infoAnswer);
							eXosip_unlock (geXcontext);

							dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
									   TEL_SIP_SIGNALING, ERR,
									   "osip_message_to_str() failed. rc=%d. Unable to convert message to string."
									   " Sending SIP Message 400 Bad Request.",
									   yIntOsipMessageToStrRc);

							break;
						}
						else
						{
							if (yIntSipMessageLen < 4095)
							{
								sprintf (gCall[zCall].sipMessage, "%s",
										 yStrSipMessageBuffer);
							}
							else
							{
								snprintf (gCall[zCall].sipMessage, 4095, "%s",
										  yStrSipMessageBuffer);
							}
							osip_free (yStrSipMessageBuffer);
						}
					}

					memset (ringEventTime, 0, sizeof (ringEventTime));
					getRingEventTime (ringEventTime);
					sprintf (gCall[zCall].lastEventTime, "%s", ringEventTime);

					setCallState (zCall, CALL_STATE_CALL_NEW);

					gCall[zCall].lastError = CALL_NO_ERROR;

					gCall[zCall].cid = eXosipEvent->cid;
					gCall[zCall].did = eXosipEvent->did;
					gCall[zCall].tid = eXosipEvent->tid;

					if (gInboundEarlyMedia == 0)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "gCall[%d].PRACKSendRequire=%d", zCall,
								   gCall[zCall].PRACKSendRequire);

						if (gCall[zCall].PRACKSendRequire)
						{
							ringing = NULL;
							yRc = buildPRACKAnswer (zCall, &ringing);
							if (yRc == 0)
							{
								dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, TEL_BASE, INFO,
										   "Sending SIP Message 180 Ringing with PRACK.");

								eXosip_lock (geXcontext);
								yRc =
									eXosip_call_send_answer (geXcontext, gCall[zCall].tid,
															 180, ringing);
								eXosip_unlock (geXcontext);

								gCall[zCall].nextSendPrackSleep *= 2;
							}
						}
						else
						{

                            if(gSipOutOfLicenseResponse != 486){

							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Sending SIP Message 180 Ringing.");
							eXosip_lock (geXcontext);
							//yRc = eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 101, NULL);
// MR3069:UAS
							yRc =
								eXosip_call_send_answer (geXcontext, eXosipEvent->tid,
														 180, NULL);
							eXosip_unlock (geXcontext);
                            } else {
                                dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
                                       TEL_BASE, INFO,
                                       "Delay sending SIP Message 180 Ringing Due to Out Of License Response set to 486");
                            }
						}
					}

					voicexml_param = NULL;
					play_param = NULL;
					early_param = NULL;
					delay_param = NULL;
					repeat_param = NULL;
					aai_param = NULL;

					osip_uri_uparam_get_byname (sipMainUri, "voicexml",
												&voicexml_param);
					osip_uri_uparam_get_byname (sipMainUri, "aai",
												&aai_param);
					osip_uri_uparam_get_byname (sipMainUri, "play",
												&play_param);
					osip_uri_uparam_get_byname (sipMainUri, "early",
												&early_param);

					if (voicexml_param != NULL
						&& voicexml_param->gvalue != NULL)
					{
						sprintf (gCall[zCall].GV_SipUriVoiceXML, "%s",
								 voicexml_param->gvalue);
					}
					else
					{
						sprintf (gCall[zCall].GV_SipUriVoiceXML, "%s", "\0");
					}

					if (aai_param != NULL && aai_param->gvalue != NULL)
					{
						sprintf (gCall[zCall].remoteAAI, "%s",
								 aai_param->gvalue);
					}
					else
					{
						sprintf (gCall[zCall].remoteAAI, "%s", "\0");
					}

					gCall[zCall].GV_SipUriPlay[0] = 0;
					gCall[zCall].GV_SipUriEarly[0] = 0;

					/*net-ann check: play is mandetory if username is annc */
					if (sipMainUri->username != NULL
						&& strcmp (sipMainUri->username, "dialog") == 0)
					{
						if (!voicexml_param || voicexml_param->gvalue == NULL)
						{
	osip_message_t *infoAnswer = NULL;
	char            responseMsg[] = "Missing VXML Value";

							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Sending SIP Message 400 Bad Request.");

							eXosip_lock (geXcontext);
							yRc =
								eXosip_call_build_answer (geXcontext, eXosipEvent->tid,
														  400, &infoAnswer);
							if (infoAnswer)
							{
								osip_message_set_body (infoAnswer,
													   responseMsg,
													   strlen (responseMsg));
								eXosip_call_send_answer (geXcontext, eXosipEvent->tid,
														 400, infoAnswer);
							}
							else
							{
								dynVarLog (__LINE__, zCall, mod,
										   REPORT_DIAGNOSE, TEL_SIP_SIGNALING,
										   WARN,
										   "eXosip_call_build_answer() failed.  Unable to build answer. rc=%d.",
										   yRc);
							}
							eXosip_unlock (geXcontext);

							if (remote_sdp != NULL)
							{
								sdp_message_free (remote_sdp);
								remote_sdp = NULL;
							}

							break;
						}
					}

					if (sipMainUri->username != NULL
						&& strcmp (sipMainUri->username, "annc") == 0)
					{
						if (!play_param || play_param->gvalue == NULL)
						{
	osip_message_t *infoAnswer = NULL;
	char            responseMsg[] = "Mandatory play parameter missing";

							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, WARN,
									   "Sending SIP Message 400 Bad Request.");

							eXosip_lock (geXcontext);
							yRc =
								eXosip_call_build_answer (geXcontext, eXosipEvent->tid,
														  400, &infoAnswer);
							if (infoAnswer)
							{
								osip_message_set_body (infoAnswer,
													   responseMsg,
													   strlen (responseMsg));
								eXosip_call_send_answer (geXcontext, eXosipEvent->tid,
														 400, infoAnswer);
							}
							else
							{
								dynVarLog (__LINE__, zCall, mod,
										   REPORT_DIAGNOSE, TEL_SIP_SIGNALING,
										   WARN,
										   "eXosip_call_build_answer() failed.  Unable to build answer. rc=%d.",
										   yRc);
							}
							eXosip_unlock (geXcontext);

							if (remote_sdp != NULL)
							{
								sdp_message_free (remote_sdp);
								remote_sdp = NULL;
							}

							break;
						}

						if (play_param != NULL && play_param->gvalue != NULL)
						{
							if (strncmp
								(play_param->gvalue, "http://",
								 strlen ("http://")) == 0
								|| strncmp (play_param->gvalue, "https://",
											strlen ("https://")) == 0
								|| strncmp (play_param->gvalue, "file://",
											strlen ("file://")) == 0
								|| strncmp (play_param->gvalue, "ftp://",
											strlen ("ftp://")) == 0)
							{
							}
							else
							{
	char            yFileName[512] = "";

								sprintf (yFileName, "vxi/%s",
										 play_param->gvalue);
								if (access (yFileName, R_OK) != 0)
								{
	osip_message_t *infoAnswer = NULL;
	char            responseMsg[] = "Announcement content not found";

									dynVarLog (__LINE__, zCall, mod,
											   REPORT_DETAIL, TEL_BASE, WARN,
											   "Sending SIP Message 404 Not Found.");

									eXosip_lock (geXcontext);
									yRc =
										eXosip_call_build_answer (geXcontext, eXosipEvent->tid, 404, &infoAnswer);
									osip_message_set_body (infoAnswer,
														   responseMsg,
														   strlen
														   (responseMsg));
									eXosip_call_send_answer (geXcontext, eXosipEvent->tid,
															 404, infoAnswer);
									eXosip_unlock (geXcontext);

									if (remote_sdp != NULL)
									{
										sdp_message_free (remote_sdp);
										remote_sdp = NULL;
									}

									break;
								}
							}
						}
					}

					if (play_param != NULL)
					{
						sprintf (gCall[zCall].GV_SipUriPlay, "%s",
								 play_param->gvalue);

						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO, "Net-ann play URI(%s)",
								   gCall[zCall].GV_SipUriPlay);

						if (early_param != NULL)
						{
							sprintf (gCall[zCall].GV_SipUriEarly, "%s",
									 early_param->gvalue);
						}

						osip_uri_uparam_get_byname (sipMainUri, "repeat",
													&repeat_param);
						osip_uri_uparam_get_byname (sipMainUri, "delay",
													&delay_param);

						createNetworkAnnouncementVxmlScript (zCall,
															 repeat_param,
															 delay_param);
					}
					else if (gCall[zCall].GV_SipUriVoiceXML[0] != '\0')
					{
						createNetworkAnnouncementVxmlURIScript (zCall);
					}

					__DDNDEBUG__ (DEBUG_MODULE_SIP, "voicexml=",
								  gCall[zCall].GV_SipUriVoiceXML, zCall);

					/*END: DDN_RG_TODO: Set gCall[zCall].GV_SipUriVoiceXML to voicexml parameter of uri request */

					eXosip_lock (geXcontext);
					remote_sdp = eXosip_get_sdp_info (eXosipEvent->request);
					gCall[zCall].remote_sdp =
						eXosip_get_sdp_info (eXosipEvent->request);
					eXosip_unlock (geXcontext);

					if (remote_sdp == NULL)
					{

						__DDNDEBUG__ (DEBUG_MODULE_CALL,
									  "There is no remote sdp, SLOW START, creating MRCP RTP Details",
									  "", zCall);

						gCall[zCall].codecType = 0;
						gCall[zCall].telephonePayloadType = 96;
						gCall[zCall].telephonePayloadType_2 = 101;
						gCall[zCall].telephonePayloadType_3 = 120;
						gCall[zCall].telephonePayloadType_4 = 127;
						createMrcpRtpDetails (zCall);
					}
					else
					{
						//eXosip_lock (geXcontext);
						audio_media = eXosip_get_audio_media (remote_sdp);
						//eXosip_unlock (geXcontext);

						if (audio_media == NULL
							|| atoi (audio_media->m_port) <= 0)
						{

							__DDNDEBUG__ (DEBUG_MODULE_CALL,
										  "The audio media does not exist!",
										  "", zCall);

							dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
									   TEL_BASE, WARN,
									   "Sending SIP Message 404 Not Found.");

							eXosip_lock (geXcontext);
							eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 404,
													 0);
							//eXosip_call_terminate (geXcontext, eXosipEvent->cid, eXosipEvent->did);
							eXosip_unlock (geXcontext);

							if (remote_sdp != NULL)
							{
								sdp_message_free (remote_sdp);
								remote_sdp = NULL;
							}

							break;
						}

					}

					if (sipMainUri->port)
					{
						snprintf (gCall[zCall].originalDnis, 128, "%s@%s:%s",
								  sipMainUri->username, sipMainUri->host,
								  sipMainUri->port);

					}
					else
					{
						snprintf (gCall[zCall].originalDnis, 128, "%s@%s",
								  sipMainUri->username, sipMainUri->host);
					}

					if (sipMainUri->username == NULL)
					{
						sprintf (dnis, "%s", "anonymous");
					}
					else
					{
						sprintf (dnis, "%s", sipMainUri->username);
					}

					sipFrom = osip_message_get_from (eXosipEvent->request);
					sipUrl = sipFrom->url;

					if (sipUrl && sipUrl->host)
					{
						snprintf (gCall[zCall].remoteSipIp, 16, "%s",
								  sipUrl->host);
					}

					if (sipUrl->port && sipUrl->port[0])
					{
						gCall[zCall].remoteSipPort = atoi (sipUrl->port);
					}
					else
					{
						gCall[zCall].remoteSipPort = 5060;
					}

					memset (gCall[zCall].GV_SipURL, 0, 2048);
					//getSIPURL(gCall[zCall].GV_SipURL);

					if (sipUrl->username != NULL)
					{
	char            tempUserName[128] = "";
	int             ispAIdentity = 0;

						sprintf (tempUserName, "%s", sipUrl->username);
						sprintf (username, "%s", sipUrl->username);

						if (strcasecmp (sipUrl->username, "anonymous") == 0)
						{
	char            sipbuf[4000];
	char            value[256] = "";
	char            lhs[32];
	char            rhs[32];
	char            pAIdentity_val[32] = "";

	char           *yStrSipMessageBuffer = NULL;
	size_t          yIntSipMessageLen = 0;
	int             yIntOsipMessageToStrRc = -1;
	char           *pAIdentity = NULL;

							yIntOsipMessageToStrRc =
								osip_message_to_str (eXosipEvent->request,
													 &yStrSipMessageBuffer,
													 (size_t *) &
													 yIntSipMessageLen);

							if (yStrSipMessageBuffer)
							{
								sprintf (sipbuf, "%s", yStrSipMessageBuffer);

								osip_free (yStrSipMessageBuffer);

								pAIdentity =
									strstr (sipbuf, "P-Asserted-Identity:");

								if (pAIdentity == NULL)
								{
									pAIdentity =
										strstr (sipbuf,
												"P-asserted-identity:");
								}

								if (pAIdentity == NULL)
								{
									pAIdentity =
										strstr (sipbuf,
												"p-asserted-identity:");
								}
							}

							if (pAIdentity != NULL)
							{

	osip_from_t    *pIdentityUrl = NULL;

								osip_from_init (&pIdentityUrl);
								osip_from_parse (pIdentityUrl, pAIdentity);

								if (pIdentityUrl && pIdentityUrl->url)
								{
									//user name
									if (pIdentityUrl->url->username)
									{
										sprintf (pAIdentity_val, "%s",
												 pIdentityUrl->url->username);
										ispAIdentity = 1;
									}
									//ip address
									if (pIdentityUrl->url->host)
									{
										sprintf (gCall[zCall].pAIdentity_ip,
												 "%s",
												 pIdentityUrl->url->host);
									}
								}
								else
								{
									ispAIdentity = 0;
								}
								osip_from_free (pIdentityUrl);
#if 0
	int             len = strlen (pAIdentity);
	char           *startpoint = strstr (pAIdentity, "<");
	char           *endpoint = strstr (pAIdentity, ">");

								if (startpoint != NULL && endpoint != NULL)
								{
	int             len1 = strlen (startpoint);
	int             len2 = strlen (endpoint);

									memcpy (value, startpoint + 1,
											len1 - len2 - 1);

									sprintf (lhs, "%s", "\0");
									sprintf (rhs, "%s", "\0");

									sscanf (value, "%[^@]=%s", lhs, rhs);

									memcpy (pAIdentity_val, lhs + 4,
											strlen (lhs) - 4);
									dynVarLog (__LINE__, zCall, mod,
											   REPORT_VERBOSE, DYN_BASE, INFO,
											   "lhs=%s, length=%d value=%s",
											   lhs, strlen (lhs), value);

									memcpy (gCall[zCall].pAIdentity_ip,
											value + strlen (lhs) + 1,
											strlen (value) - strlen (lhs) -
											1);

								}
#endif
							}

							if (ispAIdentity == 1)
							{
								gCall[zCall].presentRestrict = 1;
								if (strcmp (dnis, pAIdentity_val) == 0)
								{
									sprintf (tempUserName, "%s", dnis);
									//sprintf(ani, "%s", dnis);
									//memset(sipUrl->username, 0, sizeof(sipUrl->username));
									//sprintf(sipUrl->username, "%s", dnis);
									sprintf (username, "%s", dnis);
									if (sipUrl->port && sipUrl->port[0])
									{
										sprintf (gCall[zCall].originalAni,
												 "sip:%s@%s:%s", tempUserName,
												 sipUrl->host, sipUrl->port);
									}
									else
									{
										sprintf (gCall[zCall].originalAni,
												 "sip:%s@%s", tempUserName,
												 sipUrl->host);
									}
									dynVarLog (__LINE__, zCall, mod,
											   REPORT_VERBOSE, TEL_BASE, INFO,
											   "Remote URI(%s) Local URI(%s)",
											   gCall[zCall].originalAni,
											   gCall[zCall].originalDnis);
								}
								else
								{
									sprintf (tempUserName, "%s",
											 pAIdentity_val);
									//sprintf(ani, "%s", dnis);
									//memset(sipUrl->username, 0, sizeof(sipUrl->username));
									//sprintf(sipUrl->username, "%s", pAIdentity_val);
									memset (username, 0, 128);
									sprintf (username, "%s", pAIdentity_val);
									if (sipUrl->port && sipUrl->port[0])
									{
										sprintf (gCall[zCall].originalAni,
												 "sip:%s@%s:%s", tempUserName,
												 sipUrl->host, sipUrl->port);
									}
									else
									{
										sprintf (gCall[zCall].originalAni,
												 "sip:%s@%s", tempUserName,
												 sipUrl->host);
									}
									dynVarLog (__LINE__, zCall, mod,
											   REPORT_VERBOSE, TEL_BASE, INFO,
											   "Remote URI(%s) Local URI(%s)",
											   gCall[zCall].originalAni,
											   gCall[zCall].originalDnis);
								}
							}

						}

						if (ispAIdentity == 0)
						{
							gCall[zCall].presentRestrict = 0;
							if (sipUrl->port && sipUrl->port[0])
							{
								sprintf (gCall[zCall].originalAni, "%s@%s:%s",
										 tempUserName, sipUrl->host,
										 sipUrl->port);
							}
							else
							{
								sprintf (gCall[zCall].originalAni, "%s@%s",
										 tempUserName, sipUrl->host);
							}
						}
					}
#if 0

					if (sipUrl->username != NULL)
					{
						//  sprintf(gCall[zCall].originalAni, "%s@%s", sipUrl->username, sipUrl->host);
						if (sipUrl->port && sipUrl->port[0])
						{
							sprintf (gCall[zCall].originalAni, "%s@%s:%s",
									 sipUrl->username, sipUrl->host,
									 sipUrl->port);
						}
						else
						{
							sprintf (gCall[zCall].originalAni, "%s@%s",
									 sipUrl->username, sipUrl->host);
						}

					}
#endif
					else
					{
						//  sprintf(gCall[zCall].originalAni, "anonymous@%s", sipUrl->host);
						memset (username, 0, 128);
						if (sipUrl->port && sipUrl->port[0])
						{
							snprintf (gCall[zCall].originalAni, 128,
									  "anonymous@%s:%s", sipUrl->host,
									  sipUrl->port);
						}
						else
						{
							snprintf (gCall[zCall].originalAni, 128,
									  "anonymous@%s", sipUrl->host);
						}

					}

					if (remote_sdp == NULL)
					{
						gCall[zCall].remoteRtpIp[0] = '\0';
					}

					if (sipUrl->username == NULL)
					{
						sprintf (ani, "%s", "anonymous");
					}
					else
					{
						snprintf (ani, 128, "%s", username);
					}

#if 0
					sipTo = osip_message_get_to (eXosipEvent->request);
					sipUrl = sipTo->url;
#endif

					//sprintf(gCall[zCall].originalDnis, "%s@%s", sipUrl->username, sipUrl->host);
					snprintf (gCall[zCall].originalDnis, 128, "%s@%s:%s",
							  sipMainUri->username, sipMainUri->host,
							  sipMainUri->port);

					__DDNDEBUG__ (DEBUG_MODULE_CALL, "Remote Ip",
								  gCall[zCall].originalAni, zCall);
					__DDNDEBUG__ (DEBUG_MODULE_CALL, "Local Ip",
								  gCall[zCall].originalDnis, zCall);

					if (strlen (gCall[zCall].originalAni) > 256
						|| strlen (gCall[zCall].originalDnis) > 256)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
								   TEL_BASE, WARN,
								   "Sending SIP Message 404 Not Found.");

						eXosip_lock (geXcontext);
						eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 404, 0);
						eXosip_call_terminate (geXcontext, eXosipEvent->cid,
											   eXosipEvent->did);
						eXosip_unlock (geXcontext);

						if (remote_sdp != NULL)
						{
							sdp_message_free (remote_sdp);
							remote_sdp = NULL;
						}

						break;
					}

					if (remote_sdp != NULL)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Remote URI(%s) Local URI(%s) Remote Audio Port (%s)",
								   gCall[zCall].originalAni,
								   gCall[zCall].originalDnis,
								   audio_media->m_port);
					}
					else
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "Remote URI(%s) Local URI(%s) : SLOW START",
								   gCall[zCall].originalAni,
								   gCall[zCall].originalDnis);
					}

					if (remote_sdp != NULL)
					{
						gCall[zCall].remoteRtpIp[0] = '\0';
						snprintf (gCall[zCall].remoteRtpIp, 15, "%s",
								  remote_sdp->o_addr);
					}

					gCall[zCall].remoteRtpPort = 0;

					if (remote_sdp != NULL)
					{
						gCall[zCall].remoteRtpPort =
							atoi (audio_media->m_port);
					}
					//gCall[zCall].localSdpPort = LOCAL_STARTING_RTP_PORT + (zCall * 2);

					memset (&lMsgToRes, 0, sizeof (struct MsgToRes));

					lMsgToRes.opcode = RESOP_FIREAPP;
					lMsgToRes.appCallNum = zCall;
					lMsgToRes.appPassword = gCall[zCall].appPassword;
					lMsgToRes.dynMgrPid = getpid ();

					sprintf (lMsgToRes.ringEventTime, "%s", ringEventTime);

					dnis[31] = '\0';
					ani[31] = '\0';

					/*DDNDEBUG: Remove any characters that are not recognized by ArcIPResp */
					dnisNoSpecialChars = strchr (dnis, ';');

					if (dnisNoSpecialChars)
					{
						dnisNoSpecialChars[0] = 0;
					}

					sprintf (lMsgToRes.dnis, "%s", dnis);
					sprintf (lMsgToRes.ani, "%s", ani);

					sprintf (gCall[zCall].dnis, "%s", dnis);
					sprintf (gCall[zCall].ani, "%s", ani);

					if (gWritePCForEveryCall)
					{
						writeCDR ("process_CALL_NEW", zCall, "PC", 20014, -1);	//DDN_TODO: SNMP SetRequest
					}

					/*
					 * Find out what codecs we need to send to Media Manager
					 */

					if (gCall[zCall].remoteRtpPort > 0 && remote_sdp != NULL)
					{
						yRc =
							sdp_message_to_str (remote_sdp,
												(char **) &yStrTmpRemoteSdp);

//printf("%s %d: PID: %d zCall:%d SDP Length < %d > Message (%s)\n", __FUNCTION__, __LINE__, getpid(), zCall, strlen(yStrTmpRemoteSdp), yStrTmpRemoteSdp);fflush(stdout);

						if ((yRc < 0) || (strlen (yStrTmpRemoteSdp) > 2047))		// BT-17
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
									   TEL_SIP_SIGNALING, ERR,
									   "Invalid SDP message length (%d); max value is 2048. "
									   "Rejecting the call.",
									   strlen (yStrTmpRemoteSdp));

							eXosip_lock (geXcontext);
							eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 400,
													 0);
							eXosip_unlock (geXcontext);

							osip_free (yStrTmpRemoteSdp);
							yStrTmpRemoteSdp = NULL;

							writeCDR ("process_CALL_NEW", zCall, "RC", 20014, -1);	//DDN_TODO: SNMP SetRequest

							continue;
						}

						snprintf (gCall[zCall].sdp_body_remote, 1023, "%s",
								  yStrTmpRemoteSdp);
						snprintf (gCall[zCall].sdp_body_remote_new, 1023,
								  "%s", gCall[zCall].sdp_body_remote);

						osip_free (yStrTmpRemoteSdp);
						yStrTmpRemoteSdp = NULL;

#if 1
						if (gCall[zCall].remote_sdp == NULL)
						{
							eXosip_lock (geXcontext);
							gCall[zCall].remote_sdp =
								eXosip_get_sdp_info (eXosipEvent->request);
							eXosip_unlock (geXcontext);
						}

						yRc =
							parseSdpMessage (zCall, __LINE__,
											 gCall[zCall].sdp_body_remote,
											 remote_sdp);
						if (yRc == -1)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_DIAGNOSE,
									   TEL_SIP_SIGNALING, WARN,
									   "parseSdpMessage() failed.  Setting codecMissMatch=YES");
							gCall[zCall].codecMissMatch = YES;

						}
						else
						{
							gCall[zCall].codecMissMatch = NO;
						}
#endif

						yRc =
							getCallInfoFromSdp (zCall,
												gCall[zCall].sdp_body_remote);
						if (yRc == -1)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
									   TEL_SIP_SIGNALING, WARN,
									   "Sending SIP Message 415 Unsupported Media Type.");

							eXosip_lock (geXcontext);
							eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 415,
													 0);
							eXosip_unlock (geXcontext);

							sprintf (gCall[zCall].GV_CustData1, "415");
							sprintf (gCall[zCall].GV_CustData2,
									 "AUDIO_CODEC=%s"
									 "&FRAME_RATE=%d",
									 gCall[zCall].audioCodecString,
									 -1);
							writeCDR ("process_CALL_NEW", zCall, "RC", 20014, -1);	//DDN_TODO: SNMP SetRequest

							continue;
						}

						if (yStrTmpRemoteSdp != NULL)
						{
							osip_free (yStrTmpRemoteSdp);
							yStrTmpRemoteSdp = NULL;
						}
					}

					yRc = writeToResp (mod, (void *) &lMsgToRes);
					if (yRc != 0)
					{
						if (!gWritePCForEveryCall)
						{
							writeCDR ("process_CALL_NEW", zCall, "PC", 20014, -1);	//DDN_TODO: SNMP SetRequest
						}

						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
								   TEL_BASE, WARN,
								   "Sending SIP Message 603 Declined.");

						eXosip_lock (geXcontext);
						eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 603, 0);
						eXosip_unlock (geXcontext);
					}

					if (remote_sdp != NULL)
					{
						sdp_message_free (remote_sdp);
						remote_sdp = NULL;
					}

					gCallsStarted++;
				}
				break;

			case EXOSIP_CALL_ANSWERED:
				{
					if (gCall[zCall].callState == CALL_STATE_CALL_RELEASED)     // BT-72
					{
						if ( (gCall[zCall].lastConnectedTime == -1) && ( gCall[zCall].lastReleaseTime ) )
						{
							gCall[zCall].lastConnectedTime = 0;
							break;
						}
					}

				   time(&gCall[zCall].lastConnectedTime);

                   if (gWritePCForEveryCall)
                   {
                        writeCDR ("process_CALL_NEW", zCall, "PC", 20014, -1);  // call offered and answered on outbound 
                   }

	               osip_uri_param_t *yToTag = NULL;
	               osip_uri_param_t *yFromTag = NULL;
	               char yStrTmpSdpPort[15];

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "eXosip event EXOSIP_CALL_ANSWERED.",
							   zCall);

					gCall[zCall].sipTo =
						osip_message_get_to (eXosipEvent->response);
					gCall[zCall].sipFrom =
						osip_message_get_from (eXosipEvent->response);

					arc_save_sip_from(zCall, gCall[zCall].sipFrom);	// BT-123
					arc_save_sip_to(zCall, gCall[zCall].sipTo);

					osip_to_get_tag (gCall[zCall].sipTo, &yToTag);
					osip_to_get_tag (gCall[zCall].sipFrom, &yFromTag);

					if (yToTag && yToTag->gvalue)
					{
						sprintf (gCall[zCall].tagToValue, "%s",
								 yToTag->gvalue);
					}

					if (yFromTag && yFromTag->gvalue)
					{
						sprintf (gCall[zCall].tagFromValue, "%s",
								 yFromTag->gvalue);
					}

					if (eXosipEvent->request &&
						eXosipEvent->request->call_id &&
						eXosipEvent->request->call_id->number)
					{
						if (eXosipEvent->request->call_id->host
							&& eXosipEvent->request->call_id->host[0])
						{
							sprintf (gCall[zCall].sipCallId, "%s@%s",
									 eXosipEvent->request->call_id->number,
									 eXosipEvent->request->call_id->host);
						}
						else
						{
							sprintf (gCall[zCall].sipCallId, "%s",
									 eXosipEvent->request->call_id->number);
						}
					}

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "eXosip event EXOSIP_CALL_ANSWERED tagToValue=%s, tagFromValue=%s, "
							   "sipCallId=%s", gCall[zCall].tagToValue,
							   gCall[zCall].tagFromValue,
							   gCall[zCall].sipCallId);

					/*DDN: Stop the Ringing */
					if (gCall[zCall].leg == LEG_B
						&& gCall[zCall].crossPort >= 0)
					{
	int             yALegPort = gCall[zCall].crossPort;

						if (gCall[yALegPort].remoteRtpPort > 0)
						{
	struct Msg_PortOperation yPortOperation;

	char            yStrRingingFile[256];

							sprintf (yStrRingingFile, "%s", "ringing.g729");

							if (access (yStrRingingFile, R_OK) == 0)
							{

								__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
											  "Stoping simulated Rings to A Leg",
											  yStrRingingFile, yALegPort);

								yPortOperation.opcode = DMOP_PORTOPERATION;
								yPortOperation.appPid =
									gCall[yALegPort].msgToDM.appPid;
								yPortOperation.appRef = 99999;
								yPortOperation.appPassword = yALegPort;
								yPortOperation.appCallNum = yALegPort;
								yPortOperation.operation = STOP_ACTIVITY;
								yPortOperation.callNum = yALegPort;

								yRc =
									notifyMediaMgr (__LINE__, yALegPort,
													(struct MsgToDM *)
													&yPortOperation, mod);

							}	/*END: if */

						}		/*END: if */

					}

					gCall[zCall].did = eXosipEvent->did;

#if 0
					eXosip_lock (geXcontext);
					eXosip_call_send_ack (geXcontext, gCall[zCall].did, NULL);
					eXosip_unlock (geXcontext);
#endif

	osip_message_t *send_ack = NULL;


	eXosip_lock (geXcontext);
	eXosip_call_build_ack (geXcontext, gCall[zCall].did, &send_ack);
	arc_fixup_contact_header (send_ack, gHostIp, gSipPort, zCall);
	//arc_fixup_sip_request_line (send_ack, "", 0, zCall);

		gViaHostForAck = (char *) malloc(64);
		sprintf(gViaHostForAck, "%s", gHostIp);

		osip_via_t *via = NULL;
		char		*p = NULL;
		char		*texto = NULL;

		yRc = osip_message_get_via(send_ack, 0, &via);

        if (via != NULL) // BT-72
        {
			osip_via_to_str(via, &texto);
			p = via_get_host(via);
			
			via_set_host(via, gViaHostForAck);
			
			yRc = osip_message_get_via(send_ack, 0, &via);
			osip_via_to_str(via, &texto);
			p = via_get_host(via);
			eXosip_call_send_ack (geXcontext, gCall[zCall].did, send_ack);
		}
	eXosip_unlock (geXcontext);

					setCallState (zCall, CALL_STATE_CALL_ANSWERED);
// MR 4655
					{
						struct Msg_InitiateCall msgCallIssued;
						memset((struct Msg_InitiateCall *)&msgCallIssued, '\0', sizeof(struct Msg_InitiateCall));
						msgCallIssued.opcode = DMOP_CALL_INITIATED;
						msgCallIssued.appCallNum = zCall;
						msgCallIssued.appPid = gCall[zCall].appPid;
						msgCallIssued.appRef = 99999;
						msgCallIssued.appPassword = zCall;
						msgCallIssued.informat = 0;   // Call is not in initiated state
						notifyMediaMgr (__LINE__, zCall,
									(struct MsgToDM *) &msgCallIssued, mod);
					}
// END: MR 4655

					eXosip_lock (geXcontext);
					remote_sdp_answered =
						eXosip_get_remote_sdp (geXcontext, gCall[zCall].did);
					eXosip_unlock (geXcontext);

					if (remote_sdp_answered == NULL)
					{

						__DDNDEBUG__ (DEBUG_MODULE_CALL,
									  "Failed to find valid SDP in 200 OK, dropping the call.",
									  "", zCall);

						dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
								   TEL_SIP_SIGNALING, ERR,
								   "CALL_ANSWERED: eXosip_get_remote_sdp() failed. "
								   "Unable to find valid SDP in 200 OK, dropping the call.",
								   zCall);

						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   TEL_BASE, INFO,
								   "CALL_ANSWERED: Sending SIP Message BYE.");

						eXosip_lock (geXcontext);
						eXosip_call_terminate (geXcontext, gCall[zCall].cid,
											   gCall[zCall].did);
						time (&(gCall[zCall].lastReleaseTime));
						eXosip_unlock (geXcontext);
						break;
					}

					eXosip_lock (geXcontext);
					audio_media =
						eXosip_get_audio_media (remote_sdp_answered);
					eXosip_unlock (geXcontext);

					if (audio_media == NULL
						&& gCall[zCall].faxData.isFaxCall == 0)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
								   DYN_BASE, WARN,
								   "CALL_ANSWERED: Sending SIP Message 415 Unsupported Media Type (No Audio details in SDP, and not a fax call).");

						eXosip_lock (geXcontext);
						eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 415, 0);
						eXosip_unlock (geXcontext);

						sprintf (gCall[zCall].GV_CustData1, "415");
						sprintf (gCall[zCall].GV_CustData2,
								 "AUDIO_CODEC=%s"
								 "&FRAME_RATE=%d",
								 gCall[zCall].audioCodecString,
								 -1);

						writeCDR ("process_CALL_NEW", zCall, "RC", 20014, -1);	//DDN_TODO: SNMP SetRequest

						//eXosip_call_terminate (geXcontext, eXosipEvent->cid, eXosipEvent->did);

						break;
					}

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE,
							   INFO,
							   "CALL_ANSWERED: gCall[%d].faxData.isFaxCall=%d "
							   "gCall[%d].sendFaxReinvite = %d", zCall,
							   gCall[zCall].faxData.isFaxCall, zCall,
							   gCall[zCall].sendFaxReinvite);

					if ((gCall[zCall].faxData.isFaxCall == 1) &&
						(gCall[zCall].sendFaxReinvite == 1))
					{

						audio_media =
							eXosip_get_media (remote_sdp_answered, "image");
	char            yRemoteConnectionInfo[128] = "\0";

						if (audio_media == NULL)
						{
							sprintf (yRemoteConnectionInfo, "%s",
									 "destIP=NONE&destFaxPort=0");

							dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
									   TEL_SIP_SIGNALING, WARN,
									   "CALL_ANSWERED: Unable to get image media.  Fax call rejected. "
									   "Notifying media mgr with (%s)",
									   yRemoteConnectionInfo);

							yStrTmpRemoteSdp = (char *) NULL;
							yRc =
								parseFaxSdpMsg (zCall, yRemoteConnectionInfo,
												yStrTmpRemoteSdp);
							break;
						}

						//DDN: MR4052:  3rd priority to sdp o=...IP4...<ip>.
						sprintf (yRemoteConnectionInfo,
								 "destIP=%s&destFaxPort=%d",
								 remote_sdp_answered->o_addr,
								 atoi (audio_media->m_port));

						//DDN: MR4052:  2nd priority to sdp c= IP4...<ip>.
						if (remote_sdp_answered
							&& remote_sdp_answered->c_connection)
						{
	sdp_connection_t *c_tmp_sdp_connection =
		remote_sdp_answered->c_connection;

							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   FAX_BASE, INFO,
									   "CALL_ANSWERED: Address retrieved from audio sdp details is (%s).",
									   c_tmp_sdp_connection->c_addr);

							sprintf (yRemoteConnectionInfo,
									 "destIP=%s&destFaxPort=%d",
									 c_tmp_sdp_connection->c_addr,
									 atoi (audio_media->m_port));
						}

						//DDN: MR4052:  1st priority to sdp c= IP4..<ip>..  of m=image
						if (&audio_media->c_connections != NULL)
						{
	sdp_connection_t *c_tmp_image_header =
		(sdp_connection_t *) osip_list_get (&audio_media->c_connections, 0);

							if (c_tmp_image_header != NULL)
							{
								dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, FAX_BASE, INFO,
										   "CALL_ANSWERED: Address retrieved from image sdp details is (%s).",
										   c_tmp_image_header->c_addr);

								sprintf (yRemoteConnectionInfo,
										 "destIP=%s&destFaxPort=%d",
										 c_tmp_image_header->c_addr,
										 atoi (audio_media->m_port));
							}
						}

						sprintf (yStrTmpSdpPort, "%d",
								 gCall[zCall].localSdpPort);

						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   FAX_BASE, INFO,
								   "CALL_ANSWERED: Found media type image, marking as fax call, "
								   "dest info=(%s)", yRemoteConnectionInfo);

						yRc =
							parseFaxSdpMsg (zCall, yRemoteConnectionInfo,
											yStrTmpRemoteSdp);

						yRc = createFaxSdpBody (zCall,
												yStrTmpSdpPort, 0, 0, 101);

#if 0
						//RG added the following
						if (yRc <= 0)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "FAX: Failed to create outbound fax sdp.");

						}
#endif
					}

					if (gCall[zCall].remoteRtpPort <= 0)
					{
						gCall[zCall].remoteRtpIp[0] = '\0';
						snprintf (gCall[zCall].remoteRtpIp,
								  sizeof (gCall[zCall].remoteRtpIp), "%s",
								  remote_sdp_answered->o_addr);

						gCall[zCall].remoteRtpPort = 0;

						gCall[zCall].remoteRtpPort =
							atoi (audio_media->m_port);

						//gCall[zCall].localSdpPort = LOCAL_STARTING_RTP_PORT + (zCall * 2);

						if (gCall[zCall].remoteRtpPort > 0)
						{
	struct MsgToDM  msgRtpDetails;

							memcpy (&msgRtpDetails, &(gCall[zCall].msgToDM),
									sizeof (struct MsgToDM));

							sdp_message_to_str (remote_sdp_answered,
												(char **) &yStrTmpSdp);

							sprintf (gCall[zCall].sdp_body_remote, "%s",
									 yStrTmpSdp);

							osip_free (yStrTmpSdp);
							yStrTmpSdp = NULL;

							yRc =
								parseSdpMessage (zCall, __LINE__,
												 gCall[zCall].sdp_body_remote,
												 remote_sdp_answered);

							if (yRc == -1)
							{
								dynVarLog (__LINE__, zCall, mod,
										   REPORT_DETAIL, TEL_BASE, WARN,
										   "Sending SIP Message 415 Unsupported Media Type.");
								eXosip_lock (geXcontext);
								eXosip_call_send_answer (geXcontext, eXosipEvent->tid,
														 415, 0);
								eXosip_unlock (geXcontext);

								sprintf (gCall[zCall].GV_CustData1, "415");
								sprintf (gCall[zCall].GV_CustData2,
										 "AUDIO_CODEC=%s"
										 "&FRAME_RATE=%d",
										 gCall[zCall].audioCodecString,
										 -1);
								writeCDR ("process_CALL_NEW", zCall, "RC", 20014, -1);	//DDN_TODO: SNMP SetRequest
								//eXosip_call_terminate (geXcontext, eXosipEvent->cid, eXosipEvent->did);

								break;
							}

							if (gCall[zCall].leg == LEG_B
								&& gCall[zCall].crossPort >= 0)
							{
	int             yALegPort = gCall[zCall].crossPort;

								gCall[zCall].codecType =
									gCall[yALegPort].codecType;

								//sprintf(gCall[zCall].sdp_body,  "%s", gCall[yALegPort].sdp_body);

								gCall[zCall].full_duplex =
									gCall[yALegPort].full_duplex;

								gCall[zCall].telephonePayloadType =
									gCall[yALegPort].telephonePayloadType;
								gCall[zCall].telephonePayloadType_2 =
									gCall[yALegPort].telephonePayloadType_2;
							}
						}
					}

					gCallsStarted++;

					break;
				}

			case EXOSIP_CALL_PROCEEDING:
				gCall[zCall].tid = eXosipEvent->tid;
				gCall[zCall].did = eXosipEvent->did;

				setCallState (zCall, CALL_STATE_CALL_RINGING);
				gCall[zCall].eXosipStatusCode =
					osip_message_get_status_code (eXosipEvent->response);

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "eXosip event EXOSIP_CALL_PROCEEDING (%d).",
						   gCall[zCall].eXosipStatusCode);

				switch (gCall[zCall].eXosipStatusCode)
				{
				case 181:
				case 182:
				case 183:
					sendPRACKReq (zCall, eXosipEvent);
					break;
				}

				sprintf (gCall[zCall].eXosipReasonPhrase, "%s",
						 (eXosipEvent->response) ? eXosipEvent->response->
						 reason_phrase : eXosipEvent->textinfo);

				if (gCall[zCall].leg == LEG_B && gCall[zCall].crossPort > -1)
				{
	int             yIntALegPort = gCall[zCall].crossPort;

					if (gCall[yIntALegPort].bridgeType == DIAL_OUT
						&& gCall[zCall].eXosipStatusCode == 181)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
								   TEL_SIP_SIGNALING, WARN,
								   "Setting call state to CALL_STATE_CALL_REQUESTFAILURE for 181 on "
								   "consultation transfer. BridgeCallThread should drop the call with 603.");
						setCallState (zCall, CALL_STATE_CALL_REQUESTFAILURE);
					}
				}

				break;

			case EXOSIP_CALL_RINGING:

				gCall[zCall].did = eXosipEvent->did;
				gCall[zCall].tid = eXosipEvent->tid;
				if (gPrackMethod != PRACK_DISABLED)
				{
					if (gCall[zCall].UAC_PRACKRingCounter == 0)
					{
						sendPRACKReq (zCall, eXosipEvent);	// Only send PRACK for the first Ringing
					}
					else
					{
						if (!gCall[zCall].UAC_PRACKOKReceived)	// PRACK not acknowledged before next ring
						{
							eXosip_lock (geXcontext);
							yRc = eXosip_call_send_answer (geXcontext, eXosipEvent->tid, 421, 0);	// Bad Extension
							eXosip_unlock (geXcontext);
						}
					}
					gCall[zCall].UAC_PRACKRingCounter++;
				}
				gCall[zCall].eXosipStatusCode =
					osip_message_get_status_code (eXosipEvent->response);

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "eXosip event EXOSIP_CALL_RINGING (%d).", gCall[zCall].eXosipStatusCode);
				setCallState (zCall, CALL_STATE_CALL_RINGING);

#if 0
				/*DDN: Simulate the RINGING */
				if (gCall[zCall].leg == LEG_B && gCall[zCall].crossPort >= 0)
				{
	int             yALegPort = gCall[zCall].crossPort;

	struct Msg_SetGlobal yMsgSetGlobal;

					yMsgSetGlobal.opcode = DMOP_SETGLOBAL;
					yMsgSetGlobal.appPid = gCall[yALegPort].msgToDM.appPid;
					yMsgSetGlobal.appRef = 99999;
					yMsgSetGlobal.appPassword = yALegPort;
					yMsgSetGlobal.appCallNum = yALegPort;

					sprintf (yMsgSetGlobal.name, "$INTERNAL_BRIDGE_RINGING");
					yMsgSetGlobal.value = 1;
					notifyMediaMgr (__LINE__, yALegPort,
									(struct MsgToDM *) &yMsgSetGlobal, mod);

					if (gCall[yALegPort].remoteRtpPort > 0)
					{
	struct Msg_Speak ySpeakRings;

	char            yStrRingingFile[256];
	char            yStrRingingFileByDest[256];

#if 0

						if (gCall[zCall].bridgeDestination[0])
						{
							if (strlen (gCall[zCall].bridgeDestination) > 6)
							{
								snprintf (yStrRingingFileByDest, 6,
										  gCall[zCall].bridgeDestination);
							}
							else
							{
								printf (yStrRingingFileByDest,
										gCall[zCall].bridgeDestination);
							}
						}
#endif

						switch (gCall[yALegPort].codecType)
						{
						case 18:
							sprintf (yStrRingingFile, "%s", "ringing.g729");
							strcat (yStrRingingFileByDest, ".g729");
							break;

						case 8:
							sprintf (yStrRingingFile, "%s", "ringing.pcma");
							strcat (yStrRingingFileByDest, ".pcma");
							break;

						case 0:
						default:
							sprintf (yStrRingingFile, "%s", "ringing.pcmu");
							strcat (yStrRingingFileByDest, ".pcmu");
							break;

						}		/*END: switch */

#if 0
						if (access (yStrRingingFile, F_OK) == 0)
						{
							sprintf (yStrRingingFile, "%s",
									 yStrRingingFileByDest);
						}
#endif

						if (access (yStrRingingFile, F_OK) == 0)
						{

							__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
										  "Simulating Rings to A Leg",
										  yStrRingingFile, yALegPort);

							ySpeakRings.opcode = DMOP_SPEAK;
							ySpeakRings.appPid =
								gCall[yALegPort].msgToDM.appPid;
							ySpeakRings.appRef = 99999;
							ySpeakRings.appPassword = yALegPort;
							ySpeakRings.appCallNum = yALegPort;
							ySpeakRings.list = 0;
							ySpeakRings.synchronous = ASYNC;
							ySpeakRings.allParties = FIRST_PARTY;
							ySpeakRings.interruptOption = NONINTERRUPT;

							sprintf (ySpeakRings.file, "%s", yStrRingingFile);
							sprintf (ySpeakRings.key, "%s", "\0");

							yRc =
								notifyMediaMgr (__LINE__, yALegPort,
												(struct MsgToDM *)
												&ySpeakRings, mod);

						}		/*END: if */

					}			/*END: if */

				}
				/*DDN: Simulate the RINGING */
#endif

				eXosip_lock (geXcontext);
				remote_sdp_answered =
					eXosip_get_remote_sdp (geXcontext, gCall[zCall].did);
				eXosip_unlock (geXcontext);

				if (remote_sdp_answered != NULL)
				{
					eXosip_lock (geXcontext);
					audio_media =
						eXosip_get_audio_media (remote_sdp_answered);
					eXosip_unlock (geXcontext);

					if (gCall[zCall].remoteRtpPort <= 0)
					{
						gCall[zCall].remoteRtpIp[0] = '\0';
						snprintf (gCall[zCall].remoteRtpIp,
								  sizeof (gCall[zCall].remoteRtpIp), "%s",
								  remote_sdp_answered->o_addr);

						gCall[zCall].remoteRtpPort = 0;

						gCall[zCall].remoteRtpPort =
							atoi (audio_media->m_port);

						//gCall[zCall].localSdpPort = LOCAL_STARTING_RTP_PORT + (zCall * 2);

						if (gCall[zCall].remoteRtpPort > 0)
						{
	struct MsgToDM  msgRtpDetails;

							memcpy (&msgRtpDetails, &(gCall[zCall].msgToDM),
									sizeof (struct MsgToDM));

							sdp_message_to_str (remote_sdp_answered,
												(char **) &yStrTmpSdp);

							sprintf (gCall[zCall].sdp_body_remote, "%s",
									 yStrTmpSdp);

							osip_free (yStrTmpSdp);
							yStrTmpSdp = NULL;

							yRc =
								parseSdpMessage (zCall, __LINE__,
												 gCall[zCall].sdp_body_remote,
												 remote_sdp_answered);
							if (yRc == -1)
							{
								dynVarLog (__LINE__, zCall, mod,
										   REPORT_DETAIL, TEL_BASE, WARN,
										   "Sending SIP Message 415 Unsupported Media Type.");

								eXosip_lock (geXcontext);
								eXosip_call_send_answer (geXcontext, eXosipEvent->tid,
														 415, 0);
								eXosip_unlock (geXcontext);

								sprintf (gCall[zCall].GV_CustData1, "415");
								sprintf (gCall[zCall].GV_CustData2,
										 "AUDIO_CODEC=%s"
										 "&FRAME_RATE=%d",
										 gCall[zCall].audioCodecString,
										 -1);
								writeCDR ("process_CALL_NEW", zCall, "RC", 20014, -1);	//DDN_TODO: SNMP SetRequest

								break;
							}

							if (gCall[zCall].leg == LEG_B
								&& gCall[zCall].crossPort >= 0)
							{
	int             yALegPort = gCall[zCall].crossPort;

								gCall[zCall].codecType =
									gCall[yALegPort].codecType;

								gCall[zCall].full_duplex =
									gCall[yALegPort].full_duplex;

								gCall[zCall].telephonePayloadType =
									gCall[yALegPort].telephonePayloadType;
								gCall[zCall].telephonePayloadType_2 =
									gCall[yALegPort].telephonePayloadType_2;
							}
						}
					}

				}				/*if(remote_sdp_answered != NULL) */

				break;

			case EXOSIP_CALL_CLOSED:
				{
					
// MR 4642/4605
					if ( gCall[zCall].alreadyClosed == 1 )
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
							   TEL_SIP_SIGNALING, WARN,
							   "Channel has already processed a EXOSIP_CALL_CLOSED.  Ignoring.");
						gCall[zCall].alreadyClosed = 0;
						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
							   "Received 2nd closed event - set alreadyClosed to %d", gCall[zCall].alreadyClosed);
						break;
					}
					gCall[zCall].alreadyClosed = 1;
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Set alreadyClosed to %d", gCall[zCall].alreadyClosed);
// END 4642/4605
					{		// BT-177
						char	fName[128];
						sprintf(fName, "/tmp/invite.headers.%d", zCall);
						if (access (fName, F_OK) == 0)
						{
							(void) unlink(fName);
						}
					}		

	int             sendDisconnect = 1;

					yIntSavedCallState = gCall[zCall].callState;

					// third leg bridge only code 
	int             third_leg, rc, crossport, parent_idx;

					if (gCall[zCall].thirdParty == 1)
					{

	struct MsgToApp response;

						crossport = gCall[zCall].crossPort;
						CROSSREF_INDEX_TO_ZCALL (parent_idx,
												 gCall[zCall].parent_idx,
												 gDynMgrId, SPAN_WIDTH);

						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   DYN_BASE, INFO,
								   "C-LEG: Call is bridged checking for third party bridge . zCall=%d crossPort=%d CallLeg=%d",
								   zCall, gCall[zCall].crossPort,
								   gCall[zCall].leg);

						rc = gCallFindThirdLeg (gCall, MAX_PORTS, zCall,
												gCall[zCall].crossPort);

						CROSSREF_INDEX_TO_ZCALL (third_leg, rc, gDynMgrId,
												 SPAN_WIDTH);

						dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
								   DYN_BASE, INFO,
								   "C-LEG: Third leg=%d rc=%d dynmgr=%d span_width=%d",
								   third_leg, rc, gDynMgrId, SPAN_WIDTH);

						if (third_leg >= 0 && crossport >= 0)
						{

							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   DYN_BASE, INFO,
									   "C-LEG: Sending switch for third leg call "
									   "callnum=%d appcallnum1=%d, appcallnum2=%d, appcallnum3=%d",
									   parent_idx, zCall, third_leg,
									   crossport);

							// in this case zCall can be any one of the legs at this point

							if (gCall[zCall].leg == LEG_A)
							{
								dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, DYN_BASE, INFO,
										   "C-LEG: a leg hanging up with b or c leg found");

								// disconnect other parties and set a leg port back to idle

								SendMediaManagerPortDisconnect (third_leg, 1);
								gCall[third_leg].crossPort = -1;
								gCall[third_leg].thirdParty = 0;
								setCallState (third_leg,
											  CALL_STATE_CALL_CLOSED);
								gCallDelCrossRef (gCall, MAX_PORTS, zCall,
												  third_leg);

								writeToRespShm (__LINE__, mod, tran_tabl_ptr, third_leg,
												APPL_API, 0);
								writeToRespShm (__LINE__, mod, tran_tabl_ptr, third_leg,
												APPL_STATUS, STATUS_IDLE);
								writeToRespShm (__LINE__, mod, tran_tabl_ptr, third_leg,
												APPL_PID, getpid ());
								updateAppName (__LINE__, third_leg, third_leg);	//DDN 03232010

								eXosip_lock (geXcontext);
								eXosip_call_terminate (geXcontext, gCall[third_leg].cid,
													   gCall[third_leg].did);
								time (&(gCall[third_leg].lastReleaseTime));
								eXosip_unlock (geXcontext);
								gCallsFinished++;

		if ( gCallsFinished > gCallsStarted )
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, WARN,
				   "DEBUG: Incremented gCallsFinished to %d, gCallStarted=%d  ",
					gCallsFinished, gCallsStarted);
		}

								// one has already gone to the app so only hang up the port 
								SendMediaManagerPortDisconnect (crossport, 1);
								gCall[crossport].crossPort = -1;
								gCall[crossport].thirdParty = 0;
								setCallState (crossport,
											  CALL_STATE_CALL_CLOSED);
								gCallDelCrossRef (gCall, MAX_PORTS, zCall,
												  crossport);

								writeToRespShm (__LINE__, mod, tran_tabl_ptr, crossport,
												APPL_API, 0);
								writeToRespShm (__LINE__, mod, tran_tabl_ptr, crossport,
												APPL_STATUS, STATUS_IDLE);
								writeToRespShm (__LINE__, mod, tran_tabl_ptr, crossport,
												APPL_PID, getpid ());
								updateAppName (__LINE__, crossport, crossport);	//DDN 03232010

								eXosip_lock (geXcontext);
								eXosip_call_terminate (geXcontext, gCall[crossport].cid,
													   gCall[crossport].did);
								time (&(gCall[crossport].lastReleaseTime));
								eXosip_unlock (geXcontext);
								gCallsFinished++;
		if ( gCallsFinished > gCallsStarted )
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, WARN,
				   "DEBUG: Incremented gCallsFinished to %d, gCallStarted=%d  ",
					gCallsFinished, gCallsStarted);
		}

								// a leg 

								//SendMediaManagerPortDisconnect(zCall, 1);
								SendMediaManagerPortDisconnect (zCall, 1);
								setCallState (zCall, CALL_STATE_CALL_CLOSED);
								gCall[zCall].thirdParty = 0;
								gCall[zCall].crossPort = -1;

								// schedule killing of the app
//	struct timeb    yTmpTimeb;
	struct timeval	yTmpTimeb;

								//ftime (&yTmpTimeb);
								gettimeofday(&yTmpTimeb, NULL);
#if 0
								addToTimerList (__LINE__, zCall, MP_OPCODE_KILL_APP,
												gCall[zCall].msgToDM,
												yTmpTimeb, -1, -1, -1);
#endif

							}
							else
							{

								dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, DYN_BASE, INFO,
										   "C-LEG: b or c  leg hanging up with a b or c leg found, switching ports");

								setCallState (zCall, CALL_STATE_CALL_CLOSED);
								// else one of the other legs 
								SendMediaMgrPortSwitch (parent_idx, zCall,
														third_leg, crossport,
														1);
								SendMediaManagerPortDisconnect (zCall, 1);
								gCall[crossport].crossPort = third_leg;
								gCall[third_leg].crossPort = crossport;
								gCallDelCrossRef (gCall, MAX_PORTS, zCall,
												  zCall);
								gCall[zCall].thirdParty = 0;
								gCallsFinished++;
		if ( gCallsFinished > gCallsStarted )
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, WARN,
				   "DEBUG: Incremented gCallsFinished to %d, gCallStarted=%d  ",
					gCallsFinished, gCallsStarted);
		}

								writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall,
												APPL_API, 0);
								writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall,
												APPL_STATUS, STATUS_IDLE);

								if (gCall[zCall].leg == LEG_B)
								{
									writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall,
													APPL_PID, getpid ());
									updateAppName (__LINE__, zCall, zCall);
								}

								eXosip_lock (geXcontext);
								eXosip_call_terminate (geXcontext, gCall[zCall].cid,
													   gCall[zCall].did);
								time (&(gCall[zCall].lastReleaseTime));
								eXosip_unlock (geXcontext);

								if (gSipSignaledDigits)
								{
									dropSipSignaledDigitsSubscription (zCall);
								}

							}
							break;
						}
						else
						{		// no third leg found  // maybe we won't need this seems like we can back out of thirdparty at this point
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   DYN_BASE, INFO,
									   "C-LEG: Third leg not found third-leg=%d crossport=%d  zCall=%d tearing down c-leg call !\n",
									   third_leg, crossport, zCall);

							if (gCall[zCall].leg == LEG_A)
							{
	int             crossPort;

								dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, DYN_BASE, INFO,
										   "C-LEG: a leg hanging up with no c leg found");

								gCall[zCall].canKillApp = 1;
								crossPort = gCall[zCall].crossPort;

								if (crossPort > -1 && crossPort <= MAX_PORTS)
								{

									gCallDelCrossRef (gCall, MAX_PORTS, zCall,
													  zCall);

									setCallState (crossPort,
												  CALL_STATE_CALL_CLOSED);
									gCall[crossPort].thirdParty = 0;
									gCallsFinished++;
		if ( gCallsFinished > gCallsStarted )
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, WARN,
				   "DEBUG: Incremented gCallsFinished to %d, gCallStarted=%d  ",
					gCallsFinished, gCallsStarted);
		}

									eXosip_lock (geXcontext);
									eXosip_call_terminate (geXcontext, gCall[crossPort].
														   cid,
														   gCall[crossPort].
														   did);
									time (&
										  (gCall[crossPort].lastReleaseTime));
									eXosip_unlock (geXcontext);

									// this should cause the app to exit from third leg
									SendMediaManagerPortDisconnect (crossPort,
																	1);

									gCall[zCall].crossPort = -1;
									if (gSipSignaledDigits)
									{
										dropSipSignaledDigitsSubscription
											(zCall);
									}
								}

								SendMediaManagerPortDisconnect (zCall, 1);
								setCallState (zCall, CALL_STATE_CALL_CLOSED);
								gCall[zCall].thirdParty = 0;

								// schedule killing of the app
	//struct timeb    yTmpTimeb;
							struct timeval	yTmpTimeb;

								//ftime (&yTmpTimeb);
								gettimeofday(&yTmpTimeb, NULL);
#if 0
								addToTimerList (__LINE__, zCall, MP_OPCODE_KILL_APP,
												gCall[zCall].msgToDM,
												yTmpTimeb, -1, -1, -1);
#endif

							}
							else
							{
								// bleg initiated hang up set crossport to idle and hang up zcall
	int             crossPort;

								dynVarLog (__LINE__, zCall, mod,
										   REPORT_VERBOSE, DYN_BASE, INFO,
										   "C-LEG: b or c leg hanging up with no third leg found");

								gCallDelCrossRef (gCall, MAX_PORTS, zCall,
												  zCall);
								crossPort = gCall[zCall].crossPort;

								if (crossPort > -1 && crossPort <= MAX_PORTS)
								{
									setCallState (crossPort, CALL_STATE_IDLE);
									gCall[crossPort].crossPort = -1;
									gCall[crossPort].thirdParty = 0;
								}

								gCallsFinished++;
#if 0
		if ( gCallsFinished > gCallsStarted )
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, WARN,
				   "DEBUG: Incremented gCallsFinished to %d, gCallStarted=%d  ",
					gCallsFinished, gCallsStarted);
		}
#endif

								setCallState (zCall, CALL_STATE_CALL_CLOSED);
								SendMediaManagerPortDisconnect (zCall, 1);

								gCall[crossPort].thirdParty = 0;
								eXosip_lock (geXcontext);
								eXosip_call_terminate (geXcontext, gCall[zCall].cid,
													   gCall[zCall].did);
								time (&(gCall[zCall].lastReleaseTime));
								eXosip_unlock (geXcontext);

								if (gSipSignaledDigits)
								{
									dropSipSignaledDigitsSubscription (zCall);
								}
							}
						}
						break;
					}			// end third party 

					// if enableed try and drop the signaled digits subscription
					if (gSipSignaledDigits)
					{
						dropSipSignaledDigitsSubscription (zCall);
					}

					if (zCall < 0)
					{
						continue;
					}

					removeFromTimerList (zCall, MP_OPCODE_END_CALL);
					setCallState (zCall, CALL_STATE_CALL_CLOSED);

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "eXosip event EXOSIP_CALL_CLOSED "
							   "callState=%d, callSubState=%d, CALL_STATE_CALL_BRIDGED=%d, "
							   "CALL_STATE_CALL_MEDIACONNECTED=%d.",
							   gCall[zCall].callState,
							   gCall[zCall].callSubState,
							   CALL_STATE_CALL_BRIDGED,
							   CALL_STATE_CALL_MEDIACONNECTED);

					gCallsFinished++;
#if 0
		if ( gCallsFinished > gCallsStarted )
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, WARN,
				   "DEBUG: Incremented gCallsFinished to %d, gCallStarted=%d  ",
					gCallsFinished, gCallsStarted);
		}
#endif

					yCrossPort = gCall[zCall].crossPort;
					if (gCall[zCall].isInBlastDial == 1)
					{
						createBlastDialResultFile (zCall, yBlastResultFile);
						gCall[zCall].isInBlastDial = 0;
						if (yCrossPort > -1)
						{
							gCall[yCrossPort].isInBlastDial = 0;
						}
					}
					else if (yCrossPort > -1 &&
							 gCall[yCrossPort].isInBlastDial == 1)
					{
						createBlastDialResultFile (yCrossPort,
												   yBlastResultFile);
						gCall[yCrossPort].isInBlastDial = 0;
						gCall[zCall].isInBlastDial = 0;
					}

#if 0
					eXosip_lock (geXcontext);
					eXosip_call_terminate (geXcontext, eXosipEvent->cid,
										   eXosipEvent->did);
					eXosip_unlock (geXcontext);
#endif
					time (&(gCall[zCall].lastReleaseTime));

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "ycrossPort=%d, gCall[zCall].leg=%d, yIntSavedCallState=%d, "
							   "callSubState=%d |%d|%d|%d|.",
							   gCall[zCall].crossPort, gCall[zCall].leg,
							   yIntSavedCallState, gCall[zCall].callSubState,
							   LEG_A, CALL_STATE_CALL_BRIDGED,
							   CALL_STATE_CALL_MEDIACONNECTED);

					if (yIntSavedCallState == CALL_STATE_CALL_BRIDGED ||
						gCall[zCall].callSubState ==
						CALL_STATE_CALL_MEDIACONNECTED)
					{
//	int             yBLegPort = gCall[zCall].crossPort;

						if (gCall[zCall].permanentlyReserved == 1)
						{

							__DDNDEBUG__ (DEBUG_MODULE_CALL,
										  "Changing the status to init", "",
										  zCall);

							dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Setting port num=%d to permanentlyReserved.",
									   zCall);

							writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall,
											APPL_API, 0);
							writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall,
											APPL_STATUS, STATUS_INIT);
						}
						else
						{

							__DDNDEBUG__ (DEBUG_MODULE_CALL,
										  "Changing the status to idle", "",
										  zCall);

							writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall,
											APPL_API, 0);
							writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall,
											APPL_STATUS, STATUS_IDLE);
							//writeToRespShm(mod, tran_tabl_ptr, zCall, APPL_SIGNAL, 1);
							if (gCall[zCall].leg == LEG_B)
							{
								writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall,
												APPL_PID, getpid ());
								updateAppName (__LINE__, zCall, zCall);
							}
						}

						if (yCrossPort >= 0 && gCall[zCall].leg == LEG_A &&
							(yIntSavedCallState == CALL_STATE_CALL_BRIDGED ||
							 gCall[zCall].callSubState ==
							 CALL_STATE_CALL_MEDIACONNECTED))
						{

							gDisconnectStruct.appCallNum = zCall;
							gDisconnectStruct.opcode = DMOP_DISCONNECT;
							gDisconnectStruct.appPid = gCall[zCall].appPid;
							gDisconnectStruct.appPassword =
								gCall[zCall].appPassword;

							yRc =
								notifyMediaMgr (__LINE__, zCall,
												(struct MsgToDM *)
												&gDisconnectStruct, mod);
							sendDisconnect = 0;

							setCallState (yCrossPort, CALL_STATE_CALL_CLOSED);
							gCall[yCrossPort].prevAppPid = -1;

							gDisconnectStruct.appCallNum = yCrossPort;
							gDisconnectStruct.opcode = DMOP_DISCONNECT;
							gDisconnectStruct.appPid = gCall[zCall].appPid;
							gDisconnectStruct.appPassword =
								gCall[zCall].appPassword;
							gDisconnectStruct.appRef =
								gCall[zCall].msgToDM.appRef;

							yRc =
								notifyMediaMgr (__LINE__, yCrossPort,
												(struct MsgToDM *)
												&gDisconnectStruct, mod);

						    writeToRespShm (__LINE__, mod, tran_tabl_ptr, yCrossPort,
												APPL_API, 0);
                           if(gShutDownFlag == 1){
							     writeToRespShm (__LINE__, mod, tran_tabl_ptr, yCrossPort, APPL_STATUS, STATUS_INIT);
                           } else {
									writeToRespShm (__LINE__, mod, tran_tabl_ptr, yCrossPort, APPL_STATUS, STATUS_IDLE);
                            }
							writeToRespShm (__LINE__, mod, tran_tabl_ptr, yCrossPort,
												APPL_SIGNAL, 1);
							writeToRespShm (__LINE__, mod, tran_tabl_ptr, yCrossPort,
												APPL_PID, getpid ());
							updateAppName (__LINE__, yCrossPort, yCrossPort);

							gDisconnectStruct.appCallNum = -1;
							gDisconnectStruct.appPid = -1;
							gDisconnectStruct.appPassword = -1;

							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE,
									   TEL_BASE, INFO,
									   "Sending SIP Message BYE.");

							eXosip_lock (geXcontext);
							eXosip_call_terminate (geXcontext, gCall[yCrossPort].cid,
												   gCall[yCrossPort].did);
							time (&(gCall[yCrossPort].lastReleaseTime));
							eXosip_unlock (geXcontext);

							setCallSubState (yCrossPort,
											 CALL_STATE_CALL_CLOSED);
							setCallSubState (zCall, CALL_STATE_CALL_CLOSED);
						}
					}
					else if (gCall[zCall].callState ==
							 CALL_STATE_CALL_TRANSFERCOMPLETE
							 || gCall[zCall].callState ==
							 CALL_STATE_CALL_TRANSFERFAILURE)
					{
						setCallState (zCall,
									  CALL_STATE_CALL_TRANSFERCOMPLETE_CLOSED);
						break;
					}

					setCallState (zCall, CALL_STATE_CALL_CLOSED);

					if (gCall[zCall].leg == LEG_B)
					{
						gCall[zCall].prevAppPid = -1;
						gCall[gCall[zCall].crossPort].crossPort = -1;		// MR 4605

						{	// BT-210
							struct MsgToRes yMsgToRes;

							memset (&yMsgToRes, 0, sizeof (yMsgToRes));

							yMsgToRes.opcode = RESOP_VXML_PID;
							yMsgToRes.appCallNum = zCall;
							yMsgToRes.appPid = -99;

							dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL, TEL_OS_SIGNALING, WARN, 
									"Sent RESOP_VXML_PID for bleg cleanup; APPL_SIGNAL=2.");
							writeToResp (mod, (void *) &yMsgToRes);
							writeToRespShm(__LINE__, mod, tran_tabl_ptr, zCall, APPL_SIGNAL, 2);
						}
					}
					else
					{
						gCall[zCall].prevAppPid = gCall[zCall].appPid;
					}

					if ( (gCall[zCall].permanentlyReserved == 1) || (gShutDownFlag == 1) )
					{
						__DDNDEBUG__ (DEBUG_MODULE_CALL,
									  "Changing the status to init", "",
									  zCall);
						if (gShutDownFlag == 1)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
								   "gShutDownFlag = 1; setting port num=%d to INIT status.", zCall);
						}
						else
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
								   "Setting port num=%d to permanentlyReserved", zCall);
						}

						writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
						writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS, STATUS_INIT);
						//writeToRespShm(mod, tran_tabl_ptr, zCall, APPL_SIGNAL, 1);
					}
					else
					{
						__DDNDEBUG__ (DEBUG_MODULE_CALL,
									  "Changing the status to idle", "",
									  zCall);

						writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API,
										0);
						writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall,
										APPL_STATUS, STATUS_IDLE);
						//writeToRespShm(mod, tran_tabl_ptr, zCall, APPL_SIGNAL, 1);
					}

					if (gCall[zCall].permanentlyReserved == 1)
					{
	//struct timeb    yTmpTimeb;
						struct timeval	yTmpTimeb;

					//	ftime (&yTmpTimeb);
//fprintf(stderr, " %s() adding static app gone for zcall=%d\n", __func__, zCall);
						yRc =
							addToTimerList (__LINE__, zCall, MP_OPCODE_STATIC_APP_GONE,
											gCall[zCall].msgToDM, yTmpTimeb,
											-1, -1, -1);

						if (yRc < 0)
						{
							;
						}
					}
					else if (gCall[zCall].leg == LEG_A
							 && gCall[zCall].canKillApp == YES)
					{

						__DDNDEBUG__ (DEBUG_MODULE_CALL,
									  "Registering to kill", "",
									  gCall[zCall].msgToDM.appPid);

//	struct timeb    yTmpTimeb;
						struct timeval	yTmpTimeb;

					//	ftime (&yTmpTimeb);
						gettimeofday(&yTmpTimeb, NULL);

						yTmpTimeb.tv_sec += gCall[zCall].GV_KillAppGracePeriod;

						yRc = addToTimerList (__LINE__, zCall,
											  MP_OPCODE_TERMINATE_APP,
											  gCall[zCall].msgToDM,
											  yTmpTimeb, -1, -1, -1);

						if (yRc >= 0)
						{
							yTmpTimeb.tv_sec += 3;
#if 0
							addToTimerList (__LINE__, zCall, MP_OPCODE_KILL_APP,
											gCall[zCall].msgToDM, yTmpTimeb,
											-1, -1, -1);
#endif
						}

						__DDNDEBUG__ (DEBUG_MODULE_MEMORY, "addToTimerList",
									  "returned ", yRc);
						if (yRc < 0)
						{
							;
						}
					}

					if (sendDisconnect == 1)
					{
                        dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
                           "[cs=%d]", gCall[zCall].callState);

						gDisconnectStruct.appCallNum = zCall;
						gDisconnectStruct.opcode = DMOP_DISCONNECT;
						gDisconnectStruct.appPid = gCall[zCall].appPid;
						gDisconnectStruct.appPassword =
							gCall[zCall].appPassword;
						gDisconnectStruct.appRef =
							gCall[zCall].msgToDM.appRef;

						if ( gCall[zCall].issuedBlindTransfer )				// MR 4802
                        {
                            sprintf(gDisconnectStruct.data, "%d", CALL_STATE_CALL_TRANSFER_MESSAGE_ACCEPTED);
                            dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
                                "Blind Transfer was done.  gDisconnectStruct.data set to (%s)", gDisconnectStruct.data);
                        }

						yRc =
							notifyMediaMgr (__LINE__, zCall,
											(struct MsgToDM *)
											&gDisconnectStruct, mod);
					}
					gDisconnectStruct.appCallNum = -1;
					gDisconnectStruct.appPid = -1;
					gDisconnectStruct.appPassword = -1;
					gDisconnectStruct.data[0] = '\0';           // MR 4802
				}
				break;

#if 0
			case EXOSIP_CALL_BYE_ACK:

				if (zCall < 0)
				{
					continue;
				}

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "eXosip event EXOSIP_CALL_BYE_ACK.", zCall);

				setCallState (zCall, CALL_STATE_CALL_RELEASED);

				if (gCall[zCall].permanentlyReserved == 1)
				{

					__DDNDEBUG__ (DEBUG_MODULE_CALL,
								  "Changing the status to init", "", zCall);
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "Setting port num=%d to permanentlyReserved",
							   zCall);

					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS,
									STATUS_INIT);
					//writeToRespShm(mod, tran_tabl_ptr, zCall, APPL_SIGNAL, 1);
				}
				else
				{

					__DDNDEBUG__ (DEBUG_MODULE_CALL, "Changing the status to idle", "", zCall);
// added joes Thu Mar 26 13:18:49 EDT 2015

					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS,
									STATUS_IDLE);
					//writeToRespShm(mod, tran_tabl_ptr, zCall, APPL_SIGNAL, 1);

					if (gCall[zCall].leg == LEG_B)
					{
						writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_PID,
										getpid ());
						updateAppName (__LINE__, zCall, zCall);
					}

				}

				break;
#endif
			case EXOSIP_CALL_RELEASED:

				/*
				 * Note: Dont bother to cleanup anything on this event, we are doing that on CALL_BYE_ACK
				 */

                if ( gCall[zCall].callState != CALL_STATE_CALL_NEW) // BT-204
				{
					setCallState (zCall, CALL_STATE_CALL_RELEASED);
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Ignoring EXOSIP_CALL_RELEASED; already processing OCS call.");
					break;
				}

				if (gCall[zCall].permanentlyReserved == 1 || gShutDownFlag == 1)
				{

					__DDNDEBUG__ (DEBUG_MODULE_CALL, "Changing the status to init", "", zCall);

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
							   "Setting port num=%d to permanentlyReserved",
							   zCall);

                   if(gShutDownFlag == 1){
		                dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						"Setting port num=%d to STATUS_INIT due to shutdown flag",
						zCall);
                   }

					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS, STATUS_INIT);
					//writeToRespShm(mod, tran_tabl_ptr, zCall, APPL_SIGNAL, 1);
				}
				else
				{

					__DDNDEBUG__ (DEBUG_MODULE_CALL,
								  "Changing the status to idle", "", zCall);

					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_API, 0);
					writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_STATUS,
									STATUS_IDLE);
					//writeToRespShm(mod, tran_tabl_ptr, zCall, APPL_SIGNAL, 1);
					if (gCall[zCall].leg == LEG_B)
					{
						writeToRespShm (__LINE__, mod, tran_tabl_ptr, zCall, APPL_PID,
										getpid ());
						updateAppName (__LINE__, zCall, zCall);
					}
				}

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "eXosip event EXOSIP_CALL_RELEASED.", zCall);

				//gCallsFinished++;

				if (gShutDownFromRedirector == 1 && gSipRedirection)
				{
#if 0
					if (gCallsStarted <= gCallsFinished)
					{

						dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
								   TEL_CALLMGR_SHUTDOWN, INFO,
								   "Restarting ArcIPDynMgr (%d)."
								   " gShutDownFromRedirector = 1, gCallsStarted = %d gCallsFinished = %d.",
								   gDynMgrId, gCallsStarted, gCallsFinished);

//						gCanReadSipEvents = 0;
//						sleep (10);
//						gCanReadRequestFifo = 0;
					}
#endif

					__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calls started", "",
								  gCallsStarted);
					__DDNDEBUG__ (DEBUG_MODULE_CALL, "Calls finished", "",
								  gCallsFinished);

					yRc = canDynMgrExit ();

					// if ( (yRc == -99)  && (gCallsStarted <= gCallsFinished) && (gShutDownFlag == 1) )
					if ( (yRc == -99)  && (gShutDownFlag == 1) )
					{
                        doDynMgrExit(zCall, __func__, __LINE__);
					}
				}
				else if (!gSipRedirection && gCallMgrRecycleBase > 0)
				{
					if ((gCallsStarted >
						(gCallMgrRecycleBase +
						 gCallMgrRecycleDelta * gDynMgrId)) && arc_can_recycle(RECYCLEPIDFILE, gDynMgrId))
					{
	char            yStrPortStatusFile[256];

						sprintf (yStrPortStatusFile, ".portStatus.%d",
								 gDynMgrId);
						unlink (yStrPortStatusFile);

						if (gShutDownFlag == 0)
						{
							dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
									   TEL_CALLMGR_SHUTDOWN, WARN,
									   "Removing (%s) ArcIPDynMgr (%d)."
									   "gShutDownFlag = 1, gCallsStarted = %d.",
									   yStrPortStatusFile, gDynMgrId,
									   gCallsStarted);

							if ( gShutDownFlagTime == 0 )
							{
								time (&gShutDownFlagTime);
							}
						}

                        arc_recycle_down_file(RECYCLEPIDFILE, gDynMgrId);
						gShutDownFlag = 1;
					}
					else
					{
						gShutDownFlag = 0;
					}

#if 0
					if (gCallsStarted <= gCallsFinished && gShutDownFlag == 1)
					{
						dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL,
								   TEL_CALLMGR_SHUTDOWN, INFO,
								   "Restarting ArcIPDynMgr (%d)."
								   " gShutDownFlag = 1, gCallsStarted = %d gCallsFinished = %d.",
								   gDynMgrId, gCallsStarted, gCallsFinished);

						gCanReadSipEvents = 0;
						sleep (10);
						gCanReadRequestFifo = 0;
					}
#endif

					yRc = canDynMgrExit ();

					// if ((yRc == -99)  && (gCallsStarted <= gCallsFinished) && (gShutDownFlag == 1))
					if ((yRc == -99)  && (gShutDownFlag == 1))
					{
                        doDynMgrExit(zCall, __func__, __LINE__);
					}
				}

				break;

			case EXOSIP_CALL_CANCELLED:

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "eXosip event EXOSIP_CALL_CANCELLED.",
						   zCall);
				if ( canContinue (mod, zCall))
				{
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						INFO, "BT-340 eXosip event EXOSIP_CALL_CANCELLED when the call was already established. Disconnecting.", zCall);
					
					SendMediaManagerPortDisconnect (zCall, 1);
				}


				setCallState (zCall, CALL_STATE_CALL_CANCELLED);

				break;

			case EXOSIP_CALL_SERVERFAILURE:
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "eXosip event EXOSIP_CALL_SERVERFAILURE.",
						   zCall);

				/*DDN: Stop the Ringing */
				if (gCall[zCall].leg == LEG_B && gCall[zCall].crossPort >= 0)
				{
	int             yALegPort = gCall[zCall].crossPort;

					if (gCall[yALegPort].remoteRtpPort > 0)
					{
	struct Msg_PortOperation yPortOperation;
	char            yStrRingingFile[256];

						sprintf (yStrRingingFile, "%s", "ringing.g729");
						if (access (yStrRingingFile, R_OK) == 0)
						{
							__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
										  "Stoping simulated Rings to A Leg",
										  yStrRingingFile, yALegPort);
							yPortOperation.opcode = DMOP_PORTOPERATION;
							yPortOperation.appPid =
								gCall[yALegPort].msgToDM.appPid;
							yPortOperation.appRef = 99999;
							yPortOperation.appPassword = yALegPort;
							yPortOperation.appCallNum = yALegPort;
							yPortOperation.operation = STOP_ACTIVITY;
							yPortOperation.callNum = yALegPort;

							yRc =
								notifyMediaMgr (__LINE__, yALegPort,
												(struct MsgToDM *)
												&yPortOperation, mod);
						}		/*END: if */
					}			/*END: if */
				}
				/*DDN: Stop the Ringing */

				//gCall[zCall].eXosipStatusCode = eXosipEvent->ss_status;
				gCall[zCall].eXosipStatusCode =
					osip_message_get_status_code (eXosipEvent->response);

				//sprintf (gCall[zCall].eXosipReasonPhrase, "%s", eXosipEvent->textinfo);
				sprintf (gCall[zCall].eXosipReasonPhrase, "%s",
						 (eXosipEvent->response) ? eXosipEvent->response->
						 reason_phrase : eXosipEvent->textinfo);

				setCallState (zCall, CALL_STATE_CALL_REQUESTFAILURE);
				break;

			case EXOSIP_CALL_GLOBALFAILURE:
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "eXosip event EXOSIP_CALL_GLOBALFAILURE.",
						   zCall);

				/*DDN: Stop the Ringing */
				if (gCall[zCall].leg == LEG_B && gCall[zCall].crossPort >= 0)
				{
	int             yALegPort = gCall[zCall].crossPort;

					if (gCall[yALegPort].remoteRtpPort > 0)
					{
	struct Msg_PortOperation yPortOperation;
	char            yStrRingingFile[256];

						sprintf (yStrRingingFile, "%s", "ringing.g729");
						if (access (yStrRingingFile, R_OK) == 0)
						{
							__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
										  "Stoping simulated Rings to A Leg",
										  yStrRingingFile, yALegPort);
							yPortOperation.opcode = DMOP_PORTOPERATION;
							yPortOperation.appPid =
								gCall[yALegPort].msgToDM.appPid;
							yPortOperation.appRef = 99999;
							yPortOperation.appPassword = yALegPort;
							yPortOperation.appCallNum = yALegPort;
							yPortOperation.operation = STOP_ACTIVITY;
							yPortOperation.callNum = yALegPort;

							yRc =
								notifyMediaMgr (__LINE__, yALegPort,
												(struct MsgToDM *)
												&yPortOperation, mod);
						}		/*END: if */
					}			/*END: if */
				}
				/*DDN: Stop the Ringing */

				//gCall[zCall].eXosipStatusCode = eXosipEvent->ss_status;
				gCall[zCall].eXosipStatusCode =
					osip_message_get_status_code (eXosipEvent->response);

				//sprintf (gCall[zCall].eXosipReasonPhrase, "%s", eXosipEvent->textinfo);
				sprintf (gCall[zCall].eXosipReasonPhrase, "%s",
						 (eXosipEvent->response) ? eXosipEvent->response->
						 reason_phrase : eXosipEvent->textinfo);

				setCallState (zCall, CALL_STATE_CALL_REQUESTFAILURE);
				break;

			case EXOSIP_CALL_REQUESTFAILURE:


				gCall[zCall].eXosipStatusCode = osip_message_get_status_code (eXosipEvent->response);


				if (gCall[zCall].eXosipStatusCode == 401 
				    || gCall[zCall].eXosipStatusCode == 407 
					)
				{

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "Adding authentication info (user:%s, pass:%s, id:%s, realm:%s)",
							   gAuthUser, gAuthPassword, gAuthId, gAuthRealm);

					eXosip_lock (geXcontext);
					//eXosip_add_authentication_information(gCall[zCall].currentInvite, eXosipEvent->response);
					eXosip_automatic_action (geXcontext);
					//eXosip_call_send_initial_invite (geXcontext, gCall[zCall].currentInvite);
					eXosip_unlock (geXcontext);
					break;
				}

             	//gCall[zCall].eXosipStatusCode = eXosipEvent->ss_status;
				/*DDN: Stop the Ringing */

				if (gCall[zCall].leg == LEG_B && gCall[zCall].crossPort >= 0)
				{
					int yALegPort = gCall[zCall].crossPort;

					if (gCall[yALegPort].remoteRtpPort > 0)
					{
					    struct Msg_PortOperation yPortOperation;
						char   yStrRingingFile[256];

						sprintf (yStrRingingFile, "%s", "ringing.g729");
						if (access (yStrRingingFile, R_OK) == 0)
						{
							__DDNDEBUG__ (DEBUG_MODULE_AUDIO,
										  "Stoping simulated Rings to A Leg",
										  yStrRingingFile, yALegPort);
							yPortOperation.opcode = DMOP_PORTOPERATION;
							yPortOperation.appPid =
								gCall[yALegPort].msgToDM.appPid;
							yPortOperation.appRef = 99999;
							yPortOperation.appPassword = yALegPort;
							yPortOperation.appCallNum = yALegPort;
							yPortOperation.operation = STOP_ACTIVITY;
							yPortOperation.callNum = yALegPort;

							yRc =
								notifyMediaMgr (__LINE__, yALegPort,
												(struct MsgToDM *)
												&yPortOperation, mod);
						}		/*END: if */
					}			/*END: if */
				}
				/*DDN: Stop the Ringing */

				//sprintf (gCall[zCall].eXosipReasonPhrase, "%s", eXosipEvent->textinfo);
				sprintf (gCall[zCall].eXosipReasonPhrase, "%s",
						 (eXosipEvent->response) ? eXosipEvent->response->
						 reason_phrase : eXosipEvent->textinfo);

				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO,
						   "eXosip event EXOSIP_CALL_REQUESTFAILURE (%d:%s)",
						   gCall[zCall].eXosipStatusCode,
						   gCall[zCall].eXosipReasonPhrase);

				if ((gCall[zCall].faxData.isFaxCall == 1) &&
					(gCall[zCall].sendFaxReinvite == 1))
				{
	char            yRemoteConnectionInfo[128];

					sprintf (yRemoteConnectionInfo, "%s",
							 "destIP=NONE&destFaxPort=0");
					dynVarLog (__LINE__, zCall, mod, REPORT_DETAIL,
							   TEL_SIP_SIGNALING, WARN,
							   "EXOSIP_CALL_REQUESTFAILURE(%d) recieved.  Fax call rejected. "
							   "Notifying media mgr with (%s)",
							   EXOSIP_CALL_REQUESTFAILURE,
							   yRemoteConnectionInfo);
					yStrTmpRemoteSdp = (char *) NULL;
					yRc = parseFaxSdpMsg (zCall,
										  yRemoteConnectionInfo,
										  yStrTmpRemoteSdp);
				}
				// break;

#if 0

				if (gCall[zCall].eXosipStatusCode == 407)
				{

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO,
							   "Adding authentication info (user:%s, pass:%s, id:%s, realm:%s)",
							   gAuthUser, gAuthPassword, gAuthId, gAuthRealm);

					eXosip_lock (geXcontext);
					//eXosip_add_authentication_information(gCall[zCall].currentInvite, eXosipEvent->response);
					eXosip_automatic_action (geXcontext);
					//eXosip_call_send_initial_invite (geXcontext, gCall[zCall].currentInvite);
					eXosip_unlock (geXcontext);
					break;
				}
#endif

				if (canContinue (mod, zCall))
				{
					setCallState (zCall, CALL_STATE_CALL_REQUESTFAILURE);
				}

				break;




			case EXOSIP_CALL_ACK:
				{
					osip_uri_param_t *yToTag = NULL;
					osip_uri_param_t *yFromTag = NULL;

					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
							   INFO, "eXosip event EXOSIP_CALL_ACK.", zCall);

					gCall[zCall].did = eXosipEvent->did;

					eXosip_lock (geXcontext);
					remote_sdp = eXosip_get_sdp_info (eXosipEvent->ack);
					eXosip_unlock (geXcontext);

					if (remote_sdp == NULL)
					{
						setCallState (zCall, CALL_STATE_CALL_ACK);
						break;
					}

					eXosip_lock (geXcontext);
					audio_media = eXosip_get_audio_media (remote_sdp);
					eXosip_unlock (geXcontext);

					if (audio_media == NULL)
					{
						setCallState (zCall, CALL_STATE_CALL_ACK);
						sdp_message_free (remote_sdp);
						remote_sdp = NULL;
						break;
					}

					gCall[zCall].sipTo =
						osip_message_get_to (eXosipEvent->response);
					gCall[zCall].sipFrom =
						osip_message_get_from (eXosipEvent->request);


					arc_save_sip_from(zCall, gCall[zCall].sipFrom);	// BT-123
					arc_save_sip_to(zCall, gCall[zCall].sipTo);

					osip_to_get_tag (gCall[zCall].sipTo, &yToTag);
					osip_to_get_tag (gCall[zCall].sipFrom, &yFromTag);

					if (yToTag && yToTag->gvalue)
					{
						sprintf (gCall[zCall].tagToValue, "%s",
								 yToTag->gvalue);
						__DDNDEBUG__ (DEBUG_MODULE_SIP, "Incoming tagToValue",
									  gCall[zCall].tagToValue, zCall);
					}

					if (yFromTag && yFromTag->gvalue)
					{
						sprintf (gCall[zCall].tagFromValue, "%s",
								 yFromTag->gvalue);
						__DDNDEBUG__ (DEBUG_MODULE_SIP,
									  "Incoming tagFromValue",
									  gCall[zCall].tagFromValue, zCall);
					}

					if (gCall[zCall].remoteRtpPort <= 0)
					{
						gCall[zCall].remoteRtpPort = 0;
						gCall[zCall].remoteRtpPort =
							atoi (audio_media->m_port);
						//gCall[zCall].localSdpPort = LOCAL_STARTING_RTP_PORT + (zCall * 2);

						if (gCall[zCall].remoteRtpPort > 0)
						{
							sdp_message_to_str (remote_sdp,
												(char **) &yStrTmpSdp);

							sprintf (gCall[zCall].sdp_body_remote, "%s",
									 yStrTmpSdp);

							osip_free (yStrTmpSdp);
							yStrTmpSdp = NULL;

							yRc =
								parseSdpMessage (zCall,	__LINE__,
												 gCall[zCall].sdp_body_remote,
												 remote_sdp);
							if (yRc == -1)
							{
								dynVarLog (__LINE__, zCall, mod,
										   REPORT_DETAIL, TEL_BASE, WARN,
										   "Sending SIP Message 415 Unsupported Media Type.");

								eXosip_lock (geXcontext);
								eXosip_call_send_answer (geXcontext, eXosipEvent->tid,
														 415, 0);
								eXosip_unlock (geXcontext);

								sprintf (gCall[zCall].GV_CustData1, "415");
								sprintf (gCall[zCall].GV_CustData2,
										 "AUDIO_CODEC=%s"
										 "&FRAME_RATE=%d",
										 gCall[zCall].audioCodecString,
										 -1);
								writeCDR ("process_CALL_NEW", zCall, "RC", 20014, -1);	//DDN_TODO: SNMP SetRequest
								//eXosip_call_terminate (geXcontext, eXosipEvent->cid, eXosipEvent->did);

								break;
							}

							if (gCall[zCall].leg == LEG_B
								&& gCall[zCall].crossPort >= 0)
							{
	int             yALegPort = gCall[zCall].crossPort;

								gCall[zCall].codecType =
									gCall[yALegPort].codecType;

								gCall[zCall].full_duplex =
									gCall[yALegPort].full_duplex;

								gCall[zCall].telephonePayloadType =
									gCall[yALegPort].telephonePayloadType;
								gCall[zCall].telephonePayloadType_2 =
									gCall[yALegPort].telephonePayloadType_2;
							}
						}

					}// if (gCall[zCall].remoteRtpPort <= 0)
					else if(	gCall[zCall].reinvite_tid == eXosipEvent->tid &&
								gCall[zCall].reinvite_empty_sdp == 1)
					{
						//DDN_TODO; Restart output thread here
						restartMMOutputThread(zCall, remote_sdp, audio_media);
					}

					if (remote_sdp != NULL)
					{
						sdp_message_free (remote_sdp);
						remote_sdp = NULL;
					}

					setCallState (zCall, CALL_STATE_CALL_ACK);


				}//CALL_ACK

				break;

#if NOT_EXOSIP_PORTED
			case EXOSIP_MESSAGE_PRACK:
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Received EXOSIP_MESSAGE_PRACK");
				gCall[zCall].UAS_PRACKReceived = 1;
				break;

			case EXOSIP_MESSAGE_PRACK_200OK:
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE,
						   INFO, "Received EXOSIP_MESSAGE_PRACK_200OK");
				gCall[zCall].UAC_PRACKOKReceived = 1;
				break;
#endif // NOT_EXOSIP_PORTED

			default:
				dynVarLog (__LINE__, zCall, mod, REPORT_DIAGNOSE,
						   TEL_UNKNOWN_EVENT, INFO,
						   "Default case statement hit for zCall index=%d, eXosip event=(%d).", zCall,
						   eXosipEvent->type);

				break;

			}					/*switch (eXosipEvent->type) */

			__DDNDEBUG__ (DEBUG_MODULE_CALL, "Waiting w/ eXosip_event_wait.",
						  "", 0);

			usleep (100);

		}						/*END: if (eXosipEvent != NULL) */
		else
		{
			usleep (1000);
		}

	}							/*END: while(gCanReadSipEvents) */

	// first unregister SIP registers 
	deRegistrationHandler (&RegistrationInfoList);

	/*
	 * App Cleanup
	 */

	/*
	 * 1: Request graceful exit of media manager
	 */
	yRc = notifyMediaMgr (__LINE__, 0, (struct MsgToDM *) &gExitAllStruct, mod);

	sleep (EXIT_SLEEP);

	if (arc_kill (gMediaMgrPid, 0) == -1)
	{
		if (errno == ESRCH)
		{
			gMediaMgrPid = -1;
		}
	}

	if (gMediaMgrPid != -1)
	{
		arc_kill (gMediaMgrPid, 15);
	}

	if (gAppReqReaderThreadId > 0)
	{
		__DDNDEBUG__ (DEBUG_MODULE_CALL,
					  "Sending SIGTERM to gAppReqReaderThread.", "",
					  gAppReqReaderThreadId);
		pthread_kill (gAppReqReaderThreadId, SIGTERM);
		//pthread_kill( gAppReqReaderThreadId, SIGTERM);
	}

#if 0
	while (1)
	{
		sleep (1);

		if (gAppReqReaderThreadId < 0)
		{
			break;
		}
		__DDNDEBUG__ (DEBUG_MODULE_CALL,
					  "Waiting for gAppReqReaderThread to exit.", "",
					  gAppReqReaderThreadId);

	}
#endif

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Unlinking files..", "", 0);

	unlinkFiles ();

	/*
	 * 2: Shm Cleanup
	 */
	shmdt (gShmBase);

	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Removing shared memory.", "", 0);

	removeSharedMem (-1);

// __DDNDEBUG__ (DEBUG_MODULE_ALWAYS, "Shutting down stack.", "", 0);

	//eXosip_lock(geXcontext);
	eXosip_quit (geXcontext);
	//eXosip_unlock(geXcontext);

	/*
	 * END: App cleanup
	 */


	__DDNDEBUG__ (DEBUG_MODULE_CALL, "Exiting main", "", 0);

	//muntrace();

    arc_dbg_close();

	exit (0);

}								/*END: int main */

int
buildPRACKAnswer (int zCall, osip_message_t ** lpMsg)
{
	static char     mod[] = "buildPRACKAnswer";
	int             yRc;
	osip_message_t *ringing = NULL;

	eXosip_lock (geXcontext);
	yRc = eXosip_call_build_answer (geXcontext, gCall[zCall].tid, 180, &ringing);
	eXosip_unlock (geXcontext);

	if (yRc < 0)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "eXosip_call_build_answer() failed for tid (%d). rc=%d. "
				   "Unable to build PRACK answer call.",
				   gCall[zCall].tid, yRc);
		return (-1);
	}

	insertPRACKHeaders (zCall, ringing);

	*lpMsg = ringing;

	return (0);
}								// END: buildPRACKAnswer()

int
insertPRACKHeaders (int zCall, osip_message_t * lpMsg)
{
	char            mod[] = "insertPRACKHeaders";

	char            yStrPrackRSeqCounter[10];

	dynVarLog (__LINE__, -1, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "PRACKSendRequire=%d lpmsg=%p",
			   gCall[zCall].PRACKSendRequire, lpMsg);

	if (!gCall[zCall].PRACKSendRequire)
	{
		return (0);
	}

	if (!lpMsg)
	{
		return (0);
	}

	sprintf (yStrPrackRSeqCounter, "%d", gCall[zCall].PRACKRSeqCounter++);

	osip_message_set_require (lpMsg, "100rel");
	osip_message_set_header (lpMsg, "RSeq", yStrPrackRSeqCounter);

	return (0);
}

/*---------------------------------------------------------------------------
int checkINVITEForPRACK(eXosip_event_t *eXosipEvent)
		switch ( gPrackMethod )
		{
			case PRACK_DISABLED:
				return(PRACK_REJECT_420);
			// MR3069:UAS - if INVITE has Require:100rel,
			//         	         send 420 Bad Extension
			//			    else
			//         	         process call w/out prack
			case PRACK_SUPPORTED:
			// MR3069:UAS - if INVITE has Require:100rel 
			//         	         process call with prack (Require:100rel)
			//              if INVITE has Supported;100rel && gInboundEarlyMedia
			//         	         process call w/out prack (Require:100rel) 
			//						(per movius request )
			//			    else
			//         	         process call w/out prack (Require:100rel)
			//		break;
			case PRACK_REQUIRE:
			// MR3069:UAS - if INVITE has Require:100rel or Supported;100rel
			//         	         process call with prack (Require:100rel)
			//			    else
			//         	         send 403 Forbidden
				break;
		}

	return codes:
		PRACK_REJECT_420
		PRACK_REJECT_403
		PRACK_NORMAL_CALL
		PRACK_SEND_REQUIRE
---------------------------------------------------------------------------*/
int
checkINVITEForPRACK (int zCall, eXosip_event_t * eXosipEvent)
{
	static char     mod[] = "checkINVITEForPRACK";
	int             yCurrentPos;
	char           *pStrPrackHdr;
	osip_header_t  *lpRequireHeader = NULL;
	osip_header_t  *lpSupportedHeader = NULL;

	int             yRequireRelFound = 0;
	int             ySupportedRelFound = 0;

	/*Search for 100rel in Require */
	yCurrentPos = 0;
	while (1)
	{
		lpRequireHeader = NULL;

		osip_message_header_get_byname (eXosipEvent->request,
										"Require", yCurrentPos++,
										&lpRequireHeader);
		if (lpRequireHeader == NULL)
		{
			break;
		}

		pStrPrackHdr = osip_header_get_value (lpRequireHeader);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PRACK Header \"Require\" position=%d value=%s.",
				   yCurrentPos, pStrPrackHdr);

		if (pStrPrackHdr && strstr (pStrPrackHdr, "100rel") != NULL)
		{
			yRequireRelFound = 1;
			break;
		}
	}
	/*END: Search for 100rel in Require */

	/*Search for 100rel in Supported */
	yCurrentPos = 0;
	while (1)
	{
		lpSupportedHeader = NULL;
		osip_message_header_get_byname (eXosipEvent->request,
										"Supported", yCurrentPos++,
										&lpSupportedHeader);

		if (lpSupportedHeader == NULL)
		{
			break;
		}

		pStrPrackHdr = osip_header_get_value (lpSupportedHeader);

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PRACK Header \"Supported\" position=%d value=%s.",
				   yCurrentPos, pStrPrackHdr);

		if (pStrPrackHdr && strstr (pStrPrackHdr, "100rel") != NULL)
		{
			ySupportedRelFound = 1;
			break;
		}
	}
	/*END: Search for 100rel in Supported */

	if (yRequireRelFound)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PRACK Header \"Require\" for 100rel is present.");

		switch (gPrackMethod)
		{
		case PRACK_DISABLED:
			return (PRACK_REJECT_420);
			break;
		case PRACK_SUPPORTED:
		case PRACK_REQUIRE:
			return (PRACK_SEND_REQUIRE);
			break;
		}
	}
	else if (ySupportedRelFound)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PRACK Header \"Supported\" for 100rel is present.");

		switch (gPrackMethod)
		{
		case PRACK_DISABLED:
			return (PRACK_NORMAL_CALL);
			break;
		case PRACK_SUPPORTED:
			if (gInboundEarlyMedia)
			{
				return (PRACK_SEND_REQUIRE);
			}
			else
			{
				return (PRACK_NORMAL_CALL);
			}
			break;
		case PRACK_REQUIRE:
			return (PRACK_SEND_REQUIRE);
			break;
		}
	}
	else
	{
		// no prack
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PRACK Header (Either Require of Supported) is not present.");

		switch (gPrackMethod)
		{
		case PRACK_DISABLED:
		case PRACK_SUPPORTED:
			return (PRACK_NORMAL_CALL);
			break;
		case PRACK_REQUIRE:
			return (PRACK_REJECT_403);
			break;
		}
	}

	return (PRACK_NORMAL_CALL);

}								// END: checkINVITEForPRACK()

/*---------------------------------------------------------------------------
---------------------------------------------------------------------------*/
int
sendPRACKReq (int zCall, eXosip_event_t * eXosipEvent)
{
	static char     mod[] = "sendPRACKReq";
	char           *pStrPrackHdr;
	int             yRc = -1;
	osip_header_t  *lpHeader = NULL;

	int             yCurrentPos =
		osip_message_header_get_byname (eXosipEvent->response,
										"Require", 0, &lpHeader);

	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			   "Current PRACK Method set to %d; PRACK_DISABLED=%d, "
			   "PRACK_SUPPORTED=%d, PRACK_REQUIRE=%d.", gPrackMethod,
			   PRACK_DISABLED, PRACK_SUPPORTED, PRACK_REQUIRE);

	pStrPrackHdr = osip_header_get_value (lpHeader);

	if (!lpHeader)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PRACK Header \"Require\" not present. No action taken.");
		return (0);
	}

	if (pStrPrackHdr)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PRACK Require header: (%s). ", pStrPrackHdr);
	}
	else
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PRACK Header \"Require\" not present. No action taken.");

		return (0);
	}

	if ((gPrackMethod == PRACK_REQUIRE || gPrackMethod == PRACK_SUPPORTED)
		&& (strcmp (pStrPrackHdr, "100rel") == 0))
	{
	osip_message_t *prack = NULL;

		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
				   "PRACK enabled  (%d). Attempting to build PRACK response "
				   "with tid (%d).", gPrackMethod, gCall[zCall].tid);

		eXosip_lock (geXcontext);
		if ((yRc = eXosip_call_build_prack (geXcontext, eXosipEvent->tid, &prack)) != 0)
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE, ERR,
					   "Failed to build PRACK response. rc=%d"
					   "Unable to send PRACK response.", yRc);
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					   "Succesfully built PRACK response.  "
					   "Sending response with tid (%d).", eXosipEvent->tid);
			eXosip_call_send_prack (geXcontext, eXosipEvent->tid, prack);
		}
		eXosip_unlock (geXcontext);

	}

	return (0);

}								// END: sendPRACKReq()

int
readPlayMediaFile (struct Msg_PlayMedia *zpMediaMessage,
				   struct MsgToDM *zpMsgToDM, int zCall)
{
	char            mod[] = "readPlayMediaFile";
	char            yStrTempFileName[128];
	char            line[256];
	FILE           *fp;

	char            lhs[256];
	char            rhs[256];

	sprintf (yStrTempFileName, "%s", zpMsgToDM->data);

	if ((fp = fopen (yStrTempFileName, "r")) == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR,
				   ERR,
				   "fopen(%s) failed. [%d, %s] " "Unable to read media file.",
				   yStrTempFileName, errno, strerror (errno));

		return (-1);
	}

	while (fgets (line, sizeof (line) - 1, fp))
	{
		lhs[0] = '\0';
		rhs[0] = '\0';

		/*  Strip \n from the line if it exists */

		if (line[(int) strlen (line) - 1] == '\n')
		{
			line[(int) strlen (line) - 1] = '\0';
		}

		trim_whitespace (line);

		sscanf (line, "%[^=]=%s", lhs, rhs);
		if ((lhs != NULL) && (rhs != NULL) && (lhs[0] != 0) && (rhs[0] != 0))
		{
			if (strcmp (lhs, "party") == 0)
			{
				zpMediaMessage->party = atoi (rhs);
			}
			else if (strcmp (lhs, "interruptOption") == 0)
			{
				zpMediaMessage->interruptOption = atoi (rhs);
			}
			else if (strcmp (lhs, "audioFileName") == 0)
			{
				sprintf (zpMediaMessage->audioFileName, "%s", rhs);
			}
			else if (strcmp (lhs, "sync") == 0)
			{
				zpMediaMessage->sync = atoi (rhs);
			}
			else if (strcmp (lhs, "audioinformat") == 0)
			{
				zpMediaMessage->audioinformat = atoi (rhs);
			}
			else if (strcmp (lhs, "audiooutformat") == 0)
			{
				zpMediaMessage->audiooutformat = atoi (rhs);
			}
			else if (strcmp (lhs, "audioLooping") == 0)
			{
				zpMediaMessage->audioLooping = atoi (rhs);
			}
			else if (strcmp (lhs, "addOnCurrentPlay") == 0)
			{
				zpMediaMessage->addOnCurrentPlay = atoi (rhs);
			}
		}
	}

	fclose (fp);

	unlink (yStrTempFileName);

	return (0);

}								/*END: int readPlayMediaFile */

int
getCallInfoFromSdp (int zCall, char *zStrMessage)
{
	char            mod[] = "getCallInfoFromSdp";
	char            temp_sdp[4000];
	int             grabIt = 0;
	char            tmp[64];
	char            yStrLhs[256];
	char            yStrRhs[256];

	char           *yStrIndex, *yStrTmp, *yStrPayload;
	char           *yStrTok;

	if (!zStrMessage)
	{
		return (-1);
	}

	sprintf (temp_sdp, "%s", zStrMessage);

	yStrIndex = strtok_r (temp_sdp, "\n\r", (char **) &yStrTok);

	while (yStrIndex != NULL)
	{
		memset (yStrLhs, 0, sizeof (yStrLhs));
		memset (yStrRhs, 0, sizeof (yStrRhs));

		yStrTmp = yStrIndex;

		snprintf (yStrRhs, 255, "%s", strstr (yStrTmp, "=") + 1);

		if (strstr (yStrIndex, "h263") != NULL
			|| strstr (yStrIndex, "H263") != NULL)
		{
			break;
		}
		else if (strstr (yStrIndex, "h261") != NULL)
		{
			break;
		}

		yStrIndex = strtok_r (NULL, "\n", (char **) &yStrTok);
	}
return(0);

}

#define MAX_APPL_NAME 50

int
updateAppName (int line, int shm_index, int port)
{
	char            mod[] = { "updateAppName" };
	char           *ptr, *ptr1;
	char            status_str[5];

	ptr = (char *)tran_tabl_ptr;
	ptr += (shm_index * SHMEM_REC_LENGTH);
	/* position the pointer to the field */
	ptr += vec[APPL_NAME - 1];	/* application start index */
	ptr1 = ptr;
	(void) memset (ptr1, ' ', MAX_APPL_NAME);
	ptr += (MAX_APPL_NAME - strlen (gProgramName));
	(void) memcpy (ptr, gCall[port].logicalName,
				   strlen (gCall[port].logicalName));

	return (0);

}								/*END: int updateAppName */

int
gCallFindThirdLeg (Call * gCall, int size, int myleg, int crossport)
{

	int             rc = -1;
	int             i;
	int             idx, xport, parent;

	if (myleg < 0 || crossport < 0)
	{
		return -1;
		//fprintf(stderr, " %s() invalid leg or crossport leg=%d crossport=%d\n", __func__, myleg, crossport);
	}

	parent = gCall[myleg].parent_idx;
	idx = myleg % SPAN_WIDTH;
	xport = crossport % SPAN_WIDTH;

	gCallDumpCrossRefs (gCall, parent);

	for (i = 0; i < SPAN_WIDTH; i++)
	{
		// is set
		if (gCall[parent].crossRef[i] != NULL && i != idx && i != xport)
		{
			rc = i;
			//fprintf(stderr, " %s() found third leg [%d] for leg [%d] and crossport [%d]\n", __func__, i, myleg, crossport);
			break;
		}
	}

	return rc;
}

/*-----------------------------------------------------------------------------
This routine returns the value of the desired field from the shared memory.
-----------------------------------------------------------------------------*/
static int readFromRespShm(char *mod, void *zpBasePtr, int rec, int ind, char *val)
{
    char    *ptr = (char *)zpBasePtr;

    if (rec < 0 || ind < 0)
    {
        dynVarLog( __LINE__, rec, mod, REPORT_NORMAL, 3197, ERR,
            "readFromRespShm(base_ptr, %d, %d, output): Invalid value/not "
            "supported.", rec, ind);
        return(-1);
    }

    switch(ind)
    {
        case    APPL_START_TIME:    /* application start time */
        case    APPL_RESOURCE:      /* application # */
        case    APPL_API:       /* current api running */
        case    APPL_FIELD5:        /* reserve name */
        case    APPL_NAME:      /* application name */
        case    APPL_STATUS:        /* status of the application */
        case    APPL_PID:       /* proc_id of the application */ 
        case    APPL_SIGNAL:        /* signal to the application */
            ptr += (rec*SHMEM_REC_LENGTH);
            ptr += vec[ind-1];  /* position the pointer to the field */
            sscanf(ptr, "%s", val); /* read the field */
            break;
        
        default :
            dynVarLog( __LINE__, rec, mod, REPORT_NORMAL, 3198, ERR,  
                "readFromRespShm(base_ptr, %d, %d, output): index not "
                "supported.", rec, ind);
            break;
    }
//    dynVarLog( __LINE__, rec, mod, REPORT_VERBOSE, 3198, ERR,  
//		"SHM: read val(%s) from shared memory.", val);
    
    return(0);

}/*END: int readFromRespShm*/

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static int getParmFromFile(int zCall, char *zLabel, char *zFrom, char *zTo)
{
	static char		mod[] = "getParmFromFile";
	FILE           *fp;
	char            yStrFileName[256];
	char			tempVal[512]="";
	
	sscanf (zFrom, "FILE:%s", yStrFileName);
	
	if ((fp = fopen (yStrFileName, "r")) == NULL)
	{
		dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_FILE_IO_ERROR, ERR,
			"Failed to open file (%s), [%d, %s] Unable to set %s.",
			yStrFileName, errno, strerror (errno), zLabel);
		return(-1);
	}
	else
	{
		struct stat     lStat;
		memset((char *)tempVal, '\0', sizeof(tempVal));
		
		stat (yStrFileName, &lStat);
		fread (tempVal, lStat.st_size, 1, fp);
		fclose (fp);
		arc_unlink (zCall, mod, yStrFileName);

		sprintf(zTo, "%s", tempVal);
	}
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"Successfully read (%s) from (%s) for %s.", zTo, zFrom, zLabel);

	return(0);
} // END: getParmFromFile()

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static int		doSessionExpiry(int zCall, int zLine)
{
	static char		mod[] = "doSessionExpiry";
	int				rc;
	struct timeval	yTmpTimeb;
	//struct timeb    yTmpTimeb;
	time_t          ySaveTime;

	string			pRefresher;
    struct tm       tempTM;
    struct tm       *pTM;
	char			str[21];


	rc = gCallInfo[zCall].GetIntOption("session-expires");
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
		"Called from %d.  Retrieved session-expires time of %d.",  zLine, rc);

	if ( rc > 0 )
	{
		pRefresher = gCallInfo[zCall].GetStringOption("refresher");     //  none, uac, or uas

		if ( gCall[zCall].cid <= 0 )
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_NORMAL, TEL_BASE, ERR,
				"Unable to set refresh and termination timers because the cid is not valid.");
			return(-1);
		}
	
		dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "Call state is %d.", gCall[zCall].callState);
		if ( strcmp( pRefresher.c_str(), "uas") == 0 )
		{
			gettimeofday(&yTmpTimeb, NULL);
			//ftime (&yTmpTimeb);
			ySaveTime = yTmpTimeb.tv_sec;
			yTmpTimeb.tv_sec += rc;

		    (void *)localtime_r(&yTmpTimeb.tv_sec, &tempTM);
		    pTM = &tempTM;
		    (void)strftime(str, 32, "%H:%M:%S", pTM);

			if ( modifyExpiresTimerList (zCall, MP_OPCODE_SESSION_EXPIRES_TERMINATE_APP, yTmpTimeb) == -1 )
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					"Session to expire at %s (%d seconds). "
					"Adding to timer list [ MP_OPCODE_SESSION_EXPIRES_TERMINATE_APP, cid=%d  ]. %d",
					str, rc,  gCall[zCall].cid, yTmpTimeb.tv_sec);
				(void) addToTimerList (__LINE__, zCall, MP_OPCODE_SESSION_EXPIRES_TERMINATE_APP,
						  gCall[zCall].msgToDM, yTmpTimeb, -1, -1, -1);
			}
	
			switch (gCall[zCall].callState)		//  Are we a UAS? If so, set up a refresh
			{
				case CALL_STATE_CALL_NEW:
				case CALL_STATE_CALL_ACK:
				case CALL_STATE_CALL_MODIFIED:
					rc = gCallInfo[zCall].GetIntOption("refresh-time");
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						"Retrieved refresh-time time of %d.  Adding to timer list.", rc);
		
					yTmpTimeb.tv_sec = ySaveTime  +  rc;
				    (void *)localtime_r(&yTmpTimeb.tv_sec, &tempTM);
				    pTM = &tempTM;
				    (void)strftime(str, 32, "%H:%M:%S", pTM);
		
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						"Next refresh to be sent at %s (%d seconds).  "
						"Adding to timer list [ MP_OPCODE_SESSION_EXPIRES_SEND_REFRESH, cid=%d ]. %d",
							str, rc, gCall[zCall].cid, yTmpTimeb.tv_sec);
		
					(void) addToTimerList (__LINE__, zCall,
						  MP_OPCODE_SESSION_EXPIRES_SEND_REFRESH,
						  gCall[zCall].msgToDM, yTmpTimeb, gCall[zCall].appPid, -1, -1);		// MR 5022 - session timers
				break;
			}
		}
		else if ( strcmp( pRefresher.c_str(), "uac") == 0 )
		{
			//ftime (&yTmpTimeb);
			gettimeofday(&yTmpTimeb, NULL);
			ySaveTime = yTmpTimeb.tv_sec;
			yTmpTimeb.tv_sec += rc;

		    (void *)localtime_r(&yTmpTimeb.tv_sec, &tempTM);
		    pTM = &tempTM;
		    (void)strftime(str, 32, "%H:%M:%S", pTM);

			if ( modifyExpiresTimerList (zCall, MP_OPCODE_SESSION_EXPIRES_TERMINATE_APP, yTmpTimeb) == -1 )
			{
				dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
					"Session to expire at %s (%d seconds). "
					"Adding to timer list [ MP_OPCODE_SESSION_EXPIRES_TERMINATE_APP, cid=%d  ]. %d",
					str, rc,  gCall[zCall].cid, yTmpTimeb.tv_sec);
				(void) addToTimerList (__LINE__, zCall, MP_OPCODE_SESSION_EXPIRES_TERMINATE_APP,
						  gCall[zCall].msgToDM, yTmpTimeb, -1, -1, -1);
			}
	
			switch (gCall[zCall].callState)		//  Are we a UAC? If so, set up the refresh.
			{
				case CALL_STATE_CALL_PROCEEDING:
				case CALL_STATE_CALL_RINGING:
				case CALL_STATE_CALL_REDIRECTED:
				case CALL_STATE_CALL_ANSWERED:
					rc = gCallInfo[zCall].GetIntOption("refresh-time");
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						"Retrieved refresh-time time of %d.  Adding to timer list.", rc);
		
					yTmpTimeb.tv_sec = ySaveTime  +  rc;
				    (void *)localtime_r(&yTmpTimeb.tv_sec, &tempTM);
				    pTM = &tempTM;
				    (void)strftime(str, 32, "%H:%M:%S", pTM);
		
					dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
						"Next refresh to be sent at %s (%d seconds).  "
						"Adding to timer list [ MP_OPCODE_SESSION_EXPIRES_SEND_REFRESH, cid=%d ]. %d",
							str, rc, gCall[zCall].cid, yTmpTimeb.tv_sec);
		
					(void) addToTimerList (__LINE__, zCall,
						  MP_OPCODE_SESSION_EXPIRES_SEND_REFRESH,
						  gCall[zCall].msgToDM, yTmpTimeb, gCall[zCall].appPid, -1, -1);		// MR 5022 - session timers
					break;
			}
		}
		else
		{
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "Refresher is empty string.");
		}
	}

	return(0);
} // END: doSessionExpiry()

int setSessionTimersResponse(int zCall, int zLine, char *zMod, osip_message_t *response)
{
	static char		mod[]="setSessionTimersResponse";
	int				sessExpires;
	char			c_sessExpires[64];
	int				timerSupported;
	string			pRefresher;
	char			cRefresher[32];
	char			sRefresher[32];
	
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO, 
				"%s called from [%s, %d]", mod, zMod, zLine);

	sessExpires = gCallInfo[zCall].GetIntOption("session-expires");
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, DYN_BASE, INFO, 
				"Retrieved session-expires of %d", sessExpires);

	timerSupported = gCallInfo[zCall].GetIntOption("uac-timer-supported");
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO,
			"Retrieved uac-timer-supported of %d.", timerSupported);

	if ( sessExpires > 0 )
	{
		if ( ! timerSupported )
		{
			sprintf(sRefresher, "%s", "refresher=uas");
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "uac is not timer supported; refresther=uas");
		}
		else
		{
			pRefresher = gCallInfo[zCall].GetStringOption("refresher");		//  none, uac, or uas
			
			sprintf(cRefresher, "%s", pRefresher.c_str());
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "Retrieved refresher of %s.", cRefresher);

			if ( strcmp(cRefresher, "none") == 0 )
			{
				sprintf(sRefresher, "%s", "refresher=uas");
			}
			else
			{
				if ( strcmp(cRefresher, "uac") == 0 )
				{
					sprintf(sRefresher, "%s", "refresher=uac");
					osip_message_set_header (response, (char *)"Require", "timer");
				}
				else
				{
					sprintf(sRefresher, "%s", "refresher=uas");
				}
			}
			dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "uas refresher set to (%s).", sRefresher);

		}

		sprintf(c_sessExpires, "%d;%s", sessExpires, sRefresher);
		osip_message_set_header (response, (char *)"Session-Expires", c_sessExpires);
	}
	gCall[zCall].sessionTimersSet = 1;
	dynVarLog (__LINE__, zCall, mod, REPORT_VERBOSE, TEL_BASE, INFO, "Set gCall[%d].sessionTimersSet to %d", 
					zCall, gCall[zCall].sessionTimersSet);

	return(0);
} // END: setSessionTimersResponse()
 /*EOF*/ 		// MR # 4982
