#ifndef TEL_GLOBALS_DOT_H
#define TEL_GLOBALS_DOT_H

/*-----------------------------------------------------------------------------
Program:        TEL_Globals.h
Purpose:        Defines all the global variables used by Telecom Services.
		This file should be included in two places and only two places:
		1) In TEL_Common.c, with #define EX "", to define the variables
		2) In TEL_Common.h, with #define EX "extern" to extern them.
Author:         Wenzel-Joglekar
Date:		09/13/2000 
Update:		10/03/2000 gpw added SystemPhraseDir, SystemPhraseRoot
Update:		10/09/2000 gpw added GV_AppPassword
Update:		10/20/2000 gpw added GV_GatekeeperIP
Update:		10/26/2000 gpw added GV_KillAppGracePeriod
Update:		03/08/2001 gpw added GV_ISPBASE, set in TEL_InitTelecom
Update:		03/08/2001 gpw added "extern" b4 GV_TerminatedViaExitTelecom
Update:		03/13/2001 apj Changed APP_TO_APP_INFO_LENGTH to 512.
Update:		04/25/2001 apj Removed variables related to playback control.
Update:		05/16/2001 apj Added GV_AgentDisct.
Update:		07/12/2001 apj Added GV_RingEventTime.
Update: 07/16/01 apj Added GV_TagsLoaded, GV_AppTags, GV_PncInput, GV_TagFiles
Update: 07/17/01 apj Added GV_DontSendToTelMonitor.
Update: 08/03/01 apj Changed GV_GatekeeperIP -> GV_DefaultGatewayIP.
Update: 08/03/01 apj Removed GV_IPModel.
Update: 08/06/01 apj Added GV_BridgeResponse.
Update: 08/06/01 apj Added GV_InsideNotifyCaller.
Update: 08/08/01 apj Added GV_BeepFile.
Update: 11/01/01 apj Added GV_FirstSystemPhraseExt.
Update: 01/02/02 apj Added SIR_cleanup.
Update: 07/01/02 apj Added GV_BandWidth.
Update: 12/09/03 apj Added GV_RecordTermChar.
Update: 05/21/04 apj Added GV_Timeout and GV_Retry.
Update: 06/16/04 apj Added GV_RecordTerminateKeys.
Update: 09/15/04 apj Added GV_IsCallDisconnected1.
Update: 10/21/04 mpb Changed MAX_CDR_KEY from 38 to 60.
Update: 04/26/05 ddn Changed GV_LastStreamError
Update: 09/12/07 djb Added GV_MrcpTTS_Enabled.
Update: 01/22/08 jms Changed Logoff() and NotifyCaller to function pointers for shared object compilation 
Update: 10/06/14 jms changes to support 64 bit compilation (mostly sourcing, and pathing, and include files) 
Update: 09/15/15 djb Added GV_HideDTMF / $HIDE_DTMF
------------------------------------------------------------------------------*/

#define	VERSION			"1.0"
#define MAX_LOG_MSG_LENGTH 	512+1	
#define MAX_DIR_NAME_LENGTH	128+1	
#define MAX_PATHNAME_LENGTH	256+1	
#define MAX_DTMF_KEY_SEQUENCE_LENGTH	64	
#define MAX_DIAL_SEQUENCE_LENGTH	64	
#define MAX_USER_TO_USER_INFO_LENGTH	64	
#define MAX_APP_TO_APP_INFO_LENGTH	512	
#define MAX_CDR_KEY		60+1
#define MAX_LENGTH   1024

// extern char GV_CDRKey[];
// extern int GV_HideData;


#ifdef GLOBAL

#include  "ispinc.h"

#ifdef NETWORK_TRANSFER
char GV_NT_ResponseFifo[MAX_LENGTH] = "";
int  GV_NT_ResponseFifoFd = -1;
#endif

#ifndef CLIENT_GLOBAL_CROSSING
// void (*LogOff)() = NULL;
void LogOff()  { return; }	 // MR # 4814
int (*NotifyCaller)(int) = NULL;
#else
#warning "GLOBAL CROSSING ONLY"
#endif

GV_GlobalAttr_type GV_GlobalAttr;
char GV_IndexDelimit;
int GV_HideData;
int GV_HideDTMF;
int GV_AttchaedSRClient;
int GV_TotalSRRequests;
int GV_TotalAIRequests;
int GV_BLegCDR;
int GV_ReleaseCauseCode;
char GV_ISPBASE[MAX_DIR_NAME_LENGTH];
int GV_TerminatedViaExitTelecom;
char GV_CustData1[9] = "";
char GV_CustData2[101] = "";
char GV_CDRKey[MAX_CDR_KEY] = "";
char GV_CDR_Key[50] = "";
int  GV_AppRef;
int  GV_shm_index;
int  GV_DiAgOn_;
struct Msg_Speak GV_LastSpeakMsg1;
struct Msg_Speak GV_LastSpeakMsg2;
char Msg[MAX_LOG_MSG_LENGTH];
int GV_Err; int GV_Warn; int GV_Info;
char GV_GlobalConfigFile[MAX_PATHNAME_LENGTH] = "";
char GV_TelecomConfigFile[MAX_PATHNAME_LENGTH] = "";
char GV_FifoDir[MAX_PATHNAME_LENGTH] = "";
char GV_DefaultGatewayIP[MAX_DIAL_SEQUENCE_LENGTH] = "";
int  GV_Connected1;
int  GV_Connected2;
int  GV_IsCallDisconnected1;
int  GV_KillAppGracePeriod;
int  GV_NotifyCallerInterval;
char GV_RequestFifo[MAX_PATHNAME_LENGTH] = "";
int  GV_RequestFifoFd;
char GV_ResponseFifo[MAX_PATHNAME_LENGTH] = "";
int  GV_ResponseFifoFd;
char GV_ResFifo[MAX_PATHNAME_LENGTH] = "";
int  GV_ResFifoFd;
int  GV_AppCallNum1;
int  GV_AppCallNum2;
int  GV_AppCallNum3;
int  GV_AppPassword;
char GV_SystemPhraseRoot[MAX_DIR_NAME_LENGTH] = "";
char GV_SystemPhraseDir[MAX_DIR_NAME_LENGTH] = "";
char GV_FirstSystemPhraseExt[10] = "";
char GV_AppPhraseDir[MAX_DIR_NAME_LENGTH] = "";
char GV_Language[MAX_LENGTH] = "";
int  GV_LanguageNumber;
char GV_ServType[4] = "";
int  GV_TEL_msgnum;
int  GV_InitCalled;
char GV_DialSequence[MAX_DIAL_SEQUENCE_LENGTH] = "";
char GV_UserToUserInfo[MAX_USER_TO_USER_INFO_LENGTH] = "";
char GV_CallingParty[MAX_DIAL_SEQUENCE_LENGTH] = "";
char GV_AppToAppInfo[MAX_APP_TO_APP_INFO_LENGTH] = "";
long GV_InitTime;
long GV_RingEventTimeSec;
long GV_ConnectTimeSec;
char GV_ConnectTime[32] = "";
long GV_DisconnectTimeSec;
char GV_DisconnectTime[32] = "";
long GV_ExitTime;
char GV_ANI[MAX_LENGTH] = "";
char GV_DNIS[MAX_LENGTH] = "";
char GV_PortName[MAX_LENGTH] = "";
char GV_TEL_PortName[MAX_LENGTH] = "";
char GV_DV_DIGIT_TYPE[MAX_LENGTH] = "";
char GV_InBandDigits[MAX_LENGTH] = "";
int  GV_AppPid;
char GV_AppName[MAX_LENGTH] = "";
char LAST_API[MAX_LENGTH] = "";
char GV_TypeAheadBuffer1[MAX_LENGTH] = "";
char GV_TypeAheadBuffer2[MAX_LENGTH] = "";
char GV_RingEventTime[17] = "";
int GV_AgentDisct = NO;

int (*SERV_M3)() = NULL;
int (*SERV_ALL)(int) = NULL;

struct MsgToApp GV_BridgeResponse;
int GV_InsideNotifyCaller;
char GV_BeepFile[256] = "";
int GV_BandWidth;
int GV_RecordTermChar;
char GV_TrunkGroup[100] = "";
int GV_Timeout;
int GV_Retry;
int GV_ListenCallActive = 0;

/* bridge connect and disconnect time */
int GV_BridgeConnectTimeSec = 0;
char GV_BridgeConnectTime[48] = "";
long GV_BridgeDisconnectTimeSec = 0;
char GV_BridgeDisconnectTime[48] = "";



/*Streaming Related Variables*/
int GV_LastStreamError;

/* TTS related variables. */
char GV_TTS_Language[20] = "";
char GV_TTS_Gender[20] = "";
char GV_TTS_DataType[20] = "";
char GV_TTS_Compression[20] = "";
char GV_TTS_VoiceName[20] = "";
int  GV_MrcpTTS_Enabled;

/* TEL_PromptAndCollect related variables */
int             GV_TagsLoaded;
PhraseTagList   GV_AppTags;
PncInput        GV_PncInput;
TagFileList     GV_TagFiles[MAX_TAG_FILES]; 
int     GV_DontSendToTelMonitor=0;/* If 1, API sets=0 & no send to monitor*/

int	(*SIR_cleanup)();

char GV_RecordTerminateKeys[20] = "";
int GV_DynMgrId;
int GV_SendFaxTone;
int GV_Overlay;
char GV_Audio_Codec[32] = "";
char GV_Video_Codec[32] = "";

int	GV_LastFaxError;
char GV_SipFrom[MAX_LENGTH] = "";
int isConferenceOn;
char confId[64];
char GV_ConfData[64];

int GV_SRType;
int GV_GoogleRecord;
int GV_OverrideInbandDTMF;

#else


#ifndef CLIENT_GLOBAL_CROSSING
extern void (*LogOff)();
extern int (*NotifyCaller)(int);
#endif

extern int GV_HideData;
extern int GV_HideDTMF;
extern int GV_AttchaedSRClient;
extern int GV_TotalSRRequests;
extern int GV_TotalAIRequests;
extern int GV_BLegCDR;
extern int GV_ReleaseCauseCode;
extern char GV_ISPBASE[MAX_DIR_NAME_LENGTH];
extern int GV_TerminatedViaExitTelecom;
extern char GV_CustData1[9];
extern char GV_CustData2[101];
extern char GV_CDRKey[MAX_CDR_KEY];
extern char GV_CDR_Key[50];
extern int  GV_AppRef;
extern int  GV_shm_index;
extern int  GV_DiAgOn_;
extern struct Msg_Speak GV_LastSpeakMsg1;
extern struct Msg_Speak GV_LastSpeakMsg2;
extern char Msg[MAX_LOG_MSG_LENGTH];
extern int GV_Err; extern int GV_Warn; extern int GV_Info;
extern char GV_GlobalConfigFile[MAX_PATHNAME_LENGTH];
extern char GV_TelecomConfigFile[MAX_PATHNAME_LENGTH];
extern char GV_FifoDir[MAX_PATHNAME_LENGTH];
extern char GV_DefaultGatewayIP[MAX_DIAL_SEQUENCE_LENGTH];
extern int  GV_Connected1;
extern int  GV_Connected2;
extern int  GV_IsCallDisconnected1;
extern int  GV_KillAppGracePeriod;
extern int  GV_NotifyCallerInterval;
extern char GV_RequestFifo[MAX_PATHNAME_LENGTH];
extern int  GV_RequestFifoFd;
extern char GV_ResponseFifo[MAX_PATHNAME_LENGTH];
extern int  GV_ResponseFifoFd;
extern char GV_ResFifo[MAX_PATHNAME_LENGTH];
extern int  GV_ResFifoFd;
extern int  GV_AppCallNum1;
extern int  GV_AppCallNum2;
extern int  GV_AppCallNum3;
extern int  GV_AppPassword;
extern char GV_SystemPhraseRoot[MAX_DIR_NAME_LENGTH];
extern char GV_SystemPhraseDir[MAX_DIR_NAME_LENGTH];
extern char GV_FirstSystemPhraseExt[10];
extern char GV_AppPhraseDir[MAX_DIR_NAME_LENGTH];
extern char GV_Language[MAX_PATHNAME_LENGTH];
extern int  GV_LanguageNumber;
extern int GV_InitCalled;
extern char GV_DialSequence[MAX_DIAL_SEQUENCE_LENGTH];
extern char GV_UserToUserInfo[MAX_USER_TO_USER_INFO_LENGTH];
extern char GV_CallingParty[MAX_DIAL_SEQUENCE_LENGTH];
extern char GV_AppToAppInfo[MAX_APP_TO_APP_INFO_LENGTH];
extern long GV_InitTime;
extern long GV_RingEventTimeSec;
extern char GV_RingEventTime[17];
extern long GV_ConnectTimeSec;
extern char GV_ConnectTime[32];
extern long GV_DisconnectTimeSec;
extern char GV_DisconnectTime[32];
extern long GV_ExitTime;
extern char GV_ANI[MAX_LENGTH];
extern char GV_DNIS[MAX_LENGTH];
extern char GV_PortName[MAX_LENGTH];
extern char GV_TEL_PortName[MAX_LENGTH];
extern char GV_DV_DIGIT_TYPE[MAX_LENGTH];
extern char GV_InBandDigits[MAX_LENGTH];
extern int  GV_AppPid;
extern char GV_AppName[MAX_LENGTH];
extern char LAST_API[MAX_LENGTH];
extern char GV_TypeAheadBuffer[MAX_LENGTH];
extern char GV_TypeAheadBuffer1[MAX_LENGTH];
extern char GV_TypeAheadBuffer2[MAX_LENGTH];
extern int GV_AgentDisct;
extern int GV_ListenCallActive;

extern int (*SERV_M3)();
extern int (*SERV_ALL)(int);

extern int             GV_TagsLoaded;
extern struct PhraseTagList   GV_AppTags;
extern struct PncInput        GV_PncInput;
extern TagFileList     GV_TagFiles[MAX_TAG_FILES]; 
extern int      GV_DontSendToTelMonitor;/* To control not sending to mon */ 
extern struct MsgToApp GV_BridgeResponse;
extern int GV_InsideNotifyCaller;
extern char GV_BeepFile[256];
extern int GV_BandWidth;
extern int GV_RecordTermChar;
extern char GV_TrunkGroup[100];
extern int GV_Timeout;
extern int GV_Retry;

extern int GV_LastStreamError;
/* TTS related variables. */
extern char GV_TTS_Language[20];
extern char GV_TTS_Gender[20];
extern char GV_TTS_DataType[20];
extern char GV_TTS_Compression[20];
extern char GV_TTS_VoiceName[20];
extern int  GV_MrcpTTS_Enabled;

extern char GV_RecordTerminateKeys[20];


extern int GV_DynMgrId;
extern int GV_SendFaxTone;
extern int GV_Overlay;
extern char GV_Audio_Codec[32];
extern char GV_Video_Codec[32];
extern int	GV_LastFaxError;

extern int writeFieldToSharedMemory (char *mod, int slot, int type, int value);
extern char GV_SipFrom[MAX_LENGTH];
extern int isConferenceOn;
extern char confId[64];
extern char GV_ConfData[64];

extern int GV_SRType;
extern int GV_GoogleRecord;
extern int GV_OverrideInbandDTMF;               // BT-313

/* bridge connect and disconnect time */
extern int GV_BridgeConnectTimeSec;
extern char GV_BridgeConnectTime[48];
extern long GV_BridgeDisconnectTimeSec;
extern char GV_BridgeDisconnectTime[48];
/* end bridge connect vars */


#ifdef NETWORK_TRANSFER
extern char GV_NT_ResponseFifo[MAX_LENGTH];
extern int  GV_NT_ResponseFifoFd;
#endif

#endif

#endif


