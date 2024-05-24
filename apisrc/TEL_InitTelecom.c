/*-----------------------------------------------------------------------------
Program:        TEL_InitTelecom.c 
Purpose:        To open the communication channel (fifo) to the dynamic 
		manager and to initialize all Telecom global varibles.
Author:         Wenzel-Joglekar
Date:		10/19/2000
Update: 04/04/01 APJ Fixed Log message when get_name_value fails. 
Update: 04/04/01 APJ Added Tables in GV_TelecomConfigFile.
Update: 04/25/01 APJ Removed playback control variable references.
Update: 05/08/01 APJ Set SIGTERM signal handler.
Update: 05/30/01 APJ Set 20 sec timeout for response from DM.
Update: 06/27/01 APJ Removed hardcoding of GV_GatekeeperIP.
Update: 07/12/01 APJ Setting GV_RingEventTime.
Update: 07/28/01 APJ Removed reading of IP-Model from .TEL.cfg file.
Update: 08/03/01 APJ Changed GV_GatekeeperIP -> GV_DefaultGatewayIP.
Update: 08/03/01 APJ Initialise GV_InsideNotifyCaller.
Update: 08/07/01 MPB Initialize Globals for LOGXXPRT.
Update: 08/08/01 APJ Initialize GV_BeepFile.
Update: 11/01/01 APJ Initialize GV_FirstSystemPhraseExt.
Update: 01/15/02 APJ Changed '-p'->'-z'; '-c'->'-p'. Added '-y'.
Update: 07/01/02 APJ Initialize GV_BandWidth.
Update: 08/19/02 APJ Made DefaultGateway not found message verbose.
Update: 12/08/03 APJ Get trunnk group and ignore it.
Update: 12/09/03 APJ Initialize GV_RecordTermChar.
Update: 03/25/04 APJ Ignore -d switch.
Update: 03/25/04 APJ Type invalid switch as %d.
Update: 05/21/04 APJ Initialize GV_Timeout and GV_Retry.
Update: 06/16/04 APJ Initialize GV_RecordTerminateKeys.
Update: 06/18/04 APJ Set CDR key.
Update: 09/15/04 APJ Initialize GV_IsCallDisconnected1.
Update: 03/12/06 DDN Open SDM channel before sending request,
						to avoid write error at SDM side.
Update: 09/20/06 DDN Reset variables in intializeGlobalVariables
							before calling parse_arg.
Update: 09/13/07 djb Added mrcpTTS logic.
------------------------------------------------------------------------------*/
#include "TEL_Common.h"
#include "ispinc.h"

//int gDynMgrId = -1;

extern void proc_sigterm(int signalNumber);
extern int LOGINIT(char *object, char *prefix, int msgbase, int maxdefs, char **msglist);
extern int LOGXXCTL(LOG_ReqParms_t *PT_logctl);

/*----------------------------------------------------------------------
This function sets the answer time in msec and converts it to  a string.
----------------------------------------------------------------------*/
int get_answer_time (char *current_time)
{
    struct tm *tm;
    struct timeval tp;
    struct timezone tzp;
    int ret_code;

/* gettimeofday() is a UNIX system call. */
    if ((ret_code = gettimeofday (&tp, &tzp)) == -1)
    {
        current_time == NULL;
        return 0;
    }

/* localtime() is a UNIX system call. */
    tm = localtime (&tp.tv_sec);

    sprintf (current_time, "%02d%02d%04d%02d%02d%02d%02d",
        tm->tm_mon + 1, tm->tm_mday, tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec, tp.tv_usec / 10000);
    return 0;
}                                                 /* get answer time */

int arg_switch(char *mod, int parameter, char *value)
{
	struct  tm  st;  /* time structure for maninpulating GV_RingEventTime */
	char    year_str[]="19yy";
	char    month_str[]="mm";
	char    day_str[]="dd";
	char    hour_str[]="hh";
	char    minute_str[]="mm";
	char    second_str[]="ss";

	GV_Overlay = 0;
	switch(parameter)
	{
		case 'O': /* Overlay for StartNewApplication*/
			sscanf(value, "%d", &GV_Overlay);
			break;

        case 'A': /* ANI */
                strcpy(GV_ANI, value);
		break;
        case 'p': /* App Call Number */
                sscanf(value, "%d", &GV_AppCallNum1);
		GV_shm_index=GV_AppCallNum1;
		break;
        case 'D': /* DNIS */
                strcpy(GV_DNIS, value);
		break;
        case 'z': /* password */
                sscanf(value, "%d", &GV_AppPassword);
                break;
        case 'P': /* program (application) name  ?? Not using this */
                strcpy(GV_AppName, value);
		break;
        case 'U':
                strcpy(GV_AppToAppInfo, value);
                break;
	case 'r':

		if(value[0] == '\0')
		{
			get_answer_time(GV_RingEventTime);
			time(&GV_RingEventTimeSec);
		}
		else if(strcmp(value, "N/A") != 0)
		{


			strcpy(GV_RingEventTime, value);

                	month_str[0]    =GV_RingEventTime[0];
                	month_str[1]    =GV_RingEventTime[1];
                	day_str[0]      =GV_RingEventTime[2];
                	day_str[1]      =GV_RingEventTime[3];
                	year_str[0]     =GV_RingEventTime[4];
                	year_str[1]     =GV_RingEventTime[5];
                	year_str[2]     =GV_RingEventTime[6];
                	year_str[3]     =GV_RingEventTime[7];
                	hour_str[0]     =GV_RingEventTime[8];
                	hour_str[1]     =GV_RingEventTime[9];
                	minute_str[0]   =GV_RingEventTime[10];
                	minute_str[1]   =GV_RingEventTime[11];
                	second_str[0]   =GV_RingEventTime[12];
                	second_str[1]   =GV_RingEventTime[13];

                	st.tm_year  = atoi(year_str) - 1900;
                	st.tm_mon   = atoi(month_str) - 1;
                	st.tm_mday  = atoi(day_str);
                	st.tm_hour  = atoi(hour_str);
                	st.tm_min   = atoi(minute_str);
                	st.tm_sec   = atoi(second_str);
                	st.tm_isdst = -1;

                	GV_RingEventTimeSec=mktime(&st);
						if (GV_RingEventTimeSec == (time_t)-1)
                        {
                        	time(&GV_RingEventTimeSec);
							get_answer_time(GV_RingEventTime);

							telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Warn,
                        		"Unable to set GV_RingEventTimeSec from "
                        		"parameter (%s). Using current time.",
                        		GV_RingEventTime);
                        }
		
		}
		break;
		case 'y':
			break;
		case 't': /* Trunk Group */
			/* Ignoring Trunk group for the moment. */
			break;
		case 'd': /* DynMgr Id */
			/* Ignoring dynamic manager Id. */
			GV_DynMgrId = atoi(value);
			break;
        case 'F': /* Responsibility FIFO */
                sprintf(GV_ResFifo, "%s", value);
            break;
        default:
                telVarLog(mod, REPORT_DETAIL, TEL_INVALID_PARM, GV_Warn,
                	"Unknown parameter: -%d.",parameter);
		break;
	}

	if(GV_RingEventTime[0] == '\0')
	{
			get_answer_time(GV_RingEventTime);
	}
	return(0);
}

/*------------------------------------------------------------------------------
Add Telecom messages to Log Manager list.
------------------------------------------------------------------------------*/
int log_tel_msgs()
{
LOG_ReqParms_t  logctl;
MsgSet_t        mymsg;
char            MyObject[]="TEL";
char            MyPrefix[]="TEL";

/* Load Telecom log messages */
logctl.CommandId=ISP_LOGCMD_LOADMSG;
logctl.PT_Object=MyObject;
logctl.PT_MsgSet=&mymsg;

/* object name should be limited to 3 char */
strcpy(mymsg.object, MyObject);
strcpy(mymsg.prefix, MyPrefix);

mymsg.msgbase=TEL_MSG_BASE;
mymsg.version=TEL_MSG_VERSION;
mymsg.max_defined=GV_TEL_msgnum;
mymsg.msglist=GV_TELmsg;

if(LOGXXCTL(&logctl) == ISP_FAIL)
        return (ISP_FAIL);

return (ISP_SUCCESS);
} /* log_tel_msgs() */

/*------------------------------------------------------------------------------
Pass application name, resource name to Log Manager
------------------------------------------------------------------------------*/
int log_app_name()
{
LOG_ReqParms_t  logctl;
char            LV_ProgramName[50];
char            LV_PortName[20];

strcpy(LV_ProgramName, GV_AppName);
LV_ProgramName[12] = '\0';

sprintf(LV_PortName, "%d", GV_AppCallNum1);
LV_PortName[8] = '\0';

strcat(LV_ProgramName, ".");
strcat(LV_ProgramName, LV_PortName);

/* Load Telecom log messages */
logctl.CommandId=ISP_LOGCMD_TAGMSG;
logctl.PT_UserData = LV_ProgramName;

if(LOGXXCTL(&logctl) == ISP_FAIL )
    return (ISP_FAIL);

return (ISP_SUCCESS);
} /* log app name() */

/*----------------------------------------------------------------------------
parse_arguments(): Parse command line args passed to the app.
I hate like hell to put this code in this API, but I don't have time to
rewrite it so that it is human readable. gpw 10/28/2000.
---------------------------------------------------------------------------*/
int parse_arguments(char *mod, int argc, char *argv[])
{
	int c;
	int i;

	while( --argc > 0 )
        {
        	*++argv;
        	if(argv[1][0] != '-')
                {
                	arg_switch(mod, (*argv)[1], argv[1]);
                	*++argv;
                	--argc;
                }
        	else
                	arg_switch(mod, (*argv)[1]," ");
        }
	return (1);
} 

/*-----------------------------------------------------------------------------
This functions initializes all the global variables used by Telecom Services.
Such variables should never be initialized anywhere but here. 
-----------------------------------------------------------------------------*/
int intializeGlobalVariables(char *mod, int argc, char *argv[])
{
	char *system_phrase_root; 
	char *ispbase;
	char value[MAX_LENGTH] = "";
	char heading[MAX_LENGTH] = "";
	int rc = 0;
	char	lPortBuf[4] = "";
	char	buf[256] = "";

	/* Before doing anything, get pid to avoid repeated calls to getpid */
	GV_AppPid=getpid();

	GV_InitCalled = 0;
	memset(GV_DialSequence,		'\0',sizeof(GV_DialSequence));
	memset(GV_UserToUserInfo,	'\0',sizeof(GV_UserToUserInfo));
	memset(GV_CallingParty,		'\0',sizeof(GV_CallingParty));
	GV_InitTime = 0L;
	GV_RingEventTimeSec = 0L;
	memset(GV_RingEventTime, 0, sizeof(GV_RingEventTime));
	GV_ConnectTimeSec = 0L;
	memset(GV_ConnectTime,		'\0',sizeof(GV_ConnectTime));
	GV_DisconnectTimeSec = 0L;
	memset(GV_DisconnectTime,	'\0',sizeof(GV_DisconnectTime));
	GV_ExitTime = 0L;
	memset(GV_PortName,		'\0',sizeof(GV_PortName));
	memset(GV_TEL_PortName,		'\0',sizeof(GV_TEL_PortName));
	memset(GV_DV_DIGIT_TYPE,	'\0',sizeof(GV_DV_DIGIT_TYPE));
	memset(GV_InBandDigits,		'\0',sizeof(GV_InBandDigits));
	// GV_AppPid is assigned above	
	memset(GV_TypeAheadBuffer1,	'\0',sizeof(GV_TypeAheadBuffer1));
	memset(GV_TypeAheadBuffer2,	'\0',sizeof(GV_TypeAheadBuffer2));

	// Initialize TTS globals
	sprintf(GV_TTS_Language, "%s", "en-US");
	sprintf(GV_TTS_Gender, "%s", "FEMALE");
	sprintf(GV_TTS_DataType, "%s", "STRING");
	sprintf(GV_TTS_Compression, "%s", "COMP_64PCM");
	memset(GV_TTS_VoiceName, 0, sizeof(GV_TTS_VoiceName));
    GV_MrcpTTS_Enabled = 0;         // disabled by default

	/* Set variables based on command line arguments. */	
	memset(GV_ANI,			'\0',sizeof(GV_ANI));
	memset(GV_DNIS,			'\0',sizeof(GV_DNIS));
	memset(GV_AppName,		'\0',sizeof(GV_AppName));
	sprintf(GV_AppName,"%s", argv[0]);
	memset(GV_BeepFile, 0, sizeof(GV_BeepFile));
	memset(GV_Audio_Codec,  '\0', sizeof(GV_Audio_Codec));
	memset(GV_Video_Codec,  '\0', sizeof(GV_Video_Codec));

	/*Initialize fax variables*/
	GV_SendFaxTone = 0;
	strcpy(value, "SendFaxTone");
    memset(buf, '\0', sizeof(buf));
	rc = get_name_value("", value, "0", buf, sizeof(buf), GV_TelecomConfigFile, Msg);
	if(buf && buf[0])
	{
		GV_SendFaxTone = atoi(buf);
	}

	/* Set service name and object name. */
	sprintf(GV_ServType, "%s", "TEL");
	sprintf(GV_GlobalAttr.ObjectName, "%s", "TEL");

	GV_BLegCDR = 0;

	GV_Timeout = 0;
	GV_Retry = 0;
	GV_InsideNotifyCaller = 0;
	GV_AppRef = 1;
	GV_shm_index = -1;
	GV_AppCallNum1 = NULL_APP_CALL_NUM;
	GV_AppCallNum2 = NULL_APP_CALL_NUM;
	GV_AppCallNum3 = NULL_APP_CALL_NUM;
	GV_AppPassword = -99;
	memset(GV_AppToAppInfo,		'\0',sizeof(GV_AppToAppInfo));
	memset(GV_RecordTerminateKeys,	'\0', sizeof(GV_RecordTerminateKeys));
	sprintf(GV_ResFifo, "%s/ResFifo", GV_FifoDir);
	parse_arguments(mod, argc, argv);

	sprintf(lPortBuf, "%d", GV_AppCallNum1);
	if((rc = setCDRKey(mod, lPortBuf)) == -1)
		return(-1);

	GV_DiAgOn_ = 0;
  	sprintf (GV_CDR_Key, "%s", GV_CDRKey);

	memset(Msg, '\0', sizeof(Msg));

	/* Values for message type for telVarLog to log ERR:, WRN:, INF: */	
	GV_Err=0; GV_Warn=1; GV_Info=2;
	/* Determine where phrases are */	
	system_phrase_root= (char *)getenv("OBJDB");
	if(system_phrase_root == NULL)
	{
        	telVarLog(mod, REPORT_NORMAL, TEL_OBJDB_NOT_SET, GV_Err,
       			"Environment variable is $OBJDB not set.");
        	return(-1);
	}
	
	/* Determine base pathname. */	
	memset(GV_ISPBASE,'\0',sizeof(GV_ISPBASE));
	ispbase= (char *)getenv("ISPBASE");
	if(ispbase == NULL)
        {
		/* ?? Message # appears to be incorrect */
        	telVarLog(mod, REPORT_NORMAL, TEL_OBJDB_NOT_SET, GV_Err, 
       			"Environment variable is $ISPBASE not set.");
        	return(-1);
        }
	sprintf(GV_ISPBASE, "%s", ispbase);
	sprintf(GV_GlobalConfigFile, "%s/Global/.Global.cfg", ispbase);
	sprintf(GV_TelecomConfigFile, "%s/Telecom/Tables/.TEL.cfg", ispbase);

	strcpy(value, "FIFO_DIR");
	rc = get_name_value("", value, "/tmp", 
		GV_FifoDir, sizeof(GV_FifoDir), GV_GlobalConfigFile, Msg);
	if (rc != 0) 
	{
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Warn,
			"Failed to find '%s' in (%s). Using (%s).", 
			value, GV_GlobalConfigFile, GV_FifoDir);
	}

	/*MR#3917 Save SIP_FROM */
	strcpy(value, "SIP_From");
	memset(buf, '\0', sizeof(buf));
	rc = get_name_value("", value, "", buf, sizeof(buf), GV_TelecomConfigFile, Msg);
	if(buf && buf[0])
	{
		telVarLog (mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Found '%s=%s' in (%s).",
			value, buf, GV_TelecomConfigFile);
		sprintf(GV_SipFrom, "%s", buf);
	}
	else
	{
		telVarLog (mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
			"Failed to find '%s' in (%s).",
			value, GV_TelecomConfigFile);
	}

	memset(GV_DefaultGatewayIP, '\0', sizeof(GV_DefaultGatewayIP));
	strcpy(value, "DefaultGateway");
	sprintf(heading, "%s", "IP");
	rc = get_name_value(heading, value, "", 
		GV_DefaultGatewayIP, sizeof(GV_DefaultGatewayIP), 
					GV_TelecomConfigFile, Msg);
	if (rc != 0) 
	{
		telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
		"No '%s' found under (%s) in (%s). Using (%s).", 
		value, heading, GV_TelecomConfigFile, GV_DefaultGatewayIP);
	}

    sprintf(heading, "%s", "Settings");
    sprintf(value, "%s", "TTS_MRCP");
    memset(buf, '\0', sizeof(buf));
    memset(Msg, '\0', sizeof(Msg));
    rc = get_name_value(heading, "TTS_MRCP", "OFF",
        buf, sizeof(buf), GV_TelecomConfigFile, Msg);
    if (rc != 0)
    {
        telVarLog(mod, REPORT_DETAIL, TEL_BASE, GV_Info,
        "No '%s' found under (%s) in (%s). Using (%s).",
        value, heading, GV_TelecomConfigFile, buf);
    }
    GV_MrcpTTS_Enabled = 0;
    if ( (strcmp(buf, "ON") == 0) ||
         (strcmp(buf, "On") == 0) ||
         (strcmp(buf, "on") == 0) )
    {
        GV_MrcpTTS_Enabled = 1;
    }
    if ( rc != 0 )
    {
        telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
            "MRCP TTS Enabled=%d", GV_MrcpTTS_Enabled);
    }

	GV_TerminatedViaExitTelecom=0;
	memset(GV_CustData1, 	'\0', sizeof(GV_CustData1));
	memset(GV_CustData2, 	'\0', sizeof(GV_CustData2));
	sprintf(GV_ResponseFifo, "%s/ResponseFifo.%d", GV_FifoDir, GV_AppPid);
	GV_ResponseFifoFd = 0;
	GV_Connected1 = 0;
	GV_Connected2 = 0;
	GV_IsCallDisconnected1 = 0;
	GV_NotifyCallerInterval = 400; 

	if(GV_DynMgrId < 0)
	{
		sprintf(GV_RequestFifo, "%s/RequestFifo", GV_FifoDir);
	}
	else
	{
		sprintf(GV_RequestFifo, "%s/RequestFifo.%d", GV_FifoDir, GV_DynMgrId);
	}

	GV_RequestFifoFd = 0;
	GV_ResFifoFd = 0;
	
	memset(GV_AppPhraseDir, 	'\0', sizeof(GV_AppPhraseDir));
	memset(GV_SystemPhraseRoot, 	'\0', sizeof(GV_SystemPhraseRoot));
	memset(GV_Language,		'\0',sizeof(GV_Language));
	memset(GV_SystemPhraseDir,	'\0', sizeof(GV_SystemPhraseDir));
	memset(GV_FirstSystemPhraseExt,	'\0', sizeof(GV_FirstSystemPhraseExt));
	GV_BandWidth = 0;
	GV_RecordTermChar = (int)' ';

    sprintf(value, "%s", "ENABLE_CDRS");
    buf[0] = '\0';
    rc = get_name_value("", value, "",
        buf, sizeof(buf), GV_GlobalConfigFile, Msg);

    if (rc != 0)
    {
        telVarLog(mod, REPORT_DETAIL, TEL_BASE, GV_Warn,
            "Failed to find '%s' in (%s).", value, GV_GlobalConfigFile);
    }

    if(strstr(buf, "B_LEG_CDR") != NULL)
	{
		GV_BLegCDR = 1;
	}

	strcpy(value, "DefaultLanguage");
	rc = get_name_value("", value, "AMENG", 
		GV_Language, sizeof(GV_Language), GV_TelecomConfigFile, Msg);
	if (rc != 0)
	{
		 telVarLog(mod, REPORT_DETAIL, TEL_BASE, GV_Warn,
			"Failed to find '%s' in (%s). Using (%s).", 
			value,	GV_TelecomConfigFile, GV_Language);
	}

	if (strcmp(GV_Language, "NONE") == 0)
	{
		sprintf(GV_Language,"%s", "AMENG");		
		GV_LanguageNumber = S_AMENG;
	}
	else if (strcmp(GV_Language, "AMENG") == 0)
	       GV_LanguageNumber = S_AMENG;
	else if (strcmp(GV_Language, "FRENCH") == 0)
		GV_LanguageNumber = S_FRENCH;
	else if (strcmp(GV_Language, "GERMAN") == 0)
		GV_LanguageNumber = S_GERMAN;
	else if (strcmp(GV_Language, "DUTCH") == 0)
		GV_LanguageNumber = S_DUTCH;
	else if (strcmp(GV_Language, "SPANISH") == 0)
		GV_LanguageNumber = S_SPANISH;
	else if (strcmp(GV_Language, "FLEMISH") == 0)
		GV_LanguageNumber = S_FLEMISH;
	else if (strcmp(GV_Language, "BRITISH") == 0)
		GV_LanguageNumber = S_FLEMISH;
	else if (strncmp(GV_Language, "LANG", 4) == 0)
		GV_LanguageNumber = atoi(&GV_Language[4]);        
	{	
		GV_LanguageNumber=S_AMENG;
		sprintf(GV_Language, "%s", "AMENG");
	}
	
	sprintf(GV_SystemPhraseRoot, "%s", system_phrase_root);
	sprintf(GV_SystemPhraseDir, "%s/%s", 	GV_SystemPhraseRoot, 
						GV_Language);	

	get_first_phrase_extension(mod, GV_SystemPhraseDir, 
		GV_FirstSystemPhraseExt);	

	GV_TotalSRRequests = 0;
	GV_TotalAIRequests = 1;

	GV_SRType = MS_SR;
	GV_GoogleRecord = NO;

	return(0);
}
int TEL_InitTelecom(int argc, char **argv) 
{
	static char mod[]="TEL_InitTelecom"; 
	char apiAndParms[MAX_LENGTH] = "";
	char apiAndParmsFormat[]="%s(%d,argv)";
	int  rc = 0;
	struct MsgToApp response;
	struct Msg_InitTelecom lMsg;
	struct MsgToDM msg;
	char bogus[2] = "";
	char *p;

	GV_DynMgrId = -1;

	signal(SIGTERM, proc_sigterm);
		
	/* Initialize all Telecom Services global variables */
	rc=intializeGlobalVariables(mod, argc, argv);
	if (rc != 0) HANDLE_RETURN(rc); /* Message written in subroutine */

	/* Register with the ISP log manager */
#if 1 
	p=&(bogus[0]);
	rc = LOGINIT("TEL", "TEL", 0, 0, &p);
	if(rc < 0)
	{
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err,
        	"Can't register with log manager (LOGINIT failed). rc=%d.", rc);
        	HANDLE_RETURN(-1);
	}

#endif  

	/* Add Telecom messages to ISP log manager. */
	if ((rc = log_tel_msgs()) != ISP_SUCCESS)
	{
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err,
        	"Unable to add Telecom messages to Log Manager list.");
        	HANDLE_RETURN(-1);
	}

	/* pass application and resource names to log manager */
	if (( rc = log_app_name()) != ISP_SUCCESS)
	{
		telVarLog(mod, REPORT_NORMAL, TEL_BASE, GV_Err,
        	"Unable to set app, resource name in Log Manager.");
        	HANDLE_RETURN(-1);
	}        

	sprintf(apiAndParms,apiAndParmsFormat, mod, argc);
	rc = apiInit(mod, TEL_INITTELECOM, apiAndParms, 0, 0, 0);
 	if (rc != 0) HANDLE_RETURN(rc);


	lMsg.opcode 	= DMOP_INITTELECOM;
	lMsg.appPid 	= GV_AppPid;
	lMsg.appPassword= GV_AppPassword;
	lMsg.appCallNum = GV_AppCallNum1;
	lMsg.appRef 	= GV_AppRef++;
	
	strcpy(lMsg.responseFifo,GV_ResponseFifo);
//	sprintf(lMsg.ringEventTime, "%s", GV_RingEventTime);

	rc=openChannelToDynMgr(mod);
	if (rc == -1) HANDLE_RETURN(TEL_FAILURE);

	memset(&msg, 0, sizeof(struct MsgToDM));
	memcpy(&msg, &lMsg, sizeof(struct Msg_InitTelecom));


	rc = readResponseFromDynMgr(mod, -1, &response,sizeof(response));

#if 1
	if(GV_Overlay == 1)
	{
		GV_InitCalled = 1;
		writeFieldToSharedMemory(mod, GV_AppCallNum1, APPL_PID, GV_AppPid);
		telVarLog(mod, REPORT_VERBOSE, TEL_BASE, GV_Info,
			"GV_Overlay is set returning SUCCESS from  TEL_InitTelecom");
		HANDLE_RETURN(TEL_SUCCESS);
	}
#endif
	rc = sendRequestToDynMgr(mod, &msg);
	if (rc == -1) HANDLE_RETURN(TEL_FAILURE);

	while ( 1 )
	{
		rc = readResponseFromDynMgr(mod,20,&response,sizeof(response));
		if (rc == TEL_TIMEOUT)
		{
			telVarLog(mod,REPORT_NORMAL, TEL_BASE, GV_Err,
			"Timeout waiting for response from Dynamic Manager.");
			HANDLE_RETURN(TEL_TIMEOUT);
		}
		if(rc == -1) HANDLE_RETURN(TEL_FAILURE);
	
		rc = examineDynMgrResponse(mod, &msg, &response);	
		switch(rc)
		{
		case DMOP_INITTELECOM:
			if (response.returnCode == 0)
			{
				/* ?? already have this set from switch */
				GV_AppCallNum1 = response.appCallNum; 
				GV_AppPassword = response.appPassword; 
				GV_InitCalled = 1;
			}
			HANDLE_RETURN(response.returnCode);
			break;
		case DMOP_GETDTMF:
			/* All DTMF characters are ignored. */
			break;
		case DMOP_DISCONNECT:
                        HANDLE_RETURN(disc(mod, response.appCallNum));
			break;
		default:
			/* Unexpected message. Logged in examineDynMgr... */
			continue;
			break;
		} /* switch rc */
	} /* while */
}

